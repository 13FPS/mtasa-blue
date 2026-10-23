/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Refuse overlapping adapter placement and native teleport callbacks
 ****************************************************************************/
#pragma once
namespace mta::game_de
{
    namespace PedPoseMutationDetail
    {
        inline thread_local bool active = false;
    }
    // This only serializes adapter entry on the current admitted thread. It
    // neither locks the engine nor pins a ped/matrix across native callbacks.
    class NativePedPoseMutation final
    {
    public:
        NativePedPoseMutation() noexcept : held(!PedPoseMutationDetail::active)
        {
            if (held)
                PedPoseMutationDetail::active = true;
        }
        ~NativePedPoseMutation()
        {
            if (held)
                PedPoseMutationDetail::active = false;
        }
        NativePedPoseMutation(const NativePedPoseMutation&) = delete;
        NativePedPoseMutation& operator=(const NativePedPoseMutation&) = delete;
        explicit               operator bool() const noexcept { return held; }

    private:
        const bool held;
    };
}
