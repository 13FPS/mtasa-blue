/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep standalone construction and transfer on its actual SA owner
 ****************************************************************************/
#pragma once
#include "CCustomAnimationCleanupSA.h"
#include <game/CCustomAnimationSequenceOperation.h>
#include <set>

namespace MtaGame::CustomAnimationSequenceSA
{
    using Progress = CustomAnimationCleanupSA::Progress;
    using LifetimeLock = CustomAnimationCleanupSA::LifetimeLock;
    struct Row;
    struct Command;
    struct Frame
    {
        void*          address = nullptr;
        bool           entered = false, returned = false, fault = false;
        Progress       release = Progress::Waiting;
        std::set<Row*> participants;
    };
    struct Array
    {
        CAnimBlendSequenceSAInterface* address = nullptr;
        std::size_t                    capacity = 0;
        bool                           released = false, closing = false;
    };
    struct Row
    {
        CAnimBlendSequenceSAInterface* address = nullptr;
        // The issuing path sets this purpose. Neither a one-row allocation
        // nor a raw locator can acquire standalone deletion authority.
        bool                                standalone = false, initialized = false, closing = false, fault = false;
        Progress                            release = Progress::Waiting;
        std::shared_ptr<Array>              array;
        std::shared_ptr<Frame>              current;
        std::vector<std::shared_ptr<Frame>> historical;
        std::weak_ptr<Command>              pending;
    };
    enum class Kind
    {
        Cleanup,
        Transfer,
        Attachment
    };
    struct Command
    {
        Kind                           kind = Kind::Cleanup;
        std::shared_ptr<Row>           source, target;
        std::shared_ptr<Frame>         frame;
        std::size_t                    count = 0;
        bool                           root = false, compressed = false, active = false, terminal = false;
        bool                           addedTargetHistory = false, addedTargetParticipant = false, cancelledTransfer = false;
        std::shared_ptr<Command>       predecessor;
        Progress                       effect = Progress::Waiting;
        SCustomAnimationSequenceResult result;
    };
    struct Owner
    {
        using Attach = void (*)(CAnimBlendSequenceSAInterface*, std::size_t, bool, bool, void*);
        explicit Owner(CAnimManager& manager, Attach attach) noexcept : manager(&manager), attach(attach) {}
        CAnimManager*                         manager;
        Attach                                attach;
        const DWORD                           creatorThread = GetCurrentThreadId();
        mutable SRWLOCK                       lock = SRWLOCK_INIT;
        bool                                  closed = false, foreignDrop = false, pumping = false;
        std::size_t                           views = 0, calls = 0;
        std::vector<std::shared_ptr<Frame>>   frames;
        std::vector<std::shared_ptr<Array>>   arrays;
        std::vector<std::shared_ptr<Row>>     rows;
        std::vector<std::shared_ptr<Command>> commands;
        bool                                  HasPending() const noexcept
        {
            if (GetCurrentThreadId() != creatorThread || !TryAcquireSRWLockExclusive(&lock))
                return true;
            struct Unlock
            {
                SRWLOCK& lock;
                ~Unlock() { ReleaseSRWLockExclusive(&lock); }
            } unlock{lock};
            if (foreignDrop || views || calls)
                return true;
            for (const auto& frame : frames)
                if ((frame->entered && !frame->returned) || (frame->address && frame->release != Progress::Returned))
                    return true;
            for (const auto& array : arrays)
                if (!array->released)
                    return true;
            for (const auto& row : rows)
                if (row->standalone && row->release != Progress::Returned)
                    return true;
            for (const auto& command : commands)
                if (!command->terminal || command->active)
                    return true;
            return false;
        }
    };
    inline void Admit(Owner& owner)
    {
        if (GetCurrentThreadId() != owner.creatorThread || !owner.manager || owner.closed)
            throw std::runtime_error("Classic sequence operation creator is unavailable");
    }
    class CallGuard final
    {
    public:
        CallGuard(Owner& owner, bool recovery = false) : owner(owner)
        {
            if (GetCurrentThreadId() != owner.creatorThread || !owner.manager || (owner.closed && !recovery))
                throw std::runtime_error("Classic sequence owner call has no creator admission");
            LifetimeLock lock(owner.lock);
            ++owner.calls;
        }
        ~CallGuard()
        {
            LifetimeLock lock(owner.lock);
            --owner.calls;
        }

