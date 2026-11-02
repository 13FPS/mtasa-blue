/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Publish only a current same-owner borrowed SDK task manager
 ****************************************************************************/
#include "CPedIntelligenceDE.h"
// The existing timing-owner interface imports genuine Windows/vector SDK
// declarations normally supplied by the classic PCH; provide that context here.
#include <windows.h>
#include <SharedUtil.Defines.h>
#include "CTaskManagerDE.h"
#include "NativeGameTimingAccess.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace PedIntelligenceSdkDetail
    {
        bool Same(const NativePedPoolReader::Value& a, const NativePedPoolReader::Value& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        bool Same(const NativePedIntelligenceReader::Value& a, const NativePedIntelligenceReader::Value& b) noexcept
        {
            // Task cells remain opaque comparison words. Even unchanged cells
            // are not dereferenced and provide no task publication authority.
            return Same(a.ped, b.ped) && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativeIntelligence == b.nativeIntelligence &&
                   a.taskManager == b.taskManager && a.primaryTasks == b.primaryTasks && a.secondaryTasks == b.secondaryTasks && a.slot == b.slot &&
                   a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation && a.ownsAllocations == b.ownsAllocations &&
                   a.rawType == b.rawType;
        }
    }
    void CPedIntelligenceDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped intelligence owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    bool CPedIntelligenceDE::HasManagerBinding() const noexcept
    {
        return &manager.access == static_cast<NativeTaskRemovalAccess*>(&native) && manager.token.version == reference.version &&
               manager.token.ownerGeneration == reference.ownerGeneration && manager.token.slot == reference.slot;
    }
    CPedIntelligenceDE::CPedIntelligenceDE(NativeGameTimingAccess& owner, const NativePedReferenceToken& token, CTaskManagerDE& selected)
        : native(owner), reference(token), manager(selected)
    {
        Require(reference.version && reference.ownerGeneration && reference.slot != MtaGame::CPoolSlotPolicy::InvalidSlot ? ClockResult::Ok
                                                                                                                          : ClockResult::InvalidArgument,
                "constructor");
        // Reject a different typed interface subobject or CPU token before
        // consulting any native state. A generic manager pointer cannot prove
        // this same-owner relation and is deliberately not accepted.
        Require(HasManagerBinding() ? ClockResult::Ok : ClockResult::OwnerConflict, "constructor");
    }
    CTaskManager* CPedIntelligenceDE::GetTaskManager()
    {
        Require(HasManagerBinding() ? ClockResult::Ok : ClockResult::OwnerConflict, "GetTaskManager");
        NativePedIntelligenceReader::Value before, after;
        Require(native.ReadReferencedPedIntelligence(reference, before), "GetTaskManager");
        Require(before.ped.nativePed && before.nativeIntelligence && before.taskManager ? ClockResult::Ok : ClockResult::NotInitialized, "GetTaskManager");
        Require(native.ReadReferencedPedIntelligence(reference, after), "GetTaskManager");
        Require(PedIntelligenceSdkDetail::Same(before, after) && HasManagerBinding() ? ClockResult::Ok : ClockResult::BindingChanged, "GetTaskManager");
        return &manager;
    }
}
