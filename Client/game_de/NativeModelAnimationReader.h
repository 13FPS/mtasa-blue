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
        // The native leaf reads QWORD6c0. Bookend all64 bits, then preserve
        // the EAX intent used by RequestModel; never follow the sampled tag.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
