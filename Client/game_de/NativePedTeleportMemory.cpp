/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve one native teleport action and its already-entered effects
 ****************************************************************************/
#include "NativePedTeleportMemory.h"
#include "NativePedPoseMutation.h"
#include "NativePedPoseContext.h"
#include "NativePedTransformProfile.h"
#include <cmath>
#include <windows.h>
namespace mta::game_de
{
    namespace PedTeleportDetail
    {
#if !defined(MTA_DE_PED_TELEPORT_NATIVE_INVOKE)
        ClockResult NativeCall(NativePedTeleportMemory::NativeFunction function, void* ped, const CVector* value, bool reset, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(ped, value, reset);
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
    ClockResult NativePedTeleportMemory::Apply(std::uint32_t handle, const CVector& input, Receipt* receiptOutput, Invoke invoke) const noexcept
    {
        namespace D = NativePedPoseContext;
        Receipt receipt;
        receipt.handle = handle;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        // Adapter placement and native teleport share one callback-reentry
        // boundary. This does not pin the engine receiver or serialize its
        // internal calls; all original immediate graph checks remain required.
        NativePedPoseMutation scope;
        if (!scope)
            return finish(ClockResult::Busy);
        auto result = D::Owner(binding, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!D::Read(reinterpret_cast<std::uintptr_t>(&input), &receipt.requested, sizeof(receipt.requested)) || !std::isfinite(receipt.requested.fX) ||
            !std::isfinite(receipt.requested.fY) || !std::isfinite(receipt.requested.fZ))
            return finish(ClockResult::InvalidArgument);
        try
        {
            D::Context before;
            result = D::Capture(binding, owner, handle, before);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.nativePed = before.transform.ped.nativePed;
            receipt.intelligence = before.intelligence;
            receipt.matrixBefore = before.transform.matrix;
            receipt.entry = binding.image + NativePedTransformProfile::NativeTeleportRva;
            result = D::Owner(binding, owner);
            if (result != ClockResult::Ok)
                return finish(result);
            if (!D::Pins(binding))
                return finish(ClockResult::BindingChanged);
            result = D::Fence(binding, owner, before);
            if (result != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_PED_TELEPORT_NATIVE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_PED_TELEPORT_NATIVE_INVOKE;
#else
            const auto entry = invoke ? invoke : &PedTeleportDetail::NativeCall;
#endif
            receipt.entered = true;
            result = entry(reinterpret_cast<NativeFunction>(receipt.entry), reinterpret_cast<void*>(receipt.nativePed), &receipt.requested, true,
                           receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.returned = true;
            result = D::Owner(binding, owner);
            if (result != ClockResult::Ok)
                return finish(result);
            D::Context after;
            result = D::Capture(binding, owner, handle, after);
            if (result != ClockResult::Ok)
                return finish(result);
            // Real native effects may replace matrices/tasks/list entries or
            // advance an allocator cursor. Qualify fresh dependencies instead
            // of demanding equality with the pre-call world graph.
            if (!D::SameLifetime(before, after))
                return finish(ClockResult::BindingChanged);
            receipt.matrixAfter = after.transform.matrix;
            receipt.afterObserved = true;
            return finish(D::Owner(binding, owner));
        }
        catch (...)
        {
            // A post-entry observation/allocation failure is still a failed
            // acknowledgement of an entered native action, never a rollback
            // lease or permission to retry its effects.
            return finish(ClockResult::ReadFailure);
        }
    }
}
