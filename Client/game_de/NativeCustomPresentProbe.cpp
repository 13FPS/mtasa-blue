/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Full-signature native presenter policy diagnostics without vcalls
 *****************************************************************************/
#include "NativeCustomPresentProbe.h"
#include <limits>
#include <windows.h>
#include <bcrypt.h>

namespace mta::game_de
{
    namespace
    {
        using Classification = CustomPresentClassificationStatus;
        // All seven slots and complete code envelopes belong to the exact PE
        // admitted by the observer. Shared leaves include the whole instruction,
        // including AB4F10's three-byte ret0; a prologue is not class identity.
        constexpr std::array<CustomPresentSignature, 7> signatures{
            {{0xacb9c0, 66, {0x18, 0x57, 0x59, 0x2d, 0xa0, 0x9f, 0x02, 0x19, 0xdf, 0xbb, 0x41, 0xe8, 0x1b, 0x94, 0xc0, 0x2d,
                             0x62, 0x30, 0x8d, 0xf5, 0x20, 0xe0, 0xbe, 0x2f, 0x5e, 0x13, 0x58, 0xd4, 0x55, 0x41, 0x1e, 0x9d}},
             {0xc20780, 44, {0xf3, 0xb4, 0xa1, 0x87, 0x98, 0xb2, 0x93, 0xc1, 0x2b, 0x91, 0x84, 0xea, 0xef, 0xa2, 0x6a, 0x2b,
                             0x1a, 0xaf, 0xf2, 0x95, 0x43, 0xc7, 0x95, 0x24, 0x61, 0x5f, 0xcf, 0x6f, 0xb4, 0x27, 0x55, 0x40}},
             {0xab6580, 3, {0xe1, 0x8a, 0xac, 0xce, 0x29, 0xaf, 0xfc, 0xdd, 0xda, 0xa5, 0xf6, 0x41, 0xac, 0x1e, 0xc1, 0x25,
                            0x52, 0xe0, 0xfe, 0x9d, 0x06, 0x6e, 0x64, 0x08, 0x94, 0x1b, 0x17, 0x92, 0x81, 0x8b, 0x86, 0x19}},
             {0xac66d0, 3, {0x01, 0xcb, 0x47, 0xd0, 0x78, 0xb4, 0x84, 0x1b, 0x84, 0x08, 0xec, 0x4f, 0xe2, 0x78, 0xef, 0xa8,
                            0x31, 0x15, 0xc6, 0xf6, 0xe1, 0x01, 0x97, 0x29, 0x87, 0x50, 0x7d, 0x2b, 0x2b, 0x57, 0xdc, 0xf0}},
             {0xab4f10, 3, {0xb6, 0xcb, 0xec, 0xec, 0xf1, 0xb0, 0x7e, 0x74, 0xf3, 0x7d, 0x57, 0x71, 0x0c, 0x68, 0xc3, 0x1f,
                            0x0b, 0x92, 0xcb, 0x08, 0xd5, 0x10, 0x93, 0x25, 0x2b, 0x32, 0x1b, 0x07, 0x54, 0x2e, 0x17, 0x7b}},
             {0xc20bf0, 213, {0xeb, 0x9b, 0x8a, 0xbe, 0xfd, 0x6c, 0x0a, 0x75, 0x80, 0x69, 0xf9, 0x92, 0x29, 0xd2, 0x93, 0xdd,
                              0xba, 0xc8, 0x24, 0x32, 0xd9, 0x54, 0x3a, 0x0d, 0x22, 0xd9, 0x1c, 0x9c, 0xc2, 0x63, 0x52, 0xee}},
             {0xab4f10, 3, {0xb6, 0xcb, 0xec, 0xec, 0xf1, 0xb0, 0x7e, 0x74, 0xf3, 0x7d, 0x57, 0x71, 0x0c, 0x68, 0xc3, 0x1f,
                            0x0b, 0x92, 0xcb, 0x08, 0xd5, 0x10, 0x93, 0x25, 0x2b, 0x32, 0x1b, 0x07, 0x54, 0x2e, 0x17, 0x7b}}}};

