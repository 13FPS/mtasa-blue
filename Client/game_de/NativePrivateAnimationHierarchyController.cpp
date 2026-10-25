/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep private CPU hierarchy publication debt with its runtime owner
 ****************************************************************************/
#include "NativePrivateAnimationHierarchyController.h"
#include <algorithm>
#include <iterator>
#include <cstring>
namespace mta::game_de
{
    namespace PrivateHierarchyControllerDetail
    {
        struct Scope
        {
            std::atomic_flag& operation;
            ~Scope() { operation.clear(std::memory_order_release); }
        };
        namespace Calls = NativePrivateAnimationHierarchyCalls;
    }
    NativePrivateAnimationHierarchyController::Entries::iterator NativePrivateAnimationHierarchyController::Find(const Token& token) noexcept
    {
        return std::find_if(entries.begin(), entries.end(), [&](const Entry& entry)
                            { return entry.issued && entry.storage.OwnerGeneration() == token.ownerGeneration && entry.storage.Address() == token.locator; });
    }
    bool NativePrivateAnimationHierarchyController::Owns(const Token& token) const noexcept
    {
        return std::any_of(entries.begin(), entries.end(), [&](const Entry& entry)
                           { return entry.issued && entry.storage.OwnerGeneration() == token.ownerGeneration && entry.storage.Address() == token.locator; });
    }
    void NativePrivateAnimationHierarchyController::Publish() noexcept
    {
        if (entries.empty())
            stopping = false;
        retained.store(!entries.empty(), std::memory_order_release);
        const bool fault =
            std::any_of(entries.begin(), entries.end(),
                        [](const Entry& entry)
                        {
                            const auto state = entry.storage.GetState();
                            return state == Storage::State::Failed || state == Storage::State::PublicationPinned || state == Storage::State::Entered;
                        });
        recovery.store(!entries.empty() && (stopping || fault), std::memory_order_release);
    }
    ClockResult NativePrivateAnimationHierarchyController::Create(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, Token& output,
                                                                  Receipt* receipt) noexcept
    {
        const auto requested = generation;
        if (receipt)
            *receipt = {};
        if (!requested)
            return ClockResult::InvalidArgument;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope scope{operation};
        if (stopping || (!entries.empty() && entries.front().storage.OwnerGeneration() != requested))
            return ClockResult::OwnerConflict;
        try
        {
            entries.emplace_back(requested);
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        const auto entry = std::prev(entries.end());
        Receipt    resultReceipt;
        const auto result = PrivateHierarchyControllerDetail::Calls::Initialize(image, entry->storage, requested, owner, &resultReceipt);
        if (result == ClockResult::Ok)
        {
            entry->issued = true;
            Token next;
            next.ownerGeneration = requested;
            next.locator = entry->storage.Address();
            output = next;
        }
        else if (!resultReceipt.entered)
            entries.erase(entry);
        // An entered failed CPU record remains owned and cannot reinitialize
        // or replay. The ordinary empty-neverpublished retire path owns it.
        if (receipt)
            *receipt = resultReceipt;
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::Initialize(std::uintptr_t image, const Token& value, CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto token = value;
        if (receipt)
            *receipt = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope scope{operation};
        if (stopping)
            return ClockResult::OwnerConflict;
        const auto entry = Find(token);
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        const auto result = PrivateHierarchyControllerDetail::Calls::Initialize(image, entry->storage, token.ownerGeneration, owner, receipt);
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::SetScalar(std::uintptr_t image, const Token& value,
                                                                     NativePrivateAnimationHierarchyCalls::ScalarOperation kind, std::uint32_t bits,
                                                                     CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto token = value;
        const auto requestedOperation = kind;
        const auto requestedBits = bits;
        if (receipt)
            *receipt = {};
        if (this->operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope scope{this->operation};
        if (stopping)
            return ClockResult::OwnerConflict;
        const auto entry = Find(token);
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        ClockResult result;
        using Operation = NativePrivateAnimationHierarchyCalls::ScalarOperation;
        if (requestedOperation == Operation::RunningCompressed)
            result =
                PrivateHierarchyControllerDetail::Calls::SetRunningCompressed(image, entry->storage, token.ownerGeneration, requestedBits != 0, owner, receipt);
        else
        {
            std::int32_t block;
            std::memcpy(&block, &requestedBits, sizeof(block));
            result = PrivateHierarchyControllerDetail::Calls::SetAnimationBlockID(image, entry->storage, token.ownerGeneration, block, owner, receipt);
        }
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::SetRunningCompressed(std::uintptr_t image, const Token& value, bool compressed, CheckOwner owner,
                                                                                Receipt* receipt) noexcept
    {
        const auto token = value;
        const auto requested = compressed;
        return SetScalar(image, token, NativePrivateAnimationHierarchyCalls::ScalarOperation::RunningCompressed, requested ? 1u : 0u, owner, receipt);
    }
    ClockResult NativePrivateAnimationHierarchyController::SetAnimationBlockID(std::uintptr_t image, const Token& value, std::int32_t block, CheckOwner owner,
                                                                               Receipt* receipt) noexcept
    {
        const auto    token = value;
        const auto    requested = block;
        std::uint32_t bits;
        std::memcpy(&bits, &requested, sizeof(bits));
        return SetScalar(image, token, NativePrivateAnimationHierarchyCalls::ScalarOperation::AnimationBlockID, bits, owner, receipt);
    }
    ClockResult NativePrivateAnimationHierarchyController::Destroy(std::uintptr_t image, const Token& value, CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto token = value;
        if (receipt)
            *receipt = {};
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope scope{operation};
        const auto                              entry = Find(token);
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        Receipt    resultReceipt;
        const auto result = PrivateHierarchyControllerDetail::Calls::Retire(image, entry->storage, token.ownerGeneration, owner, &resultReceipt);
        // A known retired permission consumes only the plain CPU record even
        // if its final owner acknowledgement failed. Never delete twice.
        if (resultReceipt.retired)
            entries.erase(entry);
        if (receipt)
            *receipt = resultReceipt;
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::Read(std::uintptr_t image, const Token& value, CheckOwner owner, Values& output) noexcept
    {
        const auto token = value;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope scope{operation};
        const auto                              entry = Find(token);
        const auto                              result = entry == entries.end() ? ClockResult::BindingChanged
                                                                                : PrivateHierarchyControllerDetail::Calls::Read(image, entry->storage, token.ownerGeneration, owner, output);
        // A const native read can newly latch foreign lifetime uncertainty.
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::ReadSequences(std::uintptr_t image, const Token& value, CheckOwner owner,
                                                                         std::uintptr_t& output) noexcept
    {
        const auto token = value;
        Values     values;
        const auto result = Read(image, token, owner, values);
        if (result == ClockResult::Ok)
            output = 0;
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::ReadSequence(std::uintptr_t image, const Token& value, std::uint32_t index, CheckOwner owner,
                                                                        std::uintptr_t& output) noexcept
    {
        const auto token = value;
        const auto requested = index;
        Values     values;
        const auto result = Read(image, token, owner, values);
        if (result != ClockResult::Ok)
            return result;
        if (requested)
            return ClockResult::InvalidArgument;
        output = 0;
        return ClockResult::Ok;
    }
    ClockResult NativePrivateAnimationHierarchyController::PinPublication(const Token& value) noexcept
    {
        const auto token = value;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope scope{operation};
        const auto                              entry = Find(token);
        if (entry == entries.end())
            return ClockResult::BindingChanged;
        const auto result = PrivateHierarchyControllerDetail::Calls::PinPublication(entry->storage, token.ownerGeneration);
        Publish();
        return result;
    }
    ClockResult NativePrivateAnimationHierarchyController::RetireEmpty(std::uintptr_t image, std::uint64_t generation, std::size_t budget, CheckOwner owner,
                                                                       RetireProgress& output) noexcept
    {
        const auto requested = generation;
        const auto limit = budget;
        if (operation.test_and_set(std::memory_order_acquire))
            return ClockResult::Busy;
        PrivateHierarchyControllerDetail::Scope scope{operation};
        stopping = true;
        RetireProgress progress;
        ClockResult    firstFailure = ClockResult::Ok;
        const auto     count = (std::min)(limit, entries.size());
        for (std::size_t index = 0; index != count; ++index)
        {
            const auto entry = entries.begin();
            ++progress.scanned;
            Receipt     receipt;
            ClockResult result = ClockResult::OwnerConflict;
            if (entry->storage.OwnerGeneration() == requested)
            {
                ++progress.attempted;
                result = PrivateHierarchyControllerDetail::Calls::Retire(image, entry->storage, requested, owner, &receipt);
            }
            if (receipt.retired)
            {
                entries.erase(entry);
                ++progress.released;
            }
            else
                entries.splice(entries.end(), entries, entry);
            if (result != ClockResult::Ok && firstFailure == ClockResult::Ok)
                firstFailure = result;
        }
        progress.retained = entries.size();
        output = progress;
        Publish();
        return firstFailure;
    }
}
