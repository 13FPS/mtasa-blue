/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep native name construction and actual clothes effects in one call
 ****************************************************************************/
#include "NativePedClothesCalls.h"
#include "NativePedClothesProfile.h"
#include "NativeNameConstructionContext.h"
#include "NativePedMetadataMemory.h"
#include "NativePlayerInfoProfile.h"
#include "NativeStartGameCalls.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de::NativePedClothesCalls
{
    namespace
    {
        namespace P = NativePedClothesProfile;
        namespace N = NativeNameConstructionContext;
        namespace D = NativeStreamingCompletionContext;
        namespace M = NativeModelReferenceMap;
        namespace H = NativeModelReferenceHeap;
        static_assert(P::ClothesSlotRva == NativePlayerInfoProfile::BankRva + 16);
        bool Profile(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, P::MaximumPinBytes> bytes{};
            for (const auto& pin : P::Pins)
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            std::uintptr_t group = 0;
            std::uint32_t  branch = 0;
            return NativeImageAccess::ReadImage(image, image + P::GroupSlotRva, &group, sizeof(group)) && group == image + P::GroupRva &&
                   NativeImageAccess::ReadImage(image, image + P::TableSlotRva, &branch, sizeof(branch), false, true) && branch == P::BranchRva &&
                   D::Profile(image);
        }
        struct Base
        {
            D::Value                         world;
            NativePedMetadata                metadata;
            NativePlayerInfoMemory::Identity player;
            std::uintptr_t                   clothes = 0;
        };
        ClockResult Capture(std::uintptr_t image, const Ped& ped, const NativePlayerInfoMemory& playerInfo, CheckOwner owner, Base& output) noexcept
        {
            Base value;
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            if ((result = NativePedPoolReader({image}, owner).Recheck(ped)) != ClockResult::Ok ||
                (result = NativePedMetadataMemory({image}, owner).Read(ped, value.metadata)) != ClockResult::Ok ||
                (result = playerInfo.ReadIdentity(value.player, owner)) != ClockResult::Ok)
                return result;
            // ReadIdentity already validates the genuine class/control method,
            // bankrow0, active selector and ped+5f0 inline-data relationship.
            // A writable arbitrary data pointer cannot replace that producer.
            if (value.player.image != image || value.player.ped != ped.nativePed || value.metadata.entityType != 3)
                return ClockResult::BindingChanged;
            if (!NativeImageAccess::ReadImage(image, image + P::ClothesSlotRva, &value.clothes, sizeof(value.clothes), true))
                return ClockResult::ReadFailure;
            if (value.clothes && (value.clothes % alignof(std::uintptr_t) || !M::PrivateWritable(value.clothes, P::ClothesBytes) ||
                                  !M::Disjoint(value.clothes, P::ClothesBytes, ped.nativePed, NativePedPoolProfile::ObjectStride)))
                return ClockResult::BindingChanged;
            if ((result = D::Capture(image, value.world)) != ClockResult::Ok || (result = NativePedPoolReader({image}, owner).Recheck(ped)) != ClockResult::Ok)
                return result;
            if (!D::Fence(image, value.world) || !Profile(image))
                return ClockResult::BindingChanged;
            if ((result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            output = value;
            return ClockResult::Ok;
        }
        bool Same(const Base& a, const Base& b, bool modes) noexcept
        {
            return a.player.image == b.player.image && a.player.ped == b.player.ped && a.clothes == b.clothes &&
                   a.metadata.modelIndex == b.metadata.modelIndex && a.metadata.entityType == b.metadata.entityType && D::Same(a.world, b.world, modes);
        }
        struct Intent
        {
            std::array<char, P::MaximumNameBytes> texture{}, model{};
            std::uint16_t                         textureBytes = 0, modelBytes = 0;
        };
        ClockResult CopyName(const char* input, std::array<char, P::MaximumNameBytes>& output, std::uint16_t& bytes) noexcept
        {
            bytes = 0;
            if (!input)
                return ClockResult::Ok;
            const auto begin = reinterpret_cast<std::uintptr_t>(input);
            if (begin > (std::numeric_limits<std::uintptr_t>::max)() - output.size())
                return ClockResult::InvalidArgument;
            for (std::size_t index = 0; index < output.size(); ++index)
            {
                unsigned char byte = 0;
                if (!N::CallerByte(begin + index, byte))
                    return ClockResult::ReadFailure;
                // The real signed-byte CRT toupper cannot establish an
                // arbitrary high-byte domain. NameEntry's bounded branch is
                // length<1024; no case/suffix rewriting is done by MTA here.
                if (byte >= 0x80)
                    return ClockResult::UnsupportedBuild;
                output[index] = static_cast<char>(byte);
                if (!byte)
                {
                    bytes = static_cast<std::uint16_t>(index + 1);
                    return ClockResult::Ok;
                }
            }
            return ClockResult::UnsupportedBuild;
        }
#if !defined(MTA_DE_PED_CLOTHES_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uintptr_t clothes, const char* texture, const char* model, std::uint8_t part,
                               std::uint32_t& exception) noexcept
        {
            __try
            {
                function(clothes, texture, model, static_cast<std::int32_t>(part));
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
    ClockResult Set(std::uintptr_t image, const Ped& input, const NativePlayerInfoMemory& playerInfo, const char* texture, const char* model, std::uint8_t part,
                    CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        const Ped ped = input;
        Receipt   receipt;
        receipt.entryRva = P::EntryRva;
        receipt.nativePed = ped.nativePed;
        receipt.handle = ped.handle;
        receipt.part = part;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        try
        {
            Base before, confirmed;
            auto result = Capture(image, ped, playerInfo, owner, before);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.playerData = image + NativePlayerInfoProfile::BankRva + 8;
            receipt.clothes = before.clothes;
            receipt.root = before.world.root;
            receipt.gameMode = before.world.gameMode;
            receipt.level = before.world.level;
            receipt.world = before.world.world;
            // Match classic ordering exactly: absence does not evaluate even
            // invalid caller string pointers or an out-of-range texture part.
            if (!before.clothes)
            {
                receipt.nullClothes = true;
                if ((result = Capture(image, ped, playerInfo, owner, confirmed)) != ClockResult::Ok)
                    return finish(result);
                return finish(Same(before, confirmed, true) ? ClockResult::Ok : ClockResult::BindingChanged);
            }
            if (part >= P::TextureParts)
                return finish(ClockResult::InvalidArgument);
            Intent intent;
            if ((result = CopyName(texture, intent.texture, intent.textureBytes)) != ClockResult::Ok ||
                (result = CopyName(model, intent.model, intent.modelBytes)) != ClockResult::Ok)
                return finish(result);
            receipt.textureBytes = intent.textureBytes;
            receipt.modelBytes = intent.modelBytes;
            const bool needsNames = intent.textureBytes || intent.modelBytes;
            N::Names   names, confirmedNames;
            if (needsNames)
            {
                if (!N::Profile(image))
                    return finish(ClockResult::BindingChanged);
                if ((result = N::ReadNames(image, names)) != ClockResult::Ok)
                    return finish(result);
            }
            if ((result = Capture(image, ped, playerInfo, owner, confirmed)) != ClockResult::Ok)
                return finish(result);
            if (!Same(before, confirmed, true))
                return finish(ClockResult::BindingChanged);
            if (needsNames)
            {
                if ((result = N::ReadNames(image, confirmedNames)) != ClockResult::Ok)
                    return finish(result);
                if (!N::Same(names, confirmedNames) || !N::Profile(image))
                    return finish(ClockResult::BindingChanged);
            }
            if ((result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return finish(result);
#if defined(MTA_DE_PED_CLOTHES_NATIVE_INVOKE)
            const auto entry = invoke ? invoke : &MTA_DE_PED_CLOTHES_NATIVE_INVOKE;
#else
            const auto entry = invoke ? invoke : &NativeCall;
#endif
            receipt.entered = true;
            result = entry(reinterpret_cast<NativeFunction>(image + P::EntryRva), before.clothes, intent.textureBytes ? intent.texture.data() : nullptr,
                           intent.modelBytes ? intent.model.data() : nullptr, part, receipt.exceptionCode);
            if (result != ClockResult::Ok)
                return finish(result);
            receipt.returned = true;
            if ((result = Capture(image, ped, playerInfo, owner, confirmed)) != ClockResult::Ok)
                return finish(result);
            if (!Same(before, confirmed, false))
                return finish(ClockResult::BindingChanged);
            if (needsNames)
            {
                if ((result = N::ReadNames(image, confirmedNames)) != ClockResult::Ok)
                    return finish(result);
                // The actual call owns name-cache growth/interning and both
                // descriptor writes. Accept those effects; retain only the
                // existing heap/owner identity, never roll them back or retry.
                if (!H::Same(names.heap, confirmedNames.heap) || !N::Profile(image))
                    return finish(ClockResult::BindingChanged);
            }
            return finish(NativeStartGameCalls::CheckThread(image));
        }
        catch (...)
        {
            return finish(ClockResult::ReadFailure);
        }
    }
}
