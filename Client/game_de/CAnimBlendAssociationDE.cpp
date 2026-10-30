/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual constructor/scalar behavior through one private owner
 ****************************************************************************/
#include "CAnimBlendAssociationDE.h"
#include "CAnimBlendHierarchyDE.h"
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
        float FloatValue(std::uint32_t bits)
        {
            float value;
            static_assert(sizeof(value) == sizeof(bits));
            std::memcpy(&value, &bits, sizeof(value));
            return value;
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
    CAnimBlendAssociationSAInterface* CAnimBlendAssociationDE::Constructor(RpClump* clump, CAnimBlendHierarchySAInterface* hierarchy)
    {
        const auto     token = reference;
        const auto     clumpTag = reinterpret_cast<std::uintptr_t>(clump);
        const auto     hierarchyTag = reinterpret_cast<std::uintptr_t>(hierarchy);
        std::uintptr_t returned = 0;
        Require(native.ApplyPrivateAnimationAssociationClump(token, NativeAnimationAssociationClumpOperation::Construct, clumpTag, hierarchyTag, returned),
                "Constructor(clump)");
        return reinterpret_cast<CAnimBlendAssociationSAInterface*>(returned);
    }
    void CAnimBlendAssociationDE::Init(RpClump* clump, CAnimBlendHierarchySAInterface* hierarchy)
    {
        const auto     token = reference;
        const auto     clumpTag = reinterpret_cast<std::uintptr_t>(clump);
        const auto     hierarchyTag = reinterpret_cast<std::uintptr_t>(hierarchy);
        std::uintptr_t returned = 0;
        // Source Init does not free old nodes or return an interface. Preserve
        // that one operation; the issued owner admits a known compatible phase.
        Require(native.ApplyPrivateAnimationAssociationClump(token, NativeAnimationAssociationClumpOperation::Initialize, clumpTag, hierarchyTag, returned),
                "Init");
    }
    CAnimBlendAssociationSAInterface* CAnimBlendAssociationDE::InitializeForCustomAnimation(RpClump* clump, CAnimBlendHierarchySAInterface* hierarchy)
    {
        const auto     token = reference;
        const auto     clumpTag = reinterpret_cast<std::uintptr_t>(clump);
        const auto     hierarchyTag = reinterpret_cast<std::uintptr_t>(hierarchy);
        std::uintptr_t returned = 0;
        // The same owner journals the13 ordered source defaults then original
        // Init. Frontend scalar stores/free/ref/list helpers would split that
        // admitted operation or discard a prior resource without an ACK.
        Require(
            native.ApplyPrivateAnimationAssociationClump(token, NativeAnimationAssociationClumpOperation::InitializeCustom, clumpTag, hierarchyTag, returned),
            "InitializeForCustomAnimation");
        return reinterpret_cast<CAnimBlendAssociationSAInterface*>(returned);
    }
    void CAnimBlendAssociationDE::FreeAnimBlendNodeArray()
    {
        const auto token = reference;
        // Actual source frees the aligned nodes but leaves pointer/count stale.
        // The same owner must retain that release ACK and protect later node
        // consumers; this CPU observer neither clears fields nor retires this.
        Require(native.FreePrivateAnimationAssociationNodeArray(token), "FreeAnimBlendNodeArray");
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
        const auto     token = reference;
        std::uintptr_t interfaceAddress = 0;
        // Actual MTA exposes its stored interface even before construction.
        // The same causal owner validates allocation identity without reading
        // uninitialized scalar fields or extending native resource lifetime.
        Require(native.ReadPrivateAnimationAssociationInterface(token, interfaceAddress), "GetInterface");
        if (!interfaceAddress)
            Require(ClockResult::BindingChanged, "GetInterface");
        return reinterpret_cast<CAnimBlendAssociationSAInterface*>(interfaceAddress);
    }
    eAnimGroup CAnimBlendAssociationDE::GetAnimGroup()
    {
        return static_cast<eAnimGroup>(Read("GetAnimGroup").animGroup);
    }
    eAnimID CAnimBlendAssociationDE::GetAnimID()
    {
        return static_cast<eAnimID>(Read("GetAnimID").animId);
    }
    std::unique_ptr<CAnimBlendHierarchy> CAnimBlendAssociationDE::GetAnimHierarchy()
    {
        const auto  token = reference;
        auto* const hierarchyOwner = dynamic_cast<NativeAnimationHierarchyAccess*>(&native);
        if (!hierarchyOwner || dynamic_cast<void*>(hierarchyOwner) != dynamic_cast<void*>(&native))
            Require(ClockResult::NotInitialized, "GetAnimHierarchy");
        NativeAnimationHierarchyReferenceToken hierarchy;
        // The dynamic record and its immutable bank-H origin are qualified
        // together by one current owner. Re-reading a raw pointer then doing
        // a separate lookup would not certify that same parent observation.
        Require(native.ResolvePrivateAnimationAssociationHierarchy(token, hierarchy), "GetAnimHierarchy");
        if (hierarchy.ownerGeneration != token.ownerGeneration || !hierarchy.locator)
            Require(ClockResult::BindingChanged, "GetAnimHierarchy");
        // A CPU allocation grants no new native reference or lifetime. Each
        // later hierarchy operation independently re-admits its bank token.
        NativeAnimationAssociationFacadeView childCode;
        if (code.IsIssued())
            Require(code.ForkForHierarchy(token, childCode), "GetAnimHierarchy(code)");
        // The independent CPU child may outlive this facade. Share only the
        // same code fence; native H remains a re-admitted borrowed identity.
        return std::unique_ptr<CAnimBlendHierarchy>(new CAnimBlendHierarchyDE(*hierarchyOwner, hierarchy, std::move(childCode)));
    }
    void CAnimBlendAssociationDE::RestrictToBones(std::bitset<64> mask)
    {
        const auto token = reference;
        const auto bits = static_cast<std::uint64_t>(mask.to_ullong());
        // Copy all source mask bits before callbacks. Only the owner may
        // remove admitted node sequence pointers and persist accepted nulls;
        // this observer has no node layout or allocation/free authority.
        Require(native.RestrictPrivateAnimationAssociationToBones(token, bits), "RestrictToBones");
    }
    void CAnimBlendAssociationDE::RestrictToBonesOf(const CAnimBlendStaticAssociationSAInterface* source)
    {
        const auto token = reference;
        const auto locator = reinterpret_cast<std::uintptr_t>(source);
        // Null and source-no-map are genuine source no-ops after target
        // qualification. Transport the locator unchanged so neither an early
        // return nor client pointer arithmetic bypasses that owner decision.
        Require(native.RestrictPrivateAnimationAssociationToBonesOf(token, locator), "RestrictToBonesOf");
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
    void CAnimBlendAssociationDE::SetCurrentProgress(float progress)
    {
        const auto token = reference;
        const auto bits = FloatBits(progress);
        // Actual MTA multiplies current hierarchy duration by this progress
        // before its native set-time helper. Keep both observations/effects
        // with one owner; no observer-side clamp, default or duration lookup.
        Require(native.SetPrivateAnimationAssociationCurrentProgress(token, bits), "SetCurrentProgress");
    }
    float CAnimBlendAssociationDE::GetCurrentProgress() const
    {
        // Despite the SDK name, actual MTA returns absolute current time.
        // Lua computes its ratio separately; preserve every source float bit.
        return FloatValue(Read("GetCurrentProgress").currentTimeBits);
    }
    float CAnimBlendAssociationDE::GetCurrentSpeed() const
    {
        return FloatValue(Read("GetCurrentSpeed").speedBits);
    }
    float CAnimBlendAssociationDE::GetLength() const
    {
        const auto    token = reference;
        std::uint32_t bits = 0;
        // Current hierarchy time is mutable. Read it with this owned parent
        // under one bank/owner observation rather than caching a constructor
        // value, dividing, or inventing zero for an unsupported missing H.
        Require(native.ReadPrivateAnimationAssociationLength(token, bits), "GetLength");
        return FloatValue(bits);
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