    private:
        Owner& owner;
    };
    inline std::shared_ptr<Frame> FindFrame(Owner& owner, void* address)
    {
        for (auto it = owner.frames.rbegin(); it != owner.frames.rend(); ++it)
            if ((*it)->address == address && (*it)->returned && (*it)->release != Progress::Returned)
                return *it;
        return {};
    }
    inline std::shared_ptr<Array> FindArray(Owner& owner, CAnimBlendSequenceSAInterface* address)
    {
        for (auto it = owner.arrays.rbegin(); it != owner.arrays.rend(); ++it)
            if ((*it)->address == address && !(*it)->released)
                return *it;
        return {};
    }
    inline std::shared_ptr<Row> FindRow(Owner& owner, CAnimBlendSequenceSAInterface* address)
    {
        for (auto it = owner.rows.rbegin(); it != owner.rows.rend(); ++it)
            if ((*it)->address == address && (*it)->release != Progress::Returned && (!(*it)->array || !(*it)->array->released))
                return *it;
        return {};
    }
    inline std::shared_ptr<Frame> ReserveFrame(Owner& owner)
    {
        Admit(owner);
        auto frame = std::make_shared<Frame>();
        owner.frames.push_back(frame);
        return frame;
    }
    inline void RegisterArray(Owner& owner, CAnimBlendSequenceSAInterface* address, std::size_t capacity)
    {
        Admit(owner);
        if (!address)
            return;
        auto array = std::make_shared<Array>();
        array->address = address;
        array->capacity = capacity;
        owner.arrays.push_back(array);
    }
    inline std::shared_ptr<Row> RegisterStandalone(Owner& owner, CAnimBlendSequenceSAInterface* address)
    {
        Admit(owner);
        auto row = std::make_shared<Row>();
        row->address = address;
        row->standalone = true;
        owner.rows.push_back(row);
        return row;
    }
    inline std::shared_ptr<Row> RegisterBorrowedRow(Owner& owner, const std::shared_ptr<Array>& array, CAnimBlendSequenceSAInterface* address)
    {
        Admit(owner);
        if (auto row = FindRow(owner, address))
            return row;
        auto row = std::make_shared<Row>();
        row->address = address;
        row->array = array;
        row->initialized = true;  // Actual array allocator initializes capacity.
        owner.rows.push_back(row);
        return row;
    }
    inline void BeforeInitialize(Owner& owner, CAnimBlendSequenceSAInterface* address)
    {
        Admit(owner);
        if (auto row = FindRow(owner, address))
        {
            const auto pending = row->pending.lock();
            if (row->closing || (pending && !pending->terminal))
                throw std::runtime_error("Classic sequence initialization has an accepted owner command");
        }
    }
    inline void Initialized(Owner& owner, CAnimBlendSequenceSAInterface* address) noexcept
    {
        if (GetCurrentThreadId() != owner.creatorThread)
            return;
        if (auto row = FindRow(owner, address))
        {
            row->initialized = true;
            row->current.reset();
            // Actual Initialize clears the row pointer, not ownership of a
            // previously accepted original allocation. Keep historical debt.
        }
    }
    inline bool ReleaseReady(void* context, void* address) noexcept
    {
        auto& owner = *static_cast<Owner*>(context);
        auto  frame = FindFrame(owner, address);
        return !frame || (!frame->fault && frame->participants.empty());
    }
    inline CustomAnimationCleanupSA::FrameIdentity IdentifyFrame(void* context, void* address) noexcept
    {
        return FindFrame(*static_cast<Owner*>(context), address);
    }
    inline void                          Pump(Owner& owner) noexcept;
    inline ECustomAnimationCleanupStatus ReleaseFrame(void* context, const CustomAnimationCleanupSA::FrameIdentity& identity, void* address) noexcept
    {
        auto&      owner = *static_cast<Owner*>(context);
        const auto frame = std::static_pointer_cast<Frame>(identity);
        if (!frame || frame->address != address || GetCurrentThreadId() != owner.creatorThread || !owner.manager)
            return ECustomAnimationCleanupStatus::Refused;
        // Only Resume calls this release callback. Progress related accepted
        // cleanup/cancellation commands here, never from Begin or HasPending.
        Pump(owner);
        if (frame->release == Progress::Returned)
            return ECustomAnimationCleanupStatus::Complete;
        if (frame->fault || frame->release == Progress::Entered)
            return ECustomAnimationCleanupStatus::Refused;
        if (!frame->participants.empty())
            return ECustomAnimationCleanupStatus::Pending;
        if (FindFrame(owner, address) != frame)
            return ECustomAnimationCleanupStatus::Refused;
        try
        {
            owner.manager->FreeKeyFramesMemory(address);
        }
        catch (...)
        {
            return ECustomAnimationCleanupStatus::Refused;
        }
        return frame->release == Progress::Returned ? ECustomAnimationCleanupStatus::Complete : ECustomAnimationCleanupStatus::Refused;
    }
    inline std::vector<CustomAnimationCleanupSA::Release> SnapshotArrayFrames(Owner& owner, CAnimBlendSequenceSAInterface* address)
    {
        const auto                                     array = FindArray(owner, address);
        std::vector<CustomAnimationCleanupSA::Release> snapshot;
        if (!array)
            return snapshot;
        std::set<Frame*> seen;
        for (const auto& row : owner.rows)
            if (row->array == array)
                for (const auto& frame : row->historical)
                    if (seen.insert(frame.get()).second && frame->release != Progress::Returned)
                    {
                        CustomAnimationCleanupSA::Release release;
                        release.address = frame->address;
                        release.identity = frame;
                        snapshot.push_back(std::move(release));
                    }
        return snapshot;
    }
    inline void CloseArrayClaims(Owner& owner, CAnimBlendSequenceSAInterface* address) noexcept
    {
        const auto array = FindArray(owner, address);
        if (!array)
            return;
        array->closing = true;
        for (const auto& row : owner.rows)
            if (row->array == array)
            {
                row->closing = true;
                if (auto command = row->pending.lock(); command && !command->terminal)
                {
                    if (command->effect == Progress::Waiting)
                    {
                        if (command->kind == Kind::Attachment)
                        {
                            command->result = {ECustomAnimationSequenceStatus::Refused, ECustomAnimationSequenceDisposition::Unresolved};
                            command->terminal = true;
                        }
                        else if (command->kind == Kind::Transfer)
                        {
                            // The original transfer command, not a new raw-tag
                            // lookup, now owns full source cleanup on cancel.
                            command->cancelledTransfer = true;
                            if (command->frame && command->addedTargetParticipant)
                                command->frame->participants.erase(command->target.get());
                        }
                    }
                    else if (command->effect == Progress::Entered)
                    {
                        row->fault = command->source->fault = true;
                        if (command->frame)
                            command->frame->fault = true;
                    }
                }
                for (const auto& frame : row->historical)
                    frame->participants.erase(row.get());
            }
    }
    inline SCustomAnimationSequenceResult Resume(Owner& owner, Command& command) noexcept
    {
        const auto refused = SCustomAnimationSequenceResult{ECustomAnimationSequenceStatus::Refused, ECustomAnimationSequenceDisposition::Unresolved};
        if (command.terminal)
            return command.result;
        if (GetCurrentThreadId() != owner.creatorThread || !owner.manager)
            return refused;
        if (command.active)
            return {};
        command.active = true;
        struct Guard
        {
            bool& active;
            ~Guard() { active = false; }
        } guard{command.active};
        try
        {
            auto& source = *command.source;
            if (command.predecessor && !command.predecessor->terminal)
            {
                auto& prior = *command.predecessor;
                if (prior.effect == Progress::Waiting)
                {
                    // Cleanup cancels only a provably unentered construction
                    // intent. Accepted/unknown writes are never rolled back.
                    if (prior.kind == Kind::Transfer && prior.frame)
                    {
                        if (prior.addedTargetParticipant)
                            prior.frame->participants.erase(prior.target.get());
                        if (prior.addedTargetHistory)
                            prior.target->historical.erase(std::remove(prior.target->historical.begin(), prior.target->historical.end(), prior.frame),
                                                           prior.target->historical.end());
                    }
                    if (prior.kind == Kind::Transfer)
                    {
                        prior.cancelledTransfer = true;
                        const auto result = Resume(owner, prior);
                        if (result.status != ECustomAnimationSequenceStatus::Complete)
                            return result;
                        command.result = result;
                        command.terminal = true;
                        return result;
                    }
                    prior.result = refused;
                    prior.terminal = true;
                }
                else
                {
                    const auto result = Resume(owner, prior);
                    if (result.status != ECustomAnimationSequenceStatus::Complete)
                        return result;
                    if (result.disposition == ECustomAnimationSequenceDisposition::SourceTransferred)
                    {
                        command.result = result;
                        command.terminal = true;
                        return result;
                    }
                }
            }
            if (source.release == Progress::Returned)
            {
                command.result = {ECustomAnimationSequenceStatus::Complete, ECustomAnimationSequenceDisposition::SourceReleased};
                command.terminal = true;
                return command.result;
            }
            if (source.fault || (command.target && command.target->fault))
                return refused;
            if (command.kind == Kind::Attachment)
            {
                if (command.effect == Progress::Entered)
                    return refused;
                if (command.effect == Progress::Waiting)
                {
                    if (!source.initialized || source.address->pKeyFrames || source.address->sNumKeyFrames || !owner.attach)
                        return refused;
                    command.effect = Progress::Entered;
                    owner.attach(source.address, command.count, command.root, command.compressed, command.frame->address);
                    command.effect = Progress::Returned;
                    source.current = command.frame;
                    if (source.address->pKeyFrames != command.frame->address)
                    {
                        source.fault = command.frame->fault = true;
                        return refused;
                    }
                }
                command.result = {ECustomAnimationSequenceStatus::Complete, ECustomAnimationSequenceDisposition::FramesAttached};
            }
            else
            {
                if (command.kind == Kind::Transfer && !command.cancelledTransfer && command.effect != Progress::Returned)
                {
                    if (command.effect == Progress::Entered || !source.initialized || source.closing || command.target->closing)
                        return refused;
                    command.effect = Progress::Entered;
                    // This is the exact existing SA CopySequenceProperties
                    // assignment; it never frees target or source frames.
                    *command.target->address = *source.address;
                    command.effect = Progress::Returned;
                    command.target->current = source.current;
                    if (source.current)
                    {
                        source.current->participants.erase(&source);
                        source.historical.erase(std::remove(source.historical.begin(), source.historical.end(), source.current), source.historical.end());
                        source.current.reset();
                    }
                }
                source.closing = true;
                for (const auto& frame : source.historical)
                    frame->participants.erase(&source);
                for (const auto& frame : source.historical)
                {
                    if (frame->fault || !frame->participants.empty())
                        return {};
                    if (frame->release == Progress::Entered)
                        return refused;
                    if (frame->release != Progress::Returned)
                        owner.manager->FreeKeyFramesMemory(frame->address);
                }
                if (source.release == Progress::Entered)
                    return refused;
                if (source.release != Progress::Returned)
                    owner.manager->DeleteCustomAnimSequenceInterface(source.address);
                command.result = {ECustomAnimationSequenceStatus::Complete, command.kind == Kind::Transfer && !command.cancelledTransfer
                                                                                ? ECustomAnimationSequenceDisposition::SourceTransferred
                                                                                : ECustomAnimationSequenceDisposition::SourceReleased};
            }
            command.terminal = true;
            return command.result;
        }
        catch (...)
        {
            // A source setter/copy/free that entered without returning is
            // never replayed or guessed from a frame-pointer comparison.
            command.source->fault = true;
            if (command.target)
                command.target->fault = true;
            if (command.frame)
                command.frame->fault = true;
            return refused;
        }
    }
    inline void Pump(Owner& owner) noexcept
    {
        if (owner.pumping || GetCurrentThreadId() != owner.creatorThread || !owner.manager)
            return;
        owner.pumping = true;
        const auto count = owner.commands.size();
        for (std::size_t index = 0; index != count; ++index)
        {
            const auto command = owner.commands[index];
            if (!command->active && !command->terminal)
                Resume(owner, *command);
        }
        owner.pumping = false;
    }
    class Ticket final : public CCustomAnimationSequenceOperation
    {
    public:
        explicit Ticket(std::shared_ptr<Owner> owner) noexcept : owner(std::move(owner))
        {
            LifetimeLock lock(this->owner->lock);
            ++this->owner->views;
        }
        ~Ticket() override
        {
            LifetimeLock lock(owner->lock);
            if (GetCurrentThreadId() != owner->creatorThread)
                owner->foreignDrop = true;
            --owner->views;
        }
        void                           Adopt(std::shared_ptr<Command> value) noexcept { command = std::move(value); }
        SCustomAnimationSequenceResult Resume() noexcept override
        {
            const auto result = command ? CustomAnimationSequenceSA::Resume(*owner, *command) : SCustomAnimationSequenceResult{};
            Pump(*owner);
            return result;
        }

