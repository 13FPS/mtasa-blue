/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual constructor/scalar behavior through one private owner
 ****************************************************************************/
#include "CAnimBlendAssociationDE.h"
#include <game/CAnimBlendAssocGroup.h>
#include <cstring>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace
    {
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE private animation-association owner refused ") + operation + ": " +
                                         std::to_string(static_cast<unsigned>(result)));
        }
        std::uint32_t FloatBits(float value)
        {
            std::uint32_t bits;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }
    }
    CAnimBlendAssociationSAInterface* CAnimBlendAssociationDE::Constructor(CAnimBlendStaticAssociationSAInterface& source)
    {
        const auto     token = reference;
        const auto     locator = reinterpret_cast<std::uintptr_t>(&source);
        std::uintptr_t returned = 0;
        // The legacy reference is only a copied locator. Its owner resolves
        // the genuine current bank row and qualified node map before calling
        // the native constructor once on this explicitly unconstructed record.
        Require(native.ConstructPrivateAnimationAssociationFromStatic(token, locator, returned), "Constructor");
        return reinterpret_cast<CAnimBlendAssociationSAInterface*>(returned);
    }
    NativeAnimationAssociationValues CAnimBlendAssociationDE::Read(const char* operation) const
    {
        const auto                       token = reference;
        NativeAnimationAssociationValues value;
        Require(native.ReadPrivateAnimationAssociation(token, value), operation);
        return value;
    }
    CAnimBlendAssociationSAInterface* CAnimBlendAssociationDE::GetInterface()
    {
        // Only a Ready private record may publish this immediate opaque tag.
        // A CPU observer does not own or extend its native allocation lifetime.
        return reinterpret_cast<CAnimBlendAssociationSAInterface*>(Read("GetInterface").interfaceAddress);
    }
    eAnimGroup CAnimBlendAssociationDE::GetAnimGroup()
    {
        return static_cast<eAnimGroup>(Read("GetAnimGroup").animGroup);
    }
    eAnimID CAnimBlendAssociationDE::GetAnimID()
    {
        return static_cast<eAnimID>(Read("GetAnimID").animId);
    }
    float CAnimBlendAssociationDE::GetBlendAmount()
    {
        const auto bits = Read("GetBlendAmount").blendAmountBits;
        float      value;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    bool CAnimBlendAssociationDE::IsPartial() const
    {
        // Source partialness is bit4 of the raw WORD, not hierarchy shape,
        // blend amount, or the separate native block-reference bit4000.
        return (static_cast<std::uint16_t>(Read("IsPartial").flags) & std::uint16_t{0x10}) != 0;
    }
    void CAnimBlendAssociationDE::SetBlendAmount(float amount)
    {
        const auto token = reference;
        const auto bits = FloatBits(amount);
        Require(native.SetPrivateAnimationAssociationBlendAmount(token, bits), "SetBlendAmount");
    }
    void CAnimBlendAssociationDE::SetCurrentSpeed(float speed)
    {
        const auto token = reference;
        const auto bits = FloatBits(speed);
        // The actual source stores raw float speed. There is no finite,
        // sign or range clamp, native allocation, or time-step update here.
        Require(native.SetPrivateAnimationAssociationCurrentSpeed(token, bits), "SetCurrentSpeed");
    }
    void CAnimBlendAssociationDE::SetAnimID(short id)
    {
        static_assert(sizeof(id) == sizeof(std::int16_t));
        const auto token = reference;
        const auto value = static_cast<std::int16_t>(id);
        Require(native.SetPrivateAnimationAssociationAnimID(token, value), "SetAnimID");
    }
    void CAnimBlendAssociationDE::SetAnimGroup(short group)
    {
        const auto token = reference;
        const auto value = static_cast<std::int16_t>(group);
        Require(native.SetPrivateAnimationAssociationAnimGroup(token, value), "SetAnimGroup");
    }
    void CAnimBlendAssociationDE::SetFlags(short flags)
    {
        const auto token = reference;
        const auto value = static_cast<std::int16_t>(flags);
        // Preserve every caller bit on transport. The owner explicitly
        // refuses unsafe4000 ownership changes rather than silently masking
        // flags, moving references, retrying, or rolling back a lost origin.
        Require(native.SetPrivateAnimationAssociationFlags(token, value), "SetFlags");
    }
}
