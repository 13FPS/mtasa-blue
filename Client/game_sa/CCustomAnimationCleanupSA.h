/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve classic cleanup receipts on the creating manager
 ****************************************************************************/
#pragma once
#include "CAnimBlendHierarchySA.h"
#include "CAnimSequenceStorageSA.h"
#include <game/CAnimManager.h>
#include <game/CCustomAnimationCleanup.h>
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace MtaGame::CustomAnimationCleanupSA
{
    class LifetimeLock final
    {
    public:
        explicit LifetimeLock(SRWLOCK& value) noexcept : value(value) { AcquireSRWLockExclusive(&value); }
        ~LifetimeLock() { ReleaseSRWLockExclusive(&value); }
        LifetimeLock(const LifetimeLock&) = delete;
        LifetimeLock& operator=(const LifetimeLock&) = delete;

    private:
        SRWLOCK& value;
    };
    enum class Progress
    {
        Waiting,
        Entered,
        Returned
    };
    using FrameIdentity = std::shared_ptr<void>;
    struct Release
    {
        void*         address = nullptr;
        Progress      progress = Progress::Waiting;
        FrameIdentity identity;
    };
    struct Record
    {
        CAnimBlendHierarchySAInterface* hierarchy = nullptr;
        CAnimBlendSequenceSAInterface*  sequences = nullptr;
        std::size_t                     capacity = 0;
        bool                            cacheWasAttached = false;
        bool                            begun = false;
        bool                            gathered = false;
        bool                            active = false;
        bool                            complete = false;
        Progress                        cache = Progress::Waiting;
        Release                         array;
        Release                         shared;
        Release                         outer;
        std::vector<Release>            individuals;
        // Snapshot actual original frame records before array claims can
        // close. Historical identity survives native free/address reuse.
        std::vector<Release> extraFrames;
        void*                frameContext = nullptr;
        FrameIdentity (*identifyFrame)(void*, void*) noexcept = nullptr;
        ECustomAnimationCleanupStatus (*releaseFrame)(void*, const FrameIdentity&, void*) noexcept = nullptr;
    };
    struct Owner
    {
        explicit Owner(CAnimManager& manager) noexcept : liveOrigin(&manager) {}
        // The manager owns this journal independently of CPU ticket facades.
        // Its actual module/manager teardown admission must keep pending work
        // alive; clearing this pointer only prevents calls after owner death.
        CAnimManager*                        liveOrigin;
        const DWORD                          creatorThread = GetCurrentThreadId();
        mutable SRWLOCK                      codeLifetimeLock = SRWLOCK_INIT;
        bool                                 codeLifetimeUncertain = false;
        std::size_t                          liveTickets = 0;
        std::size_t                          activeBegins = 0;
        bool                                 creatorClosed = false;
        std::vector<std::shared_ptr<Record>> records;

        bool HasPending() const noexcept
        {
            // Teardown cannot wait for a worker or a reentrant holder to
            // finish backend code. Missing immediate proof means retention.
            if (GetCurrentThreadId() != creatorThread || !TryAcquireSRWLockExclusive(&codeLifetimeLock))
                return true;
            struct Unlock
            {
                SRWLOCK& value;
                ~Unlock() { ReleaseSRWLockExclusive(&value); }
            } unlock{codeLifetimeLock};
            if (codeLifetimeUncertain)
                return true;
            // A completed ticket still has game_sa virtual code. The actual
            // Core teardown gate must keep its module and manager alive too.
            if (liveTickets || activeBegins)
                return true;
            for (const auto& record : records)
                if (!record->complete || record->active)
                    return true;
            return false;
        }
    };

    inline std::shared_ptr<Record> Reserve(Owner& owner, CAnimBlendHierarchySAInterface* hierarchy)
    {
        if (GetCurrentThreadId() != owner.creatorThread || !owner.liveOrigin || owner.creatorClosed || !hierarchy)
            throw std::invalid_argument("Classic hierarchy reservation requires its live creator");
        auto record = std::make_shared<Record>();
        record->hierarchy = hierarchy;
        owner.records.push_back(record);
        return record;
    }

    inline void CancelUnexposed(Owner& owner, const std::shared_ptr<Record>& record) noexcept
    {
        // Only the factory's still-unexposed allocation can take this path.
        // It never cancels a cleanup command or infers a native free receipt.
        const auto found = std::find(owner.records.begin(), owner.records.end(), record);
        if (found != owner.records.end() && !record->begun && !record->active)
            owner.records.erase(found);
    }

    inline void Gather(Record& record)
    {
        std::set<void*> individuals;
        void*           shared = nullptr;
        if (record.sequences)
        {
            std::size_t actualCapacity = 0;
            // This is the actual classic allocator's private cookie, not
            // common-client row arithmetic or adoption of an arbitrary tag.
            std::memcpy(&actualCapacity, reinterpret_cast<const unsigned char*>(record.sequences) - CAnimSequenceStorageSA::CookieBytes,
                        CAnimSequenceStorageSA::CookieBytes);
            if (actualCapacity != record.capacity || (record.capacity && record.capacity - 1 > (std::numeric_limits<DWORD>::max)()))
                throw std::runtime_error("Classic cleanup array capacity changed");
            for (std::size_t index = 0; index != record.capacity; ++index)
            {
                auto* row = CAnimSequenceStorageSA::At(record.sequences, static_cast<std::uint32_t>(index));
                if (!row)
                    throw std::runtime_error("Classic cleanup capacity row unavailable");
                if (row->sFlags & 8)
                {
                    // Only row zero identifies the original shared chunk;
                    // later flagged rows may point inside that allocation.
                    if (!index)
                        shared = row->pKeyFrames;
                }
                else if (row->pKeyFrames)
                    individuals.insert(row->pKeyFrames);
            }
        }
        else if (record.capacity)
            throw std::runtime_error("Classic cleanup has capacity without storage");
        individuals.erase(shared);
        std::map<void*, Release> originals;
        const auto               merge = [&](void* address, const FrameIdentity& identity)
        {
            if (!address)
                return;
            auto found = originals.find(address);
            if (found == originals.end())
                originals.emplace(address, Release{address, Progress::Waiting, identity});
            else if (found->second.identity && identity && (found->second.identity.owner_before(identity) || identity.owner_before(found->second.identity)))
                throw std::runtime_error("Classic cleanup original frame identity changed");
            else if (!found->second.identity)
                found->second.identity = identity;
        };
        // Extra records are actual backend originals, including a historical
        // frame which no physical row currently names. Their opaque identities
        // carry the real free ACK; generic progress cannot invent completion.
        for (const auto& frames : record.extraFrames)
            merge(frames.address, frames.identity);
        for (auto* address : individuals)
            merge(address, {});
        merge(shared, {});
        for (auto& original : originals)
            if (record.identifyFrame)
                merge(original.first, record.identifyFrame(record.frameContext, original.first));
        std::vector<Release> snapshot;
        snapshot.reserve(originals.size());
        Release sharedSnapshot;
        for (const auto& original : originals)
        {
            if (original.first == shared)
                sharedSnapshot = original.second;
            else
                snapshot.push_back(original.second);
        }
        // All potentially allocating observations and frame identities finish
        // before cache/release entry or removal of the array's owner claims.
        // A failed gather leaves the original request available for retry.
        record.individuals.swap(snapshot);
        record.shared = std::move(sharedSnapshot);
        record.array.address = record.sequences;
        record.outer.address = record.hierarchy;
        record.gathered = true;
    }

    template <class Operation>
    bool Run(Progress& progress, Operation operation)
    {
        if (progress == Progress::Returned)
            return true;
        if (progress == Progress::Entered)
            return false;
        // An exception after an effect is not a release acknowledgement.
        // Leave Entered durable and never replay that void operation.
        progress = Progress::Entered;
        operation();
        progress = Progress::Returned;
        return true;
    }

    inline ECustomAnimationCleanupStatus RunFrame(Record& record, Release& frame, CAnimManager& manager)
    {
        if (frame.progress == Progress::Returned)
            return ECustomAnimationCleanupStatus::Complete;
        if (frame.progress == Progress::Entered)
            return ECustomAnimationCleanupStatus::Refused;
        if (frame.identity)
        {
            if (!record.releaseFrame)
                return ECustomAnimationCleanupStatus::Refused;
            // The same SA allocation owner arbitrates related participants
            // and remembers entered/returned release. Pending is a known
            // wait, and calling this callback again never retries a raw free.
            const auto status = record.releaseFrame(record.frameContext, frame.identity, frame.address);
            if (status == ECustomAnimationCleanupStatus::Complete)
                frame.progress = Progress::Returned;
            if (status == ECustomAnimationCleanupStatus::Complete || status == ECustomAnimationCleanupStatus::Pending ||
                status == ECustomAnimationCleanupStatus::Refused)
                return status;
            return ECustomAnimationCleanupStatus::Refused;
        }
        // Existing source-owned buffers and the strict isolated classic
        // receiver have no shared frame producer; retain their exact prior
        // entered/returned void boundary instead of fabricating an identity.
        return Run(frame.progress, [&] { manager.FreeKeyFramesMemory(frame.address); }) ? ECustomAnimationCleanupStatus::Complete
                                                                                        : ECustomAnimationCleanupStatus::Refused;
    }

    inline SCustomAnimationCleanupResult Resume(Owner& owner, Record& record) noexcept
    {
        constexpr auto disposition = ECustomAnimationCleanupDisposition::ClassicExternalChildren;
        if (record.complete)
            return {ECustomAnimationCleanupStatus::Complete, disposition};
        if (GetCurrentThreadId() != owner.creatorThread || !owner.liveOrigin)
            return {ECustomAnimationCleanupStatus::Refused, disposition};
        if (record.active)
            return {ECustomAnimationCleanupStatus::Pending, disposition};
        record.active = true;
        struct ActiveGuard
        {
            bool& active;
            ~ActiveGuard() { active = false; }
        } guard{record.active};
        try
        {
            if (!record.gathered)
                Gather(record);
            auto& manager = *owner.liveOrigin;
            if (record.hierarchy && record.sequences && record.cacheWasAttached &&
                !Run(record.cache, [&] { manager.RemoveFromUncompressedCache(record.hierarchy); }))
                return {ECustomAnimationCleanupStatus::Refused, disposition};
            for (auto& frames : record.individuals)
            {
                const auto status = RunFrame(record, frames, manager);
                if (status != ECustomAnimationCleanupStatus::Complete)
                    return {status, disposition};
            }
            if (record.array.address &&
                !Run(record.array.progress, [&] { manager.FreeCustomAnimSequences(static_cast<CAnimBlendSequenceSAInterface*>(record.array.address)); }))
                return {ECustomAnimationCleanupStatus::Refused, disposition};
            if (record.shared.address)
            {
                const auto status = RunFrame(record, record.shared, manager);
                if (status != ECustomAnimationCleanupStatus::Complete)
                    return {status, disposition};
            }
            if (record.outer.address &&
                !Run(record.outer.progress,
                     [&] { manager.DeleteCustomAnimHierarchyInterface(static_cast<CAnimBlendHierarchySAInterface*>(record.outer.address)); }))
                return {ECustomAnimationCleanupStatus::Refused, disposition};
            record.complete = true;
            return {ECustomAnimationCleanupStatus::Complete, disposition};
        }
        catch (...)
        {
            return {ECustomAnimationCleanupStatus::Refused, disposition};
        }
    }

    class Ticket final : public CCustomAnimationCleanup
    {
    public:
        Ticket(std::shared_ptr<Owner> owner, std::shared_ptr<Record> record) noexcept : owner(std::move(owner)), record(std::move(record))
        {
            LifetimeLock lock(this->owner->codeLifetimeLock);
            ++this->owner->liveTickets;
        }
        ~Ticket() override
        {
            LifetimeLock lock(owner->codeLifetimeLock);
            // A worker's last CPU-view decrement is not proof that its DLL
            // destructor code has returned. Publish permanent uncertainty
            // before decrement, so creator-thread teardown cannot unload it.
            if (GetCurrentThreadId() != owner->creatorThread)
                owner->codeLifetimeUncertain = true;
            --owner->liveTickets;
        }
        SCustomAnimationCleanupResult Resume() noexcept override { return CustomAnimationCleanupSA::Resume(*owner, *record); }

    private:
        std::shared_ptr<Owner>  owner;
        std::shared_ptr<Record> record;
    };

    inline std::unique_ptr<CCustomAnimationCleanup> Begin(const std::shared_ptr<Owner>& owner, const SCustomAnimationCleanupRequest& request,
                                                          CAnimBlendHierarchySAInterface* hierarchy)
    {
        if (!owner || GetCurrentThreadId() != owner->creatorThread || !owner->liveOrigin || request.origin != owner->liveOrigin)
            throw std::invalid_argument("Classic cleanup requires the creating manager");
        {
            LifetimeLock lock(owner->codeLifetimeLock);
            ++owner->activeBegins;
        }
        struct BeginGuard
        {
            Owner& owner;
            ~BeginGuard()
            {
                LifetimeLock lock(owner.codeLifetimeLock);
                --owner.activeBegins;
            }
        } guard{*owner};
        if (owner->creatorClosed)
            throw std::runtime_error("Classic cleanup creator is closed");
        // Completed receipts remain valid in their existing tickets, but may
        // not authorize a later allocation merely reusing an old raw address.
        owner->records.erase(std::remove_if(owner->records.begin(), owner->records.end(), [](const auto& record) { return record->complete; }),
                             owner->records.end());
        for (const auto& record : owner->records)
        {
            if (hierarchy && record->hierarchy == hierarchy && !record->begun)
            {
                // The factory reserved this CPU record before exposing the
                // outer interface. Ticket OOM cannot lose its original debt.
                record->sequences = request.originalArray;
                record->capacity = request.capacity;
                record->cacheWasAttached = request.cacheWasAttached;
                record->begun = true;
                return std::make_unique<Ticket>(owner, record);
            }
            if (record->hierarchy == hierarchy && record->sequences == request.originalArray && record->capacity == request.capacity &&
                record->cacheWasAttached == request.cacheWasAttached)
                return std::make_unique<Ticket>(owner, record);
            if (!record->complete && ((hierarchy && record->hierarchy == hierarchy) || (request.originalArray && record->sequences == request.originalArray)))
                throw std::runtime_error("Classic cleanup already owns a different request for this resource");
        }
        if (hierarchy || request.originalArray || request.capacity)
            throw std::invalid_argument("Classic cleanup requires a reserved custom hierarchy from this manager");
        auto record = std::make_shared<Record>();
        record->hierarchy = hierarchy;
        record->sequences = request.originalArray;
        record->capacity = request.capacity;
        record->cacheWasAttached = request.cacheWasAttached;
        record->begun = true;
        owner->records.push_back(record);
        // Persist before allocating the facade. If that allocation fails, a
        // later Begin recovers the same record instead of duplicating frees.
        return std::make_unique<Ticket>(owner, std::move(record));
    }
}