        bool ReadGuarded(CustomPresentReads& reads, std::uintptr_t address, void* output, std::size_t bytes, bool image, bool executable = false) noexcept
        {
            __try
            {
                return image ? reads.Image(address, output, bytes, executable) : reads.Object(address, output, bytes);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
        bool Hash(const unsigned char* bytes, std::size_t count, std::array<unsigned char, 32>& digest) noexcept
        {
            BCRYPT_ALG_HANDLE  algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            const bool         success = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
                                 BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0 &&
                                 BCryptHashData(hash, const_cast<PUCHAR>(bytes), static_cast<ULONG>(count), 0) >= 0 &&
                                 BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
            if (hash)
                BCryptDestroyHash(hash);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
            return success;
        }
    }

    const std::array<CustomPresentSignature, 7>& CustomPresentSignatures() noexcept
    {
        return signatures;
    }

    void ClassifyCustomPresent(CustomPresentReads& reads, std::uintptr_t image, std::uintptr_t presenter, std::uintptr_t nativeDevice,
                               CustomPresentClassificationState& state, CustomPresentProbeRoots& roots) noexcept
    {
        state = {};
        roots = {};
        roots.presenter = presenter;
        state.status = static_cast<std::uint32_t>(Classification::Absent);
        if (!presenter)
            return;
        state.status = static_cast<std::uint32_t>(Classification::ReadFault);
        if (image > (std::numeric_limits<std::uintptr_t>::max)() - CustomPresentTableRva || presenter > (std::numeric_limits<std::uintptr_t>::max)() - 0x18 ||
            !ReadGuarded(reads, presenter, &roots.table, sizeof(roots.table), false))
            return;
        roots.tableRead = true;
        state.status = static_cast<std::uint32_t>(Classification::UnknownImplementation);
        if (roots.table != image + CustomPresentTableRva)
            return;
        std::array<std::uintptr_t, 7> slots{};
        state.status = static_cast<std::uint32_t>(Classification::ReadFault);
        if (!ReadGuarded(reads, roots.table, slots.data(), sizeof(slots), true))
            return;
        for (std::size_t index = 0; index < signatures.size(); ++index)
        {
            const auto& signature = signatures[index];
            state.status = static_cast<std::uint32_t>(Classification::SignatureMismatch);
            if (image > (std::numeric_limits<std::uintptr_t>::max)() - signature.functionRva || slots[index] != image + signature.functionRva)
                return;
            std::array<unsigned char, 213> code{};
            std::array<unsigned char, 32>  digest{};
            state.status = static_cast<std::uint32_t>(Classification::ReadFault);
            if (!ReadGuarded(reads, slots[index], code.data(), signature.bytes, true, true) || !Hash(code.data(), signature.bytes, digest))
                return;
            state.status = static_cast<std::uint32_t>(Classification::SignatureMismatch);
            if (digest != signature.sha256)
                return;
            state.slotSignatureMask |= 1u << index;
        }
        state.status = static_cast<std::uint32_t>(Classification::ReadFault);
        if (!ReadGuarded(reads, presenter + 0x18, &roots.device, sizeof(roots.device), false))
            return;
        roots.deviceRead = true;
        state.status = static_cast<std::uint32_t>(Classification::DeviceMismatch);
        if (!nativeDevice || roots.device != nativeDevice)
            return;
        // These are static policy facts from the pinned implementation, not
        // executions of its methods or permission to draw through a custom route.
        state.status = static_cast<std::uint32_t>(Classification::KnownNativePresenter);
        state.implementationId = static_cast<std::uint32_t>(CustomPresentImplementation::GamefaceD3D12NativePresenter);
        state.staticPolicyFlags = 15;
    }

    bool CustomPresentRootsUnchanged(CustomPresentReads& reads, const CustomPresentProbeRoots& roots) noexcept
    {
        std::uintptr_t value = 0;
        return (!roots.tableRead || (ReadGuarded(reads, roots.presenter, &value, sizeof(value), false) && value == roots.table)) &&
               (!roots.deviceRead || (ReadGuarded(reads, roots.presenter + 0x18, &value, sizeof(value), false) && value == roots.device));
    }
}
