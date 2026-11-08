/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Detect an actual first vector CAS before injecting one lost bookend
 *****************************************************************************/
#pragma once
namespace ClockModelDummiesOwnerTest
{
    inline std::uintptr_t visual = 0, providerCount = 0;
    inline std::uint32_t  committedBits = 0;
    inline bool           failAfterCommitted = false, injected = false;
    inline BOOL           Read(HANDLE process, LPCVOID address, LPVOID destination, SIZE_T bytes, SIZE_T* copied) noexcept
    {
        const BOOL result = ClockObjectGroupOwnerTest::Read(process, address, destination, bytes, copied);
        if (result && visual && providerCount && failAfterCommitted && !injected)
        {
            std::uint32_t observed = 0;
            std::memcpy(&observed, reinterpret_cast<void*>(visual), sizeof(observed));
            if (observed == committedBits)
            {
                // This runs only after the shipping component CAS has changed
                // owned native storage. No shipping callback or fake exchange
                // reports success on the primitive's behalf.
                const std::int32_t changed = 11;
                std::memcpy(reinterpret_cast<void*>(providerCount), &changed, sizeof(changed));
                injected = true;
            }
        }
        return result;
    }
}
