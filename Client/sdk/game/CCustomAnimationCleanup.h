/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit same-backend cleanup without guessing detach/free success
 ****************************************************************************/
#pragma once
#include <cstddef>
#include <cstdint>
class CAnimManager;
class CAnimBlendHierarchy;
class CAnimBlendSequenceSAInterface;

enum class ECustomAnimationCleanupStatus : std::uint32_t
{
    Pending,
    Refused,
    Complete
};
enum class ECustomAnimationCleanupDisposition : std::uint32_t
{
    Unresolved,
    ClassicExternalChildren,
    PrivateDetached
};
struct SCustomAnimationCleanupResult
{
    ECustomAnimationCleanupStatus      status = ECustomAnimationCleanupStatus::Pending;
    ECustomAnimationCleanupDisposition disposition = ECustomAnimationCleanupDisposition::Unresolved;
};
struct SCustomAnimationCleanupRequest
{
    CAnimManager* origin = nullptr;
    // Borrowed only during Begin: the provider copies its own CPU identity,
    // never retains this facade pointer or adopts an arbitrary bank locator.
    CAnimBlendHierarchy*           hierarchy = nullptr;
    CAnimBlendSequenceSAInterface* originalArray = nullptr;
    std::size_t                    capacity = 0;
    // The classic source cache itinerary uses this hint. It is never a DE
    // attach/detach receipt; only the actual owner journal proves that state.
    bool cacheWasAttached = false;
};
class CCustomAnimationCleanup
{
public:
    // This facade is CPU-only. The original backend owner retains the whole
    // snapshot and outstanding receipts if the last facade reference drops.
    virtual ~CCustomAnimationCleanup() = default;
    // A private backend may queue/resume on its natural callback. A returned
    // Complete requires actual per-resource acknowledgements, never absence.
    virtual SCustomAnimationCleanupResult Resume() noexcept = 0;
};
// Required appended CAnimManager method, with no classic default fallback:
// virtual std::unique_ptr<CCustomAnimationCleanup>
//     BeginCustomAnimationCleanup(const SCustomAnimationCleanupRequest&) = 0;
