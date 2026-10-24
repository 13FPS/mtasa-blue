/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA's logical sequence union independently of DE storage
 ****************************************************************************/
#pragma once
#include <cstdint>

namespace mta::game_de
{
    class NativeAnimationSequenceLogicalState final
    {
    public:
        void Initialize() noexcept
        {
            // Classic Initialize writes only the union's low WORD. Keeping
            // the high bits and their definedness avoids inventing a hash
            // from DE's separate sixty-four-byte sequence name.
            raw = (raw & 0xffff0000u) | 0x0000ffffu;
            defined |= 0x0000ffffu;
            boneSelector = false;
        }

        void SetNameHash(std::uint32_t value) noexcept
        {
            // This is the source-owned logical assignment, not a native
            // SetName implementation. The original operation preserves the
            // selector even after replacing the complete union value.
            raw = value;
            defined = 0xffffffffu;
        }

        void SetBoneTag(std::int32_t value) noexcept
        {
            // Only the original full-width -1 is a no-op. Other values,
            // including 65535, set the selector and narrow modulo 2^16.
            if (value == -1)
                return;
            raw = (raw & 0xffff0000u) | static_cast<std::uint16_t>(value);
            defined |= 0x0000ffffu;
            boneSelector = true;
        }

        bool TryReadHash(std::uint32_t& output) const noexcept
        {
            if (defined != 0xffffffffu)
                return false;
            output = raw;
            return true;
        }

        bool TryReadBoneTag(std::uint16_t& output) const noexcept
        {
            if ((defined & 0x0000ffffu) != 0x0000ffffu)
                return false;
            // Actual MTA reads the low WORD even when the selector is clear.
            output = static_cast<std::uint16_t>(raw);
            return true;
        }

        bool          UsesBoneTag() const noexcept { return boneSelector; }
        std::uint32_t DefinedBits() const noexcept { return defined; }

    private:
        // Copying this CPU value transfers no row, keyframe or native lease.
        // Undefined bits are stored but must never be published by a getter.
        std::uint32_t raw = 0, defined = 0;
        bool          boneSelector = false;
    };
}
