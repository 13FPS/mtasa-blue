/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep source scalar semantics distinct from owner/read refusals
 ****************************************************************************/
#include "CAnimBlendAssocGroupDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace
    {
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE animation association-group owner refused ") + operation + ": " +
                                         std::to_string(static_cast<unsigned>(result)));
        }
    }
    NativeAnimationAssociationGroupValues CAnimBlendAssocGroupDE::Read(const char* operation) const
    {
        const auto                            token = reference;
        NativeAnimationAssociationGroupValues value;
        Require(native.ReadAnimationAssociationGroup(token, value), operation);
        return value;
    }
    bool CAnimBlendAssocGroupDE::IsCreated()
    {
        // Source4D37A0 tests the whole count against zero, independently of
        // child pointers, loaded state and the sign of the preserved bits.
        return Read("IsCreated").numAnimations != 0;
    }
    int CAnimBlendAssocGroupDE::GetNumAnimations()
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        return Read("GetNumAnimations").numAnimations;
    }
    eAnimGroup CAnimBlendAssocGroupDE::GetGroupID()
    {
        static_assert(std::is_same_v<std::underlying_type_t<eAnimGroup>, int>);
        // The actual source casts its raw signed32 field to this fixed enum.
        // Preserve out-of-enum bits; a manager's separate enum gate is not
        // part of an existing group's GetGroupID operation.
        return static_cast<eAnimGroup>(Read("GetGroupID").groupId);
    }
    bool CAnimBlendAssocGroupDE::IsLoaded()
    {
        const auto token = reference;
        bool       value = false;
        Require(native.ReadAnimationAssociationGroupLoaded(token, value), "IsLoaded");
        return value;
    }
    CAnimBlendAssocGroupSAInterface* CAnimBlendAssocGroupDE::GetInterface()
    {
        // Common MTA consumers compare or round-trip this backend tag; they
        // do not dereference the classic structure. This current borrow
        // asserts neither an SA layout nor a new native lifetime lease.
        return reinterpret_cast<CAnimBlendAssocGroupSAInterface*>(Read("GetInterface").interfaceAddress);
    }
}
