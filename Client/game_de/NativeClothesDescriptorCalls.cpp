/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA CPU ownership while invoking the real initializer
 ****************************************************************************/
#include "NativeClothesDescriptorCalls.h"
#include "NativeNameConstructionContext.h"
#include "NativeStartGameCalls.h"
#include <array>
#include <cstring>
#include <utility>
namespace mta::game_de::NativeClothesDescriptorCalls
{
    namespace OwnedClothesDetail
    {
        namespace P = NativeClothesDescriptorProfile;
        namespace N = NativeNameConstructionContext;
        namespace D = NativeStreamingCompletionContext;
        namespace M = NativeModelReferenceMap;
        namespace H = NativeModelReferenceHeap;

        bool Profile(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, P::MaximumCodeBytes> bytes{};
            for (const auto& pin : P::Code)
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            for (const auto& pin : P::Names)
                if (!pin.size || pin.size > bytes.size() || !H::ReadOnlyImage(image, image + pin.rva, pin.size) ||
                    !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            return N::Profile(image) && D::Profile(image);
        }
        struct Context
        {
            N::Names names;
            D::Value world;
        };
        ClockResult Capture(std::uintptr_t image, CheckOwner owner, Context& output)
        {
            Context value;
            auto    result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            if ((result = N::ReadNames(image, value.names)) != ClockResult::Ok || (result = D::Capture(image, value.world)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, value.world) || !Profile(image))
                return ClockResult::BindingChanged;
            if ((result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = std::move(value);
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_CLOTHES_DESCRIPTOR_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t descriptor, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(descriptor);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    ClockResult Initialize(std::uintptr_t image, NativeClothesDescriptorStorage& storage, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        using namespace OwnedClothesDetail;
        using State = NativeClothesDescriptorStorage::State;
        Receipt receipt;
        receipt.entryRva = P::InitializeRva;
        const auto descriptor = reinterpret_cast<std::uintptr_t>(storage.bytes.data());
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        if (storage.state.load(std::memory_order_acquire) != State::Fresh)
            return finish(ClockResult::Busy);
        try
        {
            Context before, confirmed;
            auto    result = Capture(image, owner, before);
            if (result != ClockResult::Ok)
                return finish(result);
            // The type owns this exact private CPU buffer and has no public
            // pointer-adoption path. Native sees only its0xe8-byte prefix;
            // bookkeeping stays outside the source-proved store footprint.
            if (descriptor % alignof(std::uintptr_t) || !M::PrivateWritable(descriptor, P::DescriptorBytes) ||
                !M::Disjoint(descriptor, P::DescriptorBytes, before.names.heap.authority, NativeModelReferenceTransactionProfile::HeapBytes))
                return finish(ClockResult::BindingChanged);
            receipt.root = before.world.root;
            receipt.gameMode = before.world.gameMode;
            receipt.level = before.world.level;
            receipt.world = before.world.world;
            if ((result = Capture(image, owner, confirmed)) != ClockResult::Ok)
                return finish(result);
            if (!N::Same(before.names, confirmed.names) || !D::Same(before.world, confirmed.world, true))
                return finish(ClockResult::BindingChanged);
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_CLOTHES_DESCRIPTOR_NATIVE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_CLOTHES_DESCRIPTOR_NATIVE_INVOKE;
#else
            const auto entry = invoke ? invoke : &NativeCall;
#endif
            storage.state.store(State::Entered, std::memory_order_release);
            receipt.entered = true;
            // Match actual MTA ownership and initialization policy: only the
            // genuine initializer is called, once. No native malloc, constructor
            // (which would initialize again), free or attachment is performed.
            result = entry(reinterpret_cast<NativeFunction>(image + P::InitializeRva), descriptor, receipt.exceptionCode);
            if (result != ClockResult::Ok)
            {
                storage.state.store(State::Failed, std::memory_order_release);
                return finish(result);
            }
            receipt.returned = true;
            if ((result = Capture(image, owner, confirmed)) != ClockResult::Ok)
            {
                storage.state.store(State::Failed, std::memory_order_release);
                return finish(result);
            }
            const std::array<unsigned char, P::DescriptorBytes> zero{};
            // All stores precede the first literal/helper call. Descriptor
            // address never escapes to a child, so an error cannot create a
            // native storage lease: its ordinary CPU owner may still destroy
            // this unattached object. Shared cache effects remain accepted.
            if (std::memcmp(storage.bytes.data(), zero.data(), zero.size()) || !H::Same(before.names.heap, confirmed.names.heap) ||
                !D::Same(before.world, confirmed.world, false))
            {
                storage.state.store(State::Failed, std::memory_order_release);
                return finish(ClockResult::BindingChanged);
            }
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
            {
                storage.state.store(State::Failed, std::memory_order_release);
                return finish(result);
            }
            storage.state.store(State::Initialized, std::memory_order_release);
            receipt.descriptor = descriptor;
            return finish(ClockResult::Ok);
        }
        catch (...)
        {
            if (receipt.entered)
                storage.state.store(State::Failed, std::memory_order_release);
            return finish(ClockResult::ReadFailure);
        }
    }
}
