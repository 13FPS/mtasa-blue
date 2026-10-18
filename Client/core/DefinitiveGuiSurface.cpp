/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compose the existing GUI through a queue/window owned entirely by MTA
 *****************************************************************************/
#include "DefinitiveGuiSurface.h"

#if defined(_WIN64) && defined(MTA_WITH_DEFINITIVE_ADAPTER)
    // The classic workspace keeps its historical Win7 declarations. Only this
    // inactive-in-classic DE surface needs the Windows 8.1 composition interfaces.
    #if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0603
        #undef _WIN32_WINNT
    #endif
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0603
    #endif
    #include <windows.h>
    #include <d3d12.h>
    #include <dxgi1_4.h>
    #include <dcomp.h>
    #include <wrl/client.h>
    #include <algorithm>
    #include <array>
    #include <cstring>
    #include <filesystem>
    #include <new>
    #include <SharedUtil.Defines.h>
    #include <SString.h>
    #include <gui/CGUI.h>
    #include <gui/CGUID3D12.h>
    #include "GuiWin32Input.h"

namespace DefinitiveGuiSurface
{
    using Microsoft::WRL::ComPtr;
    using mta::gui::GuiD3D12Status;
    namespace
    {
        bool ReleaseUnknown(IUnknown* value) noexcept
        {
            __try
            {
                value->Release();
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
        template <typename T>
        bool ReleaseOwned(ComPtr<T>& stored) noexcept
        {
            auto* value = stored.Detach();
            return !value || ReleaseUnknown(value);
        }
        bool ReleaseSession(mta::gui::IGuiD3D12Session*& stored, GuiD3D12Status& status) noexcept
        {
            auto* value = stored;
            stored = nullptr;
            __try
            {
                status = value->Release();
                if (status != GuiD3D12Status::Ok)
                    stored = value;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
        void Barrier(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) noexcept
        {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            list->ResourceBarrier(1, &barrier);
        }
    }

    struct Session::Impl
    {
        DWORD                              owner = GetCurrentThreadId(), parentThread = 0;
        Result                             result;
        InteractionResult                  interaction;
        GuiWin32Input::Translator          input;
        bool                               inputQuiescing = false;
        FrontendCallbacks                  frontend;
        bool                               frontendEnabled = false, frontendStartAttempted = false, frontendCallbackActive = false;
        bool                               frontendInputDeferred = false;
        POINT                              frontendMouse{};
        HMODULE                            module = nullptr, windowModule = nullptr;
        wchar_t                            windowClass[96]{};
        bool                               classRegistered = false, comInitialized = false, prepared = false, started = false, failed = false;
        bool                               submitted = false, signalPending = false, gpuComplete = true, presentPending = false;
        ULONGLONG                          nextFrameAt = 0;
        HWND                               parent = nullptr, child = nullptr;
        ID3D12Device*                      device = nullptr;
        mta::gui::CreateGuiD3D12Session    create = nullptr;
        mta::gui::IGuiD3D12Session*        session = nullptr;
        CGUI*                              gui = nullptr;
        CGUITexture*                       texture = nullptr;
        CGUIStaticImage*                   image = nullptr;
        CGUIEdit*                          edit = nullptr;
        CGUILabel*                         interactionLabel = nullptr;
        ComPtr<ID3D12CommandQueue>         queue;
        ComPtr<ID3D12Fence>                fence;
        ComPtr<ID3D12CommandAllocator>     clearAllocator, copyAllocator;
        ComPtr<ID3D12GraphicsCommandList>  clearList, copyList;
        ComPtr<ID3D12DescriptorHeap>       rtv;
        ComPtr<ID3D12Resource>             buffer, readback;
        ComPtr<IDXGISwapChain3>            swapchain;
        ComPtr<IDXGIFactory2>              factory;
        ComPtr<IDXGISwapChain1>            initialChain;
        ComPtr<IDCompositionDesktopDevice> composition;
        ComPtr<IDCompositionTarget>        compositionTarget;
        ComPtr<IDCompositionVisual2>       visual;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT                               captureWidth = 0, captureHeight = 0, redX = 0, redY = 0;

        Impl() noexcept { result.ownerThreadId = owner; }
        struct FrontendCall
        {
            bool& active;
            explicit FrontendCall(bool& value) noexcept : active(value) { active = true; }
            ~FrontendCall() { active = false; }
        };
        void FocusFrontendOnStart() noexcept
        {
            // F9 and the caller's native-control acknowledgement explicitly
            // admit this frontend. Give its own child the first keyboard input
            // without requiring a mouse click, but never activate a background
            // game or reclaim focus after the user has left the frontend.
            if (!frontendEnabled || frontendInputDeferred || !interaction.enabled || !child || !IsWindowVisible(child) || !IsWindowVisible(parent) ||
                !IsWindowEnabled(parent) || IsIconic(parent) || GetForegroundWindow() != parent ||
                GetWindowThreadProcessId(child, nullptr) != GetCurrentThreadId())
                return;
            GUITHREADINFO parentInput{};
            parentInput.cbSize = sizeof(parentInput);
            if (!GetGUIThreadInfo(parentThread, &parentInput) || parentInput.hwndActive != parent || GetForegroundWindow() != parent)
                return;
            // A denied transfer keeps the existing click-to-focus path. This
            // is deliberately a single startup attempt, never a Poll retry.
            SetFocus(child);
        }
        bool PulseFrontend()
        {
            // Match the translator's client coordinates rather than sampling
            // a different desktop cursor between a mouse-down and mouse-up.
            FrontendCall call(frontendCallbackActive);
            return frontend.pulse(frontend.context, frontendMouse.x, frontendMouse.y, GetFocus() == child);
        }
        bool FrontendMessage(UINT message, WPARAM wparam, LPARAM lparam, bool& handled)
        {
            // The frontend may add its existing console shortcuts without
            // borrowing global GUI callbacks. Never enter a partial factory,
            // a retiring owner or a callback nested through a window message.
            handled = false;
            if (!frontendEnabled || !frontend.message || !started || frontendCallbackActive)
                return true;
            // Only input messages belong to the optional shortcut policy.
            // Window destruction, painting and other lifetime bookkeeping
            // must always reach this surface's own window procedure.
            switch (message)
            {
                case WM_SETFOCUS:
                case WM_KILLFOCUS:
                case WM_CANCELMODE:
                case WM_CAPTURECHANGED:
                case WM_KEYDOWN:
                case WM_KEYUP:
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_CHAR:
                case WM_UNICHAR:
                    break;
                default:
                    return true;
            }
            FrontendCall call(frontendCallbackActive);
            if (frontend.message(frontend.context, message, wparam, lparam, &handled))
                return true;
            interaction.inputFailed = 1;
            result.stopRequested = 1;
            Fail(E_FAIL);
            return false;
        }
        bool ButtonClicked(CGUIElement*)
        {
            if (inputQuiescing)
                return true;
            ++interaction.buttonClicks;
            interactionLabel->SetText("MTA button callback received");
            return true;
        }
        bool TextChanged(CGUIElement*)
        {
            if (inputQuiescing)
                return true;
            ++interaction.textChanges;
            interaction.textBytes = static_cast<std::uint32_t>(edit->GetText().size());
            return true;
        }
        bool TextAccepted(CGUIElement*)
        {
            if (inputQuiescing)
                return true;
            ++interaction.textAccepted;
            interactionLabel->SetText("MTA edit accepted text");
            return true;
        }
        bool CloseRequested(CGUIElement*)
        {
            if (inputQuiescing)
                return true;
            interaction.closeRequested = 1;
            result.stopRequested = 1;
            return true;
        }
        void ResetInput(bool buttonsOnly = false)
        {
            if (inputQuiescing)
                return;
            struct Quiescing
            {
                bool& value;
                explicit Quiescing(bool& flag) : value(flag) { value = true; }
                ~Quiescing() { value = false; }
            } quiescing(inputQuiescing);
            // Native capture must retire even if a GUI callback throws while
            // synthesizing releases. Nested capture notifications are ignored.
            if (GetCapture() == child)
                ReleaseCapture();
            if (gui && interaction.enabled)
            {
                if (buttonsOnly)
                    input.ResetButtons(*gui);
                else
                    input.Reset(*gui);
            }
        }
        bool ProcessInput(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (!interaction.enabled || frontendInputDeferred || interaction.inputDetached || !gui || result.stopRequested || inputQuiescing)
                return false;
            if (message == WM_SETFOCUS)
            {
                interaction.focused = 1;
                ++interaction.focusAcquired;
            }
            if (message == WM_KILLFOCUS)
            {
                interaction.focused = 0;
                ++interaction.focusLost;
                bool ignored = false;
                FrontendMessage(message, wparam, lparam, ignored);
                ResetInput();
                return false;
            }
            if (message == WM_CANCELMODE || message == WM_CAPTURECHANGED)
            {
                bool ignored = false;
                FrontendMessage(message, wparam, lparam, ignored);
                ResetInput(true);
                return false;
            }
            // Clicks target only this worker's HWND. Never install a foreign
            // parent hook or inject GUI callbacks onto the game's UI thread.
            if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN)
            {
                SetFocus(window);
                if (GetFocus() != window)
                    return false;
                SetCapture(window);
            }
            // Text and shortcuts are admitted only while this exact owned
            // child has keyboard focus; queued input after deactivation drops.
            if ((message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP || message == WM_CHAR ||
                 message == WM_UNICHAR) &&
                GetFocus() != window)
                return false;
            bool frontendHandled = false;
            if (!FrontendMessage(message, wparam, lparam, frontendHandled))
            {
                ResetInput();
                return false;
            }
            if (frontendHandled)
            {
                // Modal key capture consumes shortcuts, but a key pressed
                // before capture still owes the existing GUI its release.
                // Discharge only recorded holds, without injecting a press or
                // turning an unrelated consumed key-up into another action.
                if ((message == WM_KEYUP || message == WM_SYSKEYUP) && !input.ProcessConsumedRelease(*gui, message, wparam, lparam))
                {
                    interaction.inputFailed = 1;
                    result.stopRequested = 1;
                    Fail(E_FAIL);
                    return false;
                }
                return true;
            }
            if (message == WM_KEYDOWN && wparam == VK_ESCAPE && GetFocus() == window)
            {
                CloseRequested(nullptr);
                ResetInput();
                return true;
            }
            if (frontendEnabled)
            {
                bool mouseButton = false;
                switch (message)
                {
                    case WM_MOUSEMOVE:
                        frontendMouse = {static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
                        break;
                    case WM_LBUTTONDOWN:
                    case WM_LBUTTONUP:
                    case WM_RBUTTONDOWN:
                    case WM_RBUTTONUP:
                    case WM_MBUTTONDOWN:
                    case WM_MBUTTONUP:
                    case WM_XBUTTONDOWN:
                    case WM_XBUTTONUP:
                        frontendMouse = {static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
                        mouseButton = true;
                        break;
                    case WM_MOUSEWHEEL:
                        frontendMouse = {static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
                        ScreenToClient(window, &frontendMouse);
                        break;
                }
                // A move and click may arrive in the same Poll batch. Update
                // the real menu's hit selection before its existing callback
                // consumes the button, rather than using the previous frame.
                if (mouseButton && !PulseFrontend())
                {
                    interaction.inputFailed = 1;
                    result.stopRequested = 1;
                    Fail(E_FAIL);
                    return false;
                }
            }
            const bool handled = input.Process(*gui, message, wparam, lparam);
            if ((message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP || message == WM_XBUTTONUP) &&
                !input.Snapshot().pressedButtons && GetCapture() == window)
                ReleaseCapture();
            return handled;
        }
        void Retained() noexcept
        {
            result.moduleRetained = module != nullptr;
            result.sessionRetained = session != nullptr;
            result.deviceRetained = device != nullptr;
            result.childRetained = child != nullptr;
            result.queueRetained = queue != nullptr;
            result.surfaceRetained = swapchain != nullptr || composition != nullptr;
        }
        Status Fail(HRESULT hr, GuiD3D12Status status = GuiD3D12Status::Ok) noexcept
        {
            if (!failed)
            {
                result.failureStage = result.stage;
                result.hresult = static_cast<std::uint32_t>(FAILED(hr) ? hr : E_FAIL);
                failed = true;
            }
            if (status != GuiD3D12Status::Ok && status != GuiD3D12Status::Busy && !result.guiStatus)
                result.guiStatus = static_cast<std::uint32_t>(status);
            Retained();
            return Status::Failed;
        }
        bool Removed() noexcept
        {
            const auto hr = device->GetDeviceRemovedReason();
            if (SUCCEEDED(hr))
                return false;
            result.deviceRemoved = 1;
            gpuComplete = true;
            Fail(hr);
            return true;
        }
        Status Completion() noexcept
        {
            if (!submitted || gpuComplete)
                return Status::Ok;
            if (signalPending)
            {
                ++result.signalAttempts;
                const auto hr = queue->Signal(fence.Get(), result.fenceValue);
                if (FAILED(hr))
                {
                    Fail(hr);
                    return Removed() ? Status::Ok : Status::Busy;
                }
                signalPending = false;
            }
            result.completedFenceValue = fence->GetCompletedValue();
            if (result.completedFenceValue == UINT64_MAX)
                return Removed() ? Status::Ok : Status::Busy;
            gpuComplete = result.completedFenceValue >= result.fenceValue;
            return gpuComplete ? Status::Ok : Status::Busy;
        }
        static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept
        {
            auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
            if (message == WM_NCCREATE)
            {
                self = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            const bool admitsInput = self && self->interaction.enabled && !self->frontendInputDeferred && !self->interaction.inputDetached &&
                                     !self->result.stopRequested && !self->inputQuiescing;
            if (message == WM_NCHITTEST)
                return admitsInput ? HTCLIENT : HTTRANSPARENT;
            if (message == WM_MOUSEACTIVATE)
                return admitsInput ? MA_ACTIVATE : MA_NOACTIVATE;
            if (self && self->interaction.enabled)
            {
                try
                {
                    if (admitsInput && message == WM_SETCURSOR && LOWORD(lparam) == HTCLIENT && self->gui)
                    {
                        // Draw MTA's own cursor without changing the process
                        // ShowCursor counter or the native parent's cursor.
                        SetCursor(nullptr);
                        return TRUE;
                    }
                    if (self->ProcessInput(window, message, wparam, lparam))
                        return message == WM_XBUTTONDOWN || message == WM_XBUTTONUP ? TRUE : 0;
                }
                catch (...)
                {
                    self->interaction.inputFailed = 1;
                    self->result.stopRequested = 1;
                }
            }
            if (message == WM_ERASEBKGND)
                return 1;
            if (message == WM_PAINT)
            {
                PAINTSTRUCT paint{};
                BeginPaint(window, &paint);
                EndPaint(window, &paint);
                return 0;
            }
            if (message == WM_CLOSE)
            {
                if (self && self->interaction.enabled)
                    self->CloseRequested(nullptr);
                return 0;
            }
            if (message == WM_NCDESTROY && self)
            {
                self->child = nullptr;
                self->result.childVisible = 0;
                SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            }
            return DefWindowProcW(window, message, wparam, lparam);
        }
        bool ParentSize(UINT& width, UINT& height) noexcept
        {
            DWORD      process = 0;
            const auto thread = GetWindowThreadProcessId(parent, &process);
            RECT       rectangle{};
            const bool valid = IsWindow(parent) && thread == parentThread && process == GetCurrentProcessId() && GetAncestor(parent, GA_ROOT) == parent &&
                               !(GetWindowLongPtrW(parent, GWL_STYLE) & WS_CHILD) && GetClientRect(parent, &rectangle);
            result.parentAlive = valid;
            if (!valid)
            {
                result.parentDestroyed = 1;
                result.stopRequested = 1;
                return false;
            }
            width = static_cast<UINT>(std::max<LONG>(0, rectangle.right - rectangle.left));
            height = static_cast<UINT>(std::max<LONG>(0, rectangle.bottom - rectangle.top));
            return true;
        }
        HRESULT MakeReadback()
        {
            captureWidth = std::min<UINT>(result.width, 640);
            captureHeight = std::min<UINT>(result.height, 480);
            D3D12_RESOURCE_DESC description{};
            description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            description.Width = captureWidth;
            description.Height = captureHeight;
            description.DepthOrArraySize = description.MipLevels = 1;
            description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            description.SampleDesc.Count = 1;
            UINT64 bytes = 0;
            device->GetCopyableFootprints(&description, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
            if (!bytes || bytes == UINT64_MAX)
                return E_FAIL;
            description = {};
            description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            description.Width = bytes;
            description.Height = description.DepthOrArraySize = description.MipLevels = 1;
            description.SampleDesc.Count = 1;
            description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_READBACK;
            heap.CreationNodeMask = heap.VisibleNodeMask = 1;
            return device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
        }
        void PositionWidgets()
        {
            if (!image)
                return;
            redX = std::min<UINT>(500, result.width - 80);
            redY = std::min<UINT>(100, result.height - 80);
            image->SetPosition(CVector2D(static_cast<float>(redX), static_cast<float>(redY)));
        }
        Status Readback()
        {
            if (result.readbackComplete || !submitted || !gpuComplete || result.deviceRemoved)
                return Status::Ok;
            void*       mapped = nullptr;
            D3D12_RANGE range{0, static_cast<SIZE_T>(readback->GetDesc().Width)};
            const auto  hr = readback->Map(0, &range, &mapped);
            if (FAILED(hr))
                return Fail(hr);
            unsigned      nonzero = 0, invalidPremult = 0;
            std::uint32_t transparent = 1, red = 0;
            for (UINT y = 0; y < captureHeight; ++y)
            {
                const auto* row = static_cast<const BYTE*>(mapped) + footprint.Offset + SIZE_T(y) * footprint.Footprint.RowPitch;
                for (UINT x = 0; x < captureWidth; ++x)
                {
                    std::uint32_t pixel = 0;
                    std::memcpy(&pixel, row + SIZE_T(x) * 4, sizeof(pixel));
                    nonzero += pixel != 0;
                    const auto alpha = pixel >> 24;
                    invalidPremult += (pixel & 255) > alpha + 1 || ((pixel >> 8) & 255) > alpha + 1 || ((pixel >> 16) & 255) > alpha + 1;
                    if (!x && !y)
                        transparent = pixel;
                    if (x == redX + 32 && y == redY + 32)
                        red = pixel;
                }
            }
            const D3D12_RANGE noWrite{};
            readback->Unmap(0, &noWrite);
            result.readbackPixels = nonzero;
            if (frontendEnabled)
            {
                // The real menu fades in using its existing lifecycle. Early
                // empty frames are not fixture failures; keep sampling until
                // substantial actual content appears or the caller times out.
                if (invalidPremult)
                    return Fail(E_FAIL);
                result.readbackComplete = nonzero > 10000;
                result.premultipliedPixelsVerified = result.readbackComplete;
                return Status::Ok;
            }
            result.readbackComplete = 1;
            result.transparentPixelVerified = transparent == 0;
            result.readbackRedVerified = red == 0xffff0000;
            result.premultipliedPixelsVerified = invalidPremult == 0;
            return result.transparentPixelVerified && result.readbackRedVerified && result.premultipliedPixelsVerified && nonzero > 10000 ? Status::Ok
                                                                                                                                          : Fail(E_FAIL);
        }
        Status Present() noexcept
        {
            ++result.presentCalls;
            const auto hr = swapchain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
            result.presentHresult = static_cast<std::uint32_t>(hr);
            if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
            {
                ++result.presentBusy;
                return Status::Busy;
            }
            presentPending = false;
            if (FAILED(hr))
                return Fail(hr);
            if (hr == S_OK)
                ++result.presentSucceeded;
            return Status::Ok;
        }
        Status Submit()
        {
            if (!ReleaseOwned(buffer))
            {
                result.releaseFault = 1;
                return Fail(E_UNEXPECTED);
            }
            auto hr = swapchain->GetBuffer(swapchain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&buffer));
            if (FAILED(hr))
                return Fail(hr);
            hr = clearAllocator->Reset();
            if (SUCCEEDED(hr))
                hr = clearList->Reset(clearAllocator.Get(), nullptr);
            if (FAILED(hr))
                return Fail(hr);
            const auto handle = rtv->GetCPUDescriptorHandleForHeapStart();
            device->CreateRenderTargetView(buffer.Get(), nullptr, handle);
            Barrier(clearList.Get(), buffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            constexpr float transparent[] = {0, 0, 0, 0};
            clearList->ClearRenderTargetView(handle, transparent, 0, nullptr);
            Barrier(clearList.Get(), buffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            hr = clearList->Close();
            if (FAILED(hr))
                return Fail(hr);
            // Completion covers clear, the actual GUI's own submissions, and
            // copy. A failed Signal never permits allocator reset or teardown.
            submitted = true;
            gpuComplete = false;
            signalPending = true;
            ++result.fenceValue;
            ID3D12CommandList* clear[] = {clearList.Get()};
            queue->ExecuteCommandLists(1, clear);
            ++result.ownQueueSubmissions;
            const mta::gui::GuiD3D12Frame frame{buffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT, result.width, result.height};
            const auto                    status = session->RenderFrame(frame);
            if (status != GuiD3D12Status::Ok)
            {
                Completion();
                return Fail(E_FAIL, status);
            }
            ++result.guiFrames;
            hr = copyAllocator->Reset();
            if (SUCCEEDED(hr))
                hr = copyList->Reset(copyAllocator.Get(), nullptr);
            if (FAILED(hr))
            {
                Completion();
                return Fail(hr);
            }
            Barrier(copyList.Get(), buffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
            source.pResource = buffer.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination.pResource = readback.Get();
            destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            destination.PlacedFootprint = footprint;
            const D3D12_BOX box{0, 0, 0, captureWidth, captureHeight, 1};
            copyList->CopyTextureRegion(&destination, 0, 0, 0, &source, &box);
            Barrier(copyList.Get(), buffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
            hr = copyList->Close();
            if (FAILED(hr))
            {
                Completion();
                return Fail(hr);
            }
            ID3D12CommandList* copies[] = {copyList.Get()};
            queue->ExecuteCommandLists(1, copies);
            ++result.ownQueueSubmissions;
            Completion();
            if (failed)
                return Status::Failed;
            presentPending = true;
            return Present();
        }
        Status Resize(UINT width, UINT height)
        {
            result.resizePending = 1;
            result.stage = static_cast<unsigned>(Stage::Resizing);
            const auto status = session->Resize(width, height);
            if (status == GuiD3D12Status::Busy)
                return Status::Busy;
            if (status != GuiD3D12Status::Ok)
                return Fail(E_FAIL, status);
            if (!ReleaseOwned(buffer) || !ReleaseOwned(readback))
            {
                result.releaseFault = 1;
                return Fail(E_UNEXPECTED);
            }
            // Both GUI frame slots and this helper's copy are retired. Only this
            // composition swapchain is resized; the borrowed parent is untouched.
            auto hr = clearAllocator->Reset();
            if (SUCCEEDED(hr))
                hr = clearList->Reset(clearAllocator.Get(), nullptr);
            if (SUCCEEDED(hr))
                hr = clearList->Close();
            if (SUCCEEDED(hr))
                hr = copyAllocator->Reset();
            if (SUCCEEDED(hr))
                hr = copyList->Reset(copyAllocator.Get(), nullptr);
            if (SUCCEEDED(hr))
                hr = copyList->Close();
            if (SUCCEEDED(hr))
                hr = swapchain->ResizeBuffers(3, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
            if (FAILED(hr))
                return Fail(hr);
            result.width = width;
            result.height = height;
            submitted = false;
            result.readbackComplete = result.readbackPixels = result.readbackRedVerified = result.transparentPixelVerified =
                result.premultipliedPixelsVerified = 0;
            hr = MakeReadback();
            if (FAILED(hr))
                return Fail(hr);
            PositionWidgets();
            if (!SetWindowPos(child, HWND_TOP, 0, 0, static_cast<int>(width), static_cast<int>(height), SWP_NOACTIVATE))
                return Fail(HRESULT_FROM_WIN32(GetLastError()));
            ++result.resizeCount;
            result.resizePending = 0;
            result.stage = static_cast<unsigned>(Stage::Running);
            return Status::Ok;
        }
    };

    Session::~Session() noexcept
    {
        // The owner explicitly drains. Destruction during failed native cleanup
        // must not invoke COM, GUI, HWND or loader code behind retained peers.
        if (impl && impl->result.cleanupComplete)
            delete impl;
    }
    Status Session::Prepare(const wchar_t* absoluteGuiDll, const wchar_t* absoluteAssetRoot) noexcept
    {
        if (impl || !absoluteGuiDll || !absoluteAssetRoot)
            return Status::InvalidArgument;
        impl = new (std::nothrow) Impl;
        if (!impl)
            return Status::Failed;
        try
        {
            const std::filesystem::path library(absoluteGuiDll), root(absoluteAssetRoot);
            if (!library.is_absolute() || !root.is_absolute() || library.filename() != L"cgui.dll" ||
                library.lexically_normal().parent_path() != root.lexically_normal())
                return impl->Fail(E_INVALIDARG);
            const auto counted = root.lexically_normal().wstring();
            if (counted.empty() || counted.size() > 32767)
                return impl->Fail(E_INVALIDARG);
            std::filesystem::create_directories(root / L"mta/logs");
            impl->module = LoadLibraryExW(library.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!impl->module)
                return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
            const auto configure = reinterpret_cast<mta::gui::ConfigureGuiD3D12AssetRoot>(GetProcAddress(impl->module, "ConfigureGUID3D12AssetRoot"));
            impl->create = reinterpret_cast<mta::gui::CreateGuiD3D12Session>(GetProcAddress(impl->module, "CreateGUID3D12Session"));
            if (!configure || !impl->create)
                return impl->Fail(HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND));
            if (!configure(mta::gui::GuiD3D12AssetRootApiVersion, counted.data(), static_cast<std::uint32_t>(counted.size())))
                return impl->Fail(E_INVALIDARG);
            impl->prepared = true;
            impl->result.modulePrepared = 1;
            impl->result.stage = static_cast<unsigned>(Stage::Prepared);
            impl->Retained();
            return Status::Ok;
        }
        catch (...)
        {
            return impl->Fail(E_FAIL);
        }
    }
    Status Session::AttachDeviceWindow(ID3D12Device* transferredReference, HWND__* borrowedParent, bool windowed) noexcept
    {
        if (!impl)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (!impl->prepared || impl->device || impl->started || impl->result.stopRequested || !transferredReference)
            return Status::InvalidArgument;
        impl->device = transferredReference;
        impl->parent = borrowedParent;
        impl->result.deviceAttached = 1;
        impl->result.parentWindowed = windowed ? 1u : 0u;
        impl->result.stage = static_cast<unsigned>(Stage::Attached);
        impl->Retained();
        return Status::Ok;
    }
    Status Session::EnableInteraction() noexcept
    {
        if (!impl)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (!impl->prepared || impl->started || impl->failed || impl->result.stopRequested || impl->frontendEnabled)
            return Status::InvalidArgument;
        impl->interaction.enabled = 1;
        return Status::Ok;
    }
    Status Session::EnableFrontend(const FrontendCallbacks& callbacks, bool deferredInput) noexcept
    {
        if (!impl)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (!impl->prepared || impl->started || impl->failed || impl->result.stopRequested || impl->interaction.enabled || !callbacks.start ||
            !callbacks.pulse || !callbacks.stop)
            return Status::InvalidArgument;
        impl->frontend = callbacks;
        impl->frontendEnabled = true;
        impl->frontendInputDeferred = deferredInput;
        impl->interaction.enabled = 1;
        return Status::Ok;
    }
    Status Session::Start() noexcept
    {
        if (!impl)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (!impl->prepared || !impl->device || impl->started || impl->result.stopRequested)
            return Status::InvalidArgument;
        impl->started = true;
        try
        {
            DWORD process = 0;
            impl->parentThread = GetWindowThreadProcessId(impl->parent, &process);
            UINT width = 0, height = 0;
            if (!impl->result.parentWindowed || process != GetCurrentProcessId() || !impl->ParentSize(width, height) || width < 240 || height < 200 ||
                width > 16384 || height > 16384)
                return impl->Fail(E_INVALIDARG);
            impl->result.parentThreadId = impl->parentThread;
            impl->result.parentValidated = 1;
            impl->result.width = width;
            impl->result.height = height;
            const auto apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (SUCCEEDED(apartment))
                impl->comInitialized = true;
            else if (apartment != RPC_E_CHANGED_MODE)
                return impl->Fail(apartment);
            const auto luid = impl->device->GetAdapterLuid();
            impl->result.deviceLuidLow = luid.LowPart;
            impl->result.deviceLuidHigh = static_cast<std::uint32_t>(luid.HighPart);
            impl->result.nodeCount = impl->device->GetNodeCount();
            if (impl->result.nodeCount != 1)
                return impl->Fail(E_INVALIDARG);
            D3D12_COMMAND_QUEUE_DESC queue{};
            queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            auto hr = impl->device->CreateCommandQueue(&queue, IID_PPV_ARGS(&impl->queue));
            if (FAILED(hr))
                return impl->Fail(hr);
            impl->result.queueCreated = 1;
            hr = impl->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&impl->fence));
            if (FAILED(hr))
                return impl->Fail(hr);
            hr = impl->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&impl->clearAllocator));
            if (SUCCEEDED(hr))
                hr = impl->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&impl->copyAllocator));
            if (SUCCEEDED(hr))
                hr = impl->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, impl->clearAllocator.Get(), nullptr, IID_PPV_ARGS(&impl->clearList));
            if (SUCCEEDED(hr))
                hr = impl->clearList->Close();
            if (SUCCEEDED(hr))
                hr = impl->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, impl->copyAllocator.Get(), nullptr, IID_PPV_ARGS(&impl->copyList));
            if (SUCCEEDED(hr))
                hr = impl->copyList->Close();
            if (FAILED(hr))
                return impl->Fail(hr);
            D3D12_DESCRIPTOR_HEAP_DESC heap{};
            heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heap.NumDescriptors = 1;
            hr = impl->device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&impl->rtv));
            if (FAILED(hr))
                return impl->Fail(hr);
            hr = CreateDXGIFactory1(IID_PPV_ARGS(&impl->factory));
            if (FAILED(hr))
                return impl->Fail(hr);
            DXGI_SWAP_CHAIN_DESC1 description{};
            description.Width = width;
            description.Height = height;
            description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            description.BufferCount = 3;
            description.Scaling = DXGI_SCALING_STRETCH;
            description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
            hr = impl->factory->CreateSwapChainForComposition(impl->queue.Get(), &description, nullptr, &impl->initialChain);
            if (SUCCEEDED(hr))
                hr = impl->initialChain.As(&impl->swapchain);
            if (FAILED(hr))
                return impl->Fail(hr);
            impl->result.swapchainCreated = 1;
            hr = impl->MakeReadback();
            if (FAILED(hr))
                return impl->Fail(hr);
            impl->result.stage = static_cast<unsigned>(Stage::Resources);
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    reinterpret_cast<LPCWSTR>(&Impl::WindowProc), &impl->windowModule))
                return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
            swprintf_s(impl->windowClass, L"MtaDefinitiveSurface_%lu_%p", impl->owner, static_cast<void*>(impl));
            WNDCLASSEXW windowClass{};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.lpfnWndProc = &Impl::WindowProc;
            windowClass.hInstance = impl->windowModule;
            windowClass.lpszClassName = impl->windowClass;
            if (!RegisterClassExW(&windowClass))
                return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
            impl->classRegistered = true;
            const DWORD extendedStyle = WS_EX_NOREDIRECTIONBITMAP | (impl->interaction.enabled ? 0 : WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);
            impl->child = CreateWindowExW(extendedStyle, impl->windowClass, L"MTA experimental GUI", WS_CHILD | (impl->frontendInputDeferred ? 0 : WS_VISIBLE),
                                          0, 0, static_cast<int>(width), static_cast<int>(height), impl->parent, nullptr, impl->windowModule, impl);
            if (!impl->child)
                return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
            impl->result.childCreated = 1;
            impl->result.childVisible = IsWindowVisible(impl->child) ? 1u : 0u;
            impl->result.childShown = impl->result.childVisible;
            // Null is deliberate: DirectComposition owns only a visual tree;
            // its content is our D3D12 composition swapchain, not a D2D surface.
            hr = DCompositionCreateDevice2(nullptr, IID_PPV_ARGS(&impl->composition));
            if (SUCCEEDED(hr))
                hr = impl->composition->CreateTargetForHwnd(impl->child, TRUE, &impl->compositionTarget);
            if (SUCCEEDED(hr))
                hr = impl->composition->CreateVisual(&impl->visual);
            if (SUCCEEDED(hr))
                hr = impl->visual->SetContent(impl->swapchain.Get());
            if (SUCCEEDED(hr))
                hr = impl->compositionTarget->SetRoot(impl->visual.Get());
            if (SUCCEEDED(hr))
                hr = impl->composition->Commit();
            if (FAILED(hr))
                return impl->Fail(hr);
            impl->result.compositionCreated = 1;
            impl->result.stage = static_cast<unsigned>(Stage::Composed);
            mta::gui::GuiD3D12CreateInfo info;
            info.device = impl->device;
            info.queue = impl->queue.Get();
            info.targetFormat = 87;
            info.width = width;
            info.height = height;
            impl->session = impl->create(mta::gui::GuiD3D12ApiVersion, &info);
            if (!impl->session)
                return impl->Fail(E_FAIL, GuiD3D12Status::Failed);
            impl->result.sessionCreated = 1;
            impl->result.stage = static_cast<unsigned>(Stage::Session);
            const auto status = impl->session->GetGUI(impl->gui);
            if (status != GuiD3D12Status::Ok || !impl->gui)
                return impl->Fail(E_FAIL, status);
            impl->gui->SetSkin("Default");
            impl->gui->SetCursorEnabled(impl->interaction.enabled != 0);
            if (impl->interaction.enabled)
            {
                impl->gui->ProcessMouseInput(CGUI_MI_MOUSEPOS, width - 24, height - 24);
                impl->frontendMouse = {static_cast<LONG>(width - 24), static_cast<LONG>(height - 24)};
            }
            if (!impl->gui->GetDefaultFont() || impl->gui->GetTextExtent("MTA: Definitive Edition") <= 0)
                return impl->Fail(E_FAIL);
            if (impl->frontendEnabled)
            {
                // Commit the cleanup obligation before crossing Core's factory:
                // it may publish a failed but still-owned frontend session.
                impl->frontendStartAttempted = true;
                Impl::FrontendCall call(impl->frontendCallbackActive);
                if (!impl->frontend.start(impl->frontend.context, impl->gui))
                    return impl->Fail(E_FAIL);
            }
            else
            {
                auto* window = impl->gui->CreateWnd(nullptr, "MTA: Definitive Edition / EXPERIMENTAL");
                window->SetPosition(CVector2D(24, 24));
                window->SetSize(CVector2D(static_cast<float>(std::min<UINT>(430, width - 48)), static_cast<float>(std::min<UINT>(260, height - 48))));
                window->SetVisible(true);
                auto* button = impl->gui->CreateButton(static_cast<CGUIElement*>(window), "Existing MTA GUI - composition trial");
                button->SetPosition(CVector2D(20, 55));
                button->SetSize(CVector2D(300, 44));
                button->SetVisible(true);
                if (impl->interaction.enabled)
                {
                    // These are the same widgets and callbacks used by MTA menus.
                    // The owned host validates routing, not a replacement frontend.
                    button->SetText("Test MTA button");
                    button->SetClickHandler(GUI_CALLBACK(&Impl::ButtonClicked, impl));
                    impl->edit = impl->gui->CreateEdit(window, "");
                    impl->edit->SetPosition(CVector2D(20, 110));
                    impl->edit->SetSize(CVector2D(300, 30));
                    impl->edit->SetMaxLength(64);
                    impl->edit->SetVisible(true);
                    impl->edit->SetTextChangedHandler(GUI_CALLBACK(&Impl::TextChanged, impl));
                    impl->edit->SetTextAcceptedHandler(GUI_CALLBACK(&Impl::TextAccepted, impl));
                    impl->interactionLabel = impl->gui->CreateLabel(window, "Click the button or type and press Enter");
                    impl->interactionLabel->SetPosition(CVector2D(20, 150));
                    impl->interactionLabel->SetSize(CVector2D(370, 28));
                    impl->interactionLabel->SetVisible(true);
                    window->SetCloseClickHandler(GUI_CALLBACK(&Impl::CloseRequested, impl));
                }
                impl->texture = impl->gui->CreateTexture();
                std::array<std::uint32_t, 16> red{};
                red.fill(0xffff0000);
                impl->texture->LoadFromMemory(red.data(), 4, 4);
                impl->image = impl->gui->CreateStaticImage();
                impl->image->SetSize(CVector2D(64, 64));
                impl->image->SetFrameEnabled(false);
                impl->image->SetVisible(true);
                impl->PositionWidgets();
                if (!impl->image->LoadFromTexture(impl->texture))
                    return impl->Fail(E_FAIL);
            }
            impl->result.widgetsCreated = 1;
            impl->result.stage = static_cast<unsigned>(Stage::Running);
            impl->Retained();
            impl->FocusFrontendOnStart();
            const auto submitted = impl->Submit();
            return submitted == Status::Busy ? Status::Ok : submitted;
        }
        catch (...)
        {
            return impl->Fail(E_FAIL);
        }
    }
    Status Session::AdmitFrontendInput() noexcept
    {
        if (!impl)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (impl->frontendCallbackActive)
            return Status::Busy;
        if (!impl->frontendEnabled || !impl->started || !impl->gui || !impl->child || impl->failed || impl->result.stopRequested ||
            impl->interaction.inputDetached)
            return Status::InvalidArgument;
        if (!impl->frontendInputDeferred)
            return Status::Ok;
        // The native owner has already proved a completed/restored scope. Show
        // only our child without activating any background parent or app.
        if (!SetWindowPos(impl->child, nullptr, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER))
            return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
        impl->frontendInputDeferred = false;
        impl->result.childVisible = impl->result.childShown = IsWindowVisible(impl->child) ? 1u : 0u;
        if (!impl->result.childShown)
            return impl->Fail(E_FAIL);
        impl->FocusFrontendOnStart();
        return Status::Ok;
    }

    Status Session::Poll() noexcept
    {
        if (!impl)
            return Status::InvalidArgument;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (impl->frontendCallbackActive)
            return Status::Busy;
        if (!impl->started)
            return Status::InvalidArgument;
        if (impl->result.stopRequested)
            return impl->interaction.enabled ? (impl->interaction.inputFailed ? Status::Failed : Status::Ok) : Status::InvalidArgument;
        try
        {
            MSG message{};
            for (unsigned count = 0; count < 32 && impl->child && PeekMessageW(&message, impl->child, 0, 0, PM_REMOVE); ++count)
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (impl->result.stopRequested)
                return impl->interaction.inputFailed ? Status::Failed : Status::Ok;
            UINT width = 0, height = 0;
            if (!impl->ParentSize(width, height))
                return Status::Ok;
            const auto completion = impl->Completion();
            if (completion != Status::Ok)
                return completion;
            if (impl->failed)
                return Status::Failed;
            if (impl->Readback() != Status::Ok)
                return Status::Failed;
            if (impl->presentPending)
            {
                const auto status = impl->Present();
                if (status != Status::Ok)
                    return status;
            }
            if (width < 240 || height < 200 || width > 16384 || height > 16384)
                return Status::Busy;
            if (width != impl->result.width || height != impl->result.height)
                return impl->Resize(width, height);
            const auto now = GetTickCount64();
            if (now < impl->nextFrameAt)
                return Status::Ok;
            impl->nextFrameAt = now + 33;
            if (impl->frontendEnabled && !impl->PulseFrontend())
                return impl->Fail(E_FAIL);
            return impl->Submit();
        }
        catch (...)
        {
            return impl->Fail(E_FAIL);
        }
    }
    Status Session::DetachInput() noexcept
    {
        if (!impl)
            return Status::Ok;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (impl->frontendCallbackActive)
            return Status::Busy;
        impl->result.stopRequested = 1;
        impl->result.stage = static_cast<unsigned>(Stage::Stopping);
        try
        {
            if (impl->interaction.enabled && !impl->interaction.inputDetached)
            {
                // Retire input immediately even if GPU resources need several
                // nonblocking drain attempts. No held GUI key survives hiding.
                impl->ResetInput();
                if (impl->gui)
                    impl->gui->SetCursorEnabled(false);
                if (impl->frontendEnabled && GetFocus() == impl->child)
                    SetFocus(nullptr);
                impl->interaction.focused = 0;
                impl->interaction.inputDetached = 1;
            }
        }
        catch (...)
        {
            impl->interaction.inputFailed = 1;
            return impl->Fail(E_FAIL);
        }
        return Status::Ok;
    }

    Status Session::Shutdown() noexcept
    {
        if (!impl)
            return Status::Ok;
        if (GetCurrentThreadId() != impl->owner)
            return Status::WrongThread;
        if (impl->frontendCallbackActive)
            return Status::Busy;
        if (impl->result.cleanupComplete)
            return Status::Ok;
        if (impl->result.releaseFault)
            return Status::Failed;
        const auto detached = DetachInput();
        if (detached != Status::Ok)
            return detached;
        // GPU retirement may outlive the last Poll. Refresh borrowed-window
        // facts during every drain retry, without preventing owned cleanup.
        if (impl->started && impl->result.parentValidated)
        {
            UINT width = 0, height = 0;
            impl->ParentSize(width, height);
        }
        if (impl->child && !IsWindow(impl->child))
            impl->child = nullptr;
        if (impl->child)
        {
            ShowWindow(impl->child, SW_HIDE);
            impl->result.childVisible = 0;
        }
        if (impl->frontendStartAttempted)
        {
            try
            {
                // Retire real menu callbacks while their borrowed CGUI still
                // exists, even if its GPU versions remain behind a fence.
                Impl::FrontendCall call(impl->frontendCallbackActive);
                if (!impl->frontend.stop(impl->frontend.context))
                    return Status::Busy;
                impl->frontendStartAttempted = false;
                impl->frontend = {};
            }
            catch (...)
            {
                return impl->Fail(E_FAIL);
            }
        }
        if (impl->Completion() != Status::Ok)
            return Status::Busy;
        try
        {
            if (impl->texture)
            {
                if (impl->image)
                    impl->image->Clear();
                delete impl->texture;
                impl->texture = nullptr;
            }
            if (impl->session)
            {
                auto status = GuiD3D12Status::Failed;
                if (!ReleaseSession(impl->session, status))
                {
                    impl->result.releaseFault = 1;
                    return impl->Fail(E_UNEXPECTED);
                }
                if (status == GuiD3D12Status::Busy)
                    return Status::Busy;
                if (status != GuiD3D12Status::Ok)
                    return impl->Fail(E_FAIL, status);
                impl->gui = nullptr;
                impl->image = nullptr;
            }
            // Destruction of the borrowed parent destroys our child too. A
            // vanished HWND cannot display the target, and need not accept a
            // new DComp transaction before our drained local refs are released.
            if (impl->composition && impl->child)
            {
                auto hr = impl->visual ? impl->visual->SetContent(nullptr) : S_OK;
                if (SUCCEEDED(hr) && impl->compositionTarget)
                    hr = impl->compositionTarget->SetRoot(nullptr);
                if (SUCCEEDED(hr))
                    hr = impl->composition->Commit();
                if (FAILED(hr))
                    return impl->Fail(hr);
            }
            if (!ReleaseOwned(impl->visual) || !ReleaseOwned(impl->compositionTarget) || !ReleaseOwned(impl->composition))
            {
                impl->result.releaseFault = 1;
                return impl->Fail(E_UNEXPECTED);
            }
            impl->result.compositionCleanupComplete = 1;
            if (impl->child)
            {
                if (!DestroyWindow(impl->child))
                    return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
                impl->child = nullptr;
            }
            impl->result.windowCleanupComplete = 1;
            if (impl->classRegistered)
            {
                if (!UnregisterClassW(impl->windowClass, impl->windowModule))
                    return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
                impl->classRegistered = false;
            }
            impl->result.classUnregistered = 1;
            if (!ReleaseOwned(impl->buffer) || !ReleaseOwned(impl->readback) || !ReleaseOwned(impl->clearList) || !ReleaseOwned(impl->copyList) ||
                !ReleaseOwned(impl->clearAllocator) || !ReleaseOwned(impl->copyAllocator) || !ReleaseOwned(impl->rtv) || !ReleaseOwned(impl->initialChain) ||
                !ReleaseOwned(impl->swapchain) || !ReleaseOwned(impl->factory) || !ReleaseOwned(impl->fence) || !ReleaseOwned(impl->queue))
            {
                impl->result.releaseFault = 1;
                return impl->Fail(E_UNEXPECTED);
            }
            if (impl->device)
            {
                auto* device = impl->device;
                impl->device = nullptr;
                if (!ReleaseUnknown(device))
                {
                    impl->result.releaseFault = 1;
                    return impl->Fail(E_UNEXPECTED);
                }
            }
            if (impl->comInitialized)
            {
                CoUninitialize();
                impl->comInitialized = false;
            }
            if (impl->module)
            {
                if (!FreeLibrary(impl->module))
                {
                    impl->result.releaseFault = 1;
                    return impl->Fail(HRESULT_FROM_WIN32(GetLastError()));
                }
                impl->module = nullptr;
                impl->create = nullptr;
            }
            // A failed resize remains a failed workload, but a fully retired
            // surface has no outstanding resize operation or buffer lifetime.
            impl->result.resizePending = 0;
            impl->result.cleanupComplete = 1;
            impl->result.stage = static_cast<unsigned>(Stage::Complete);
            impl->Retained();
            return Status::Ok;
        }
        catch (...)
        {
            return impl->Fail(E_FAIL);
        }
    }
    Result Session::Snapshot() const noexcept
    {
        if (!impl)
        {
            Result empty;
            empty.cleanupComplete = empty.windowCleanupComplete = empty.compositionCleanupComplete = empty.classUnregistered = 1;
            return empty;
        }
        return GetCurrentThreadId() == impl->owner ? impl->result : Result{};
    }
    InteractionResult Session::InteractionSnapshot() const noexcept
    {
        if (!impl || GetCurrentThreadId() != impl->owner)
            return {};
        auto       result = impl->interaction;
        const auto input = impl->input.Snapshot();
        result.events = input.injectedEvents;
        result.syntheticReleases = input.syntheticReleases;
        result.pressedKeys = input.pressedKeys;
        result.pressedButtons = input.pressedButtons;
        return result;
    }
}
#endif
