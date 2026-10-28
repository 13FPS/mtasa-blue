/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current qualified animation-file getter observation without invocation
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de
{
    class NativeModelAnimationReader final
    {
    public:
        using Binding = NativeModelBankReader::Binding;
        using CheckOwner = NativeModelBankReader::CheckOwner;
        NativeModelAnimationReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(std::uint32_t fullModelKey, std::uint32_t& animationFileIndex) const noexcept;
        // Qualify the current vehicle6c0 or genuine ped/clump68 path before
        // reading its fullQWORD. Bookend all64 bits, then preserve RequestModel's
        // EAX intent; never follow the sampled tag or invoke either getter.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
