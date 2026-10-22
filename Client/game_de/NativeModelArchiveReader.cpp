/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse real bank/raw flags and borrowed owner for native archive facts
 ****************************************************************************/
#include "NativeModelArchiveReader.h"
#include "NativeModelArchiveProfile.h"
#include "NativeModelLoadedReader.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <array>
#include <cstring>
#include <initializer_list>
namespace mta::game_de
{
    namespace ModelArchiveDetail
    {
        namespace P = NativeModelArchiveProfile;
        bool Same(const NativeModelArchiveReader::Value& a, const NativeModelArchiveReader::Value& b) noexcept
        {
            return a.modelIndex == b.modelIndex && a.slotAddress == b.slotAddress && a.nativeModelInfo == b.nativeModelInfo &&
                   a.flagsAddress == b.flagsAddress && a.rawFlags == b.rawFlags && a.flagsObserved == b.flagsObserved &&
                   a.allocatedInArchive == b.allocatedInArchive;
        }
        ClockResult Owner(NativeModelArchiveReader::Binding binding, NativeModelArchiveReader::CheckOwner owner) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = owner();
            if (result != ClockResult::Ok || (result = NativeStartGameCalls::CheckThread(binding.image)) != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(binding.image, state))
                return ClockResult::BindingChanged;
            return state == 9 ? owner() : ClockResult::NotInitialized;
        }
    }
    bool NativeModelArchiveReader::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativeModelBankReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativeModelArchiveReader::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativeModelArchiveProfile;
        if (!NativeModelLoadedReader::Revalidate(binding))
            return false;
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() ||
                !NativeImageAccess::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        std::uintptr_t group = 0;
        std::uint32_t  branch = 0;
        return NativeImageAccess::ReadImage(binding.image, binding.image + P::GroupSlotRva, &group, sizeof(group)) && group == binding.image + P::GroupRva &&
               NativeImageAccess::ReadImage(binding.image, binding.image + P::BranchTableRva + P::BranchIndex * 4, &branch, sizeof(branch)) &&
               branch == P::BranchRva;
    }
    ClockResult NativeModelArchiveReader::Read(std::uint32_t model, Value& output) const noexcept
    {
        namespace P = NativeModelArchiveProfile;
        namespace D = ModelArchiveDetail;
        // Preserve the full SDK key. The script's negative/special model
        // remapping is separate state and is not an unsigned bank selector.
        if (model >= P::ModelCount)
            return ClockResult::InvalidArgument;
        auto result = D::Owner(binding, owner);
        if (result != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::BindingChanged;
        NativeModelBankReader   bank(binding, owner);
        NativeModelLoadedReader loaded(binding, owner);
        Value                   first, second;
        for (auto* next : {&first, &second})
        {
            NativeModelBankReader::Value word;
            if ((result = bank.Read(model, word)) != ClockResult::Ok)
                return result;
            next->modelIndex = model;
            next->slotAddress = word.slotAddress;
            next->nativeModelInfo = word.nativeModelInfo;
            if (word.nativeModelInfo)
            {
                NativeModelLoadedReader::Value stream;
                if ((result = loaded.Read(model, stream)) != ClockResult::Ok)
                    return result;
                next->flagsObserved = true;
                next->flagsAddress = stream.rowAddress + P::FlagsOffset;
                next->rawFlags = stream.rawFlags;
                next->allocatedInArchive = (stream.rawFlags & P::MissingArchiveBit) == 0;
            }
            // The real named query only compares this word with zero. Its
            // metadata and native provider are never dereferenced or queried
            // through a mutating loader merely to answer archive membership.
            if ((result = bank.Recheck(word)) != ClockResult::Ok)
                return result;
        }
        if (!D::Same(first, second) || !Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativeModelArchiveReader::Recheck(const Value& previous) const noexcept
    {
        Value      next;
        const auto result = Read(previous.modelIndex, next);
        return result != ClockResult::Ok ? result : ModelArchiveDetail::Same(previous, next) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}