    private:
        std::shared_ptr<Owner>   owner;
        std::shared_ptr<Command> command;
    };
    inline std::unique_ptr<CCustomAnimationSequenceOperation> Begin(const std::shared_ptr<Owner>& owner, Kind kind,
                                                                    CAnimBlendSequenceSAInterface* sourceAddress,
                                                                    CAnimBlendSequenceSAInterface* targetAddress = nullptr, std::size_t count = 0,
                                                                    bool root = false, bool compressed = false, void* original = nullptr)
    {
        CallGuard call(*owner);
        auto      source = FindRow(*owner, sourceAddress);
        auto      target = targetAddress ? FindRow(*owner, targetAddress) : std::shared_ptr<Row>{};
        if (!source || source->closing || (kind != Kind::Attachment && !source->standalone) ||
            (kind == Kind::Transfer && (!target || target == source || target->closing)))
            throw std::invalid_argument("Classic sequence operation requires its issued source and target roles");
        auto ticket = std::make_unique<Ticket>(owner);
        auto predecessor = source->pending.lock();
        if (predecessor && !predecessor->terminal && predecessor->kind == Kind::Cleanup && kind == Kind::Cleanup)
        {
            ticket->Adopt(std::move(predecessor));
            return ticket;
        }
        if (predecessor && !predecessor->terminal && kind != Kind::Cleanup)
            throw std::runtime_error("Classic source already has an accepted construction command");
        if (target)
            if (auto pending = target->pending.lock(); pending && !pending->terminal)
                throw std::runtime_error("Classic target already has an accepted construction command");
        auto command = std::make_shared<Command>();
        if (predecessor && !predecessor->terminal)
            command->predecessor = predecessor;
        command->kind = kind;
        command->source = source;
        command->target = target;
        command->count = count;
        command->root = root;
        command->compressed = compressed;
        auto receiver = kind == Kind::Transfer ? target : source;
        auto frame = kind == Kind::Attachment ? FindFrame(*owner, original) : source->current;
        if (kind == Kind::Attachment && !frame)
            throw std::invalid_argument("Classic assignment requires its current original frame allocation");
        auto           history = receiver->historical;
        std::set<Row*> participants;
        if (frame)
        {
            participants = frame->participants;
            command->addedTargetParticipant = participants.insert(receiver.get()).second;
            if (std::find(history.begin(), history.end(), frame) == history.end())
            {
                history.push_back(frame);
                command->addedTargetHistory = true;
            }
        }
        owner->commands.push_back(command);
        // All CPU allocation precedes acceptance. From this point no throwing
        // copy or facade allocation can lose an original frame obligation.
        if (frame)
        {
            receiver->historical.swap(history);
            frame->participants.swap(participants);
        }
        command->frame = frame;
        source->pending = command;
        if (target)
            target->pending = command;
        ticket->Adopt(std::move(command));
        return ticket;
    }
}
