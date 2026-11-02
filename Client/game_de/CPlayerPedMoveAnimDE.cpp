/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Forward one copied registration and preserve the native signed group
 ****************************************************************************/
#include "CPlayerPedMoveAnimDE.h"
#include "CPedStateDE.h"
#include "CAnimManagerDE.h"
#include "NativeGameTimingAccess.h"
#include <game/CPlayerMoveAnimPolicy.h>
#include <set>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    eMoveAnim CPlayerPedMoveAnimDE::GetMoveAnim()
    {
        // Match actual CPlayerPedSA's signed int field cast, including values
        // outside MTA's setter policy. Do not substitute the custom CPU cache.
        const auto   token = reference;
        std::int32_t value;
        const auto   result = native.ReadPedMoveAnim(token, value);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE player movement owner refused GetMoveAnim: " + std::to_string(static_cast<unsigned>(result)));
        static_assert(sizeof(int) == sizeof(value));
        return static_cast<eMoveAnim>(value);
    }
    namespace PlayerMoveAnimSetterDetail
    {
        // Classic keeps this name intent for the entire backend module, not
        // per wrapper or temporary block facade. Do not undo it on destruction
        // or on a later native refusal; insertion deliberately precedes AddRef.
        std::set<std::string> doneBlockReferences;
        void                  Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE player movement owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
        }
    }
    CPlayerPedMoveAnimDE::CPlayerPedMoveAnimDE(CWanted* wanted, NativeGameTimingAccess& owner, const NativePedReferenceToken& token, CAnimManagerDE& animations,
                                               CPedStateDE& state)
        : CPlayerPedMoveAnimDE(wanted, static_cast<NativePedMoveAnimAccess&>(owner), token)
    {
        ConfigureMoveAnim(owner, animations, state);
    }
    void CPlayerPedMoveAnimDE::ConfigureMoveAnim(NativeGameTimingAccess& owner, CAnimManagerDE& animations, CPedStateDE& state)
    {
        // These are exact typed endpoint/SDK identities. Do not compare unlike
        // multiple-inheritance subobject addresses or call a foreign facade's
        // virtual GetInterface while trying to establish its backend affinity.
        // Group/static channels are separate Runtime interfaces, not members
        // of NativeGameTimingAccess. Borrow only the complete owner's exact
        // endpoints already held by this manager; do not widen the timing ABI.
        if (&native != static_cast<NativePedMoveAnimAccess*>(&owner) || !reference.ownerGeneration || animations.ownerGeneration != reference.ownerGeneration ||
            &animations.native != static_cast<NativeAnimationManagerAccess*>(&owner) ||
            animations.blockAccess != static_cast<NativeAnimationBlockAccess*>(&owner) || !animations.gameProvider || !animations.groupAccess ||
            animations.groupAccess != dynamic_cast<NativeAnimationAssociationGroupAccess*>(&owner) || !animations.staticAccess ||
            animations.staticAccess != dynamic_cast<NativeAnimationStaticAssociationAccess*>(&owner) || static_cast<CPed*>(&state) != static_cast<CPed*>(this))
            throw std::invalid_argument("DE player movement setter requires its same-wrapper state and same-owner configured animation manager");
        if (movementOwner || animationManager || customState)
            throw std::invalid_argument("DE player movement setter context is already configured");
        movementOwner = &owner;
        animationManager = &animations;
        customState = &state;
    }
    void CPlayerPedMoveAnimDE::RequireMoveAnimServices(const char* operation) const
    {
        if (!movementOwner || !animationManager || !customState || animationManager->ownerGeneration != reference.ownerGeneration ||
            &animationManager->native != static_cast<NativeAnimationManagerAccess*>(movementOwner) ||
            animationManager->blockAccess != static_cast<NativeAnimationBlockAccess*>(movementOwner) || !animationManager->gameProvider ||
            !animationManager->groupAccess || animationManager->groupAccess != dynamic_cast<NativeAnimationAssociationGroupAccess*>(movementOwner) ||
            !animationManager->staticAccess || animationManager->staticAccess != dynamic_cast<NativeAnimationStaticAssociationAccess*>(movementOwner) ||
            static_cast<const CPed*>(customState) != static_cast<const CPed*>(this))
            throw std::runtime_error(std::string("DE player movement setter requires its configured services for ") + operation);
    }
    void CPlayerPedMoveAnimDE::SetMoveAnim(eMoveAnim requested)
    {
        // Share the real classic policy, including invalid/default early exits
        // and both distinct block-facade lifetimes. No replacement movement
        // state machine or eager native write is introduced in this facade.
        struct Operations
        {
            using BlockName = std::string;
            CPlayerPedMoveAnimDE&                         owner;
            NativeAnimationAssociationGroupReferenceToken Group(std::uint32_t group)
            {
                owner.RequireMoveAnimServices("group lookup");
                NativeAnimationAssociationGroupReferenceToken token;
                PlayerMoveAnimSetterDetail::Require(
                    owner.animationManager->groupAccess->ResolveAnimationAssociationGroup(owner.reference.ownerGeneration, group, token), "group lookup");
                if (token.ownerGeneration != owner.reference.ownerGeneration || token.index != group || !token.locator)
                    throw std::runtime_error("DE player movement group lookup changed its requested identity");
                return token;
            }
            void ClearOverride()
            {
                owner.RequireMoveAnimServices("clear override");
                PlayerMoveAnimSetterDetail::Require(owner.native.WithdrawPlayerMoveAnimOverride(owner.reference), "clear override");
                owner.customState->m_iCustomMoveAnim = 0;
            }
            bool IsGroupLoaded(std::uint32_t group)
            {
                const auto                           token = Group(group);
                NativeAnimationAssociationGroupBlock block;
                PlayerMoveAnimSetterDetail::Require(owner.animationManager->groupAccess->ReadAnimationAssociationGroupBlock(token, block),
                                                    "group block presence");
                // The actual classic helper tests this pointer, not the native
                // block's loaded byte or the manager's separate IsLoaded leaf.
                return block.interfaceAddress != 0;
            }
            bool IsGroupValid(std::uint32_t group)
            {
                const auto                           token = Group(group);
                NativeAnimationAssociationGroupBlock block;
                PlayerMoveAnimSetterDetail::Require(owner.animationManager->groupAccess->ReadAnimationAssociationGroupBlock(token, block), "valid group block");
                if (!block.interfaceAddress)
                    return false;
                bool children = false;
                PlayerMoveAnimSetterDetail::Require(owner.animationManager->groupAccess->ReadAnimationAssociationGroupChildArrayPresent(token, children),
                                                    "valid group child-array presence");
                if (!children)
                    return false;
                constexpr std::uint32_t ids[]{0, 1, 2, 3, 5};
                for (const auto id : ids)
                {
                    NativeAnimationStaticAssociationReferenceToken child;
                    PlayerMoveAnimSetterDetail::Require(owner.animationManager->groupAccess->ResolveAnimationAssociationGroupStaticById(token, id, child),
                                                        "valid group ID");
                    NativeAnimationStaticAssociationValues value;
                    PlayerMoveAnimSetterDetail::Require(owner.animationManager->staticAccess->ReadAnimationStaticAssociation(child, value),
                                                        "valid group hierarchy");
                    if (!child.locator || value.interfaceAddress != child.locator || child.ownerGeneration != token.ownerGeneration ||
                        child.groupIndex != token.index)
                        throw std::runtime_error("DE player movement static association changed its group identity");
                    if (!value.hierarchyAddress)
                        return false;
                }
                return true;
            }
            std::unique_ptr<CAnimBlock> GetAnimationBlock(const BlockName& name)
            {
                owner.RequireMoveAnimServices("block lookup");
                // The explicit typed manager affinity was checked before any
                // effects; reuse its existing SDK factory/request contract.
                return owner.animationManager->GetAnimationBlock(name.c_str());
            }
            bool HasBlockReference(const BlockName& name)
            {
                return PlayerMoveAnimSetterDetail::doneBlockReferences.find(name) != PlayerMoveAnimSetterDetail::doneBlockReferences.end();
            }
            void RecordBlockReference(const BlockName& name) { PlayerMoveAnimSetterDetail::doneBlockReferences.insert(name); }
            void StoreOverride(std::uint32_t group)
            {
                owner.RequireMoveAnimServices("store override");
                PlayerMoveAnimSetterDetail::Require(owner.native.EnablePlayerMoveAnimHooks(), "enable movement hooks");
                PlayerMoveAnimSetterDetail::Require(owner.native.PublishPlayerMoveAnimOverride(owner.reference, static_cast<std::int32_t>(group)),
                                                    "publish movement override");
                // Publication contains no throwing work after commit. Keep the
                // one real protected CPedState value synchronized immediately.
                owner.customState->m_iCustomMoveAnim = static_cast<int>(group);
            }
            void StoreNativeGroup(std::uint32_t group)
            {
                PlayerMoveAnimSetterDetail::Require(owner.native.WriteCurrentPlayerMoveAnimGroup(owner.reference, static_cast<std::int32_t>(group)),
                                                    "write current movement group");
            }
            void Reapply() { PlayerMoveAnimSetterDetail::Require(owner.native.ReapplyCurrentPlayerMoveAnim(owner.reference), "reapply movement group"); }
        } operations{*this};
        MtaGame::CPlayerMoveAnimPolicy::SetMoveAnim(static_cast<std::uint32_t>(requested), operations);
    }

}
