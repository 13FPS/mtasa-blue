/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Inject only owned-buffer read/bookend failures around real field CAS
 ****************************************************************************/
#pragma once
#ifndef MTA_DE_OBJECT_PROPERTIES_FIXTURE_PARENT_READ
    #define MTA_DE_OBJECT_PROPERTIES_FIXTURE_PARENT_READ ::ClockObjectGroupOwnerTest::Read
    #define MTA_DE_OBJECT_PROPERTIES_FIXTURE_LOCAL_PARENT_READ
#endif
namespace ClockObjectPropertiesOwnerTest
{
    std::uintptr_t row = 0, worldMode = 0;
    std::uint32_t  committedMass = 0;
    bool           failAfterCommitted = false, failPrefix = false, injected = false;
    unsigned       restoredReads = 0;
    BOOL WINAPI    Read(HANDLE process, LPCVOID address, LPVOID output, SIZE_T bytes, SIZE_T* copied) noexcept
    {
        const auto result = MTA_DE_OBJECT_PROPERTIES_FIXTURE_PARENT_READ(process, address, output, bytes, copied);
        if (!result || reinterpret_cast<std::uintptr_t>(address) != row || bytes != 0x58 || !copied || *copied != bytes)
            return result;
        std::array<std::uint32_t, 5> observed{};
        std::memcpy(observed.data(), output, sizeof(observed));
        if (failAfterCommitted && !injected && observed[0] == committedMass)
        {
            // The production32-bit CAS committed. Lose a separate typed world
            // raw anchor afterwards; the original native function is not run.
            const unsigned char invalidMode = 3;
            std::memcpy(reinterpret_cast<void*>(worldMode), &invalidMode, sizeof(invalidMode));
            injected = true;
        }
        if (failPrefix && !injected && observed[0] == 0x3f800000 && observed[1] != 0x40000000 && ++restoredReads == 3)
        {
            // Two successful field reads seal the first post-CAS Sample. The
            // next row read is the following field's pre-entry Capture. Failing
            // there proves that a known acknowledged prefix survives refusal.
            injected = true;
            *copied = 0;
            return FALSE;
        }
        return result;
    }
}
#if defined(MTA_DE_OBJECT_PROPERTIES_FIXTURE_LOCAL_PARENT_READ)
    #undef MTA_DE_OBJECT_PROPERTIES_FIXTURE_LOCAL_PARENT_READ
    #undef MTA_DE_OBJECT_PROPERTIES_FIXTURE_PARENT_READ
#endif
