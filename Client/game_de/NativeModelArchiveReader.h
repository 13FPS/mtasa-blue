/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate genuine DE named archive-membership observations
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
namespace mta::game_de
{
    class NativeModelArchiveReader final
    {
    public:
        using Binding = NativeModelBankReader::Binding;
        using CheckOwner = NativeModelBankReader::CheckOwner;
        struct Value
        {
            std::uint32_t  modelIndex = 0;
            std::uintptr_t slotAddress = 0, nativeModelInfo = 0, flagsAddress = 0;
            unsigned char  rawFlags = 0;
            bool           flagsObserved = false, allocatedInArchive = false;
        };
        NativeModelArchiveReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(std::uint32_t fullModelKey, Value&) const noexcept;
        ClockResult Recheck(const Value&) const noexcept;
        // Exactly IS_MODEL_IN_CDIMAGE07DE: nonnull native bank word and
        // clear native flag80. The word stays opaque, loaded state is not
        // the predicate, and no classic physical block count is invented.
        // A null bank short-circuits the flags read as the genuine query does.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
