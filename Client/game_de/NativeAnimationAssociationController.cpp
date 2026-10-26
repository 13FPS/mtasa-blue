/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One durable issued serial owner for private native associations
 ****************************************************************************/
#include "NativeAnimationAssociationController.h"
#include <limits>
#include <algorithm>
#include <new>
namespace mta::game_de
{
    namespace AnimationAssociationControllerDetail
    {
        struct Guard
        {
            std::atomic<bool>& busy;
            ~Guard() { busy.store(false, std::memory_order_release); }
        };
        bool Enter(std::atomic<bool>& busy) noexcept
        {
            bool expected = false;
            return busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        }
    }
    struct NativeAnimationAssociationController::Entry
    {
        Token                             token{};
        NativeAnimationAssociationStorage storage;
    };
    NativeAnimationAssociationController::NativeAnimationAssociationController() noexcept = default;
    NativeAnimationAssociationController::~NativeAnimationAssociationController() = default;
    void NativeAnimationAssociationController::Publish() noexcept
    {
        bool any = false, fault = false;
        for (const auto& entry : records)
        {
            any |= entry->storage.IsRetained();
            const auto state = entry->storage.GetState();
            fault |= state == NativeAnimationAssociationStorage::State::FaultDebt || state == NativeAnimationAssociationStorage::State::PublicationPinned ||
                     state == NativeAnimationAssociationStorage::State::ConstructEntered || state == NativeAnimationAssociationStorage::State::RetireEntered;
        }
        retained.store(any, std::memory_order_release);
        recovery.store(fault, std::memory_order_release);
    }
    NativeAnimationAssociationController::Entry* NativeAnimationAssociationController::Find(std::uint64_t generation, const Token& token) noexcept
    {
        if (!generation || token.ownerGeneration != generation || !token.serial)
            return nullptr;
        for (const auto& entry : records)
            if (entry->token.ownerGeneration == generation && entry->token.serial == token.serial)
                return entry.get();
        return nullptr;
    }
    ClockResult NativeAnimationAssociationController::Create(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, Token& output,
                                                             Receipt* receipt) noexcept
    {
        Receipt next;
        if (receipt)
            *receipt = next;
        if (!generation)
            return ClockResult::InvalidArgument;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        if (serial == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::OwnerConflict;
        // Reserve the durable record before the first native allocator entry.
        // CPU OOM therefore cannot orphan an acknowledged outer allocation.
        try
        {
            auto entry = std::make_unique<Entry>();
            entry->token = {generation, serial + 1};
            records.push_back(std::move(entry));
        }
        catch (...)
        {
            return ClockResult::WriteFailure;
        }
        auto& entry = *records.back();
        serial = entry.token.serial;
        const auto result = NativeAnimationAssociationCalls::Create(image, entry.storage, generation, owner, &next);
        if (result == ClockResult::Ok)
            output = entry.token;
        if (entry.storage.CanDiscard())
            records.pop_back();
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::Construct(std::uintptr_t image, std::uint64_t generation, const Token& token, std::uintptr_t source,
                                                                CheckOwner owner, std::uintptr_t& output, Receipt* receipt) noexcept
    {
        const auto request = token;
        const auto locator = source;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::Construct(image, entry->storage, generation, locator, owner, output, &next);
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::Read(std::uintptr_t image, std::uint64_t generation, const Token& token, CheckOwner owner,
                                                           Values& output) noexcept
    {
        const auto request = token;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::Read(image, entry->storage, generation, owner, output);
        Publish();
        return result;
    }
    ClockResult NativeAnimationAssociationController::Set(std::uintptr_t image, std::uint64_t generation, const Token& token, Scalar scalar, std::uint32_t bits,
                                                          CheckOwner owner, Receipt* receipt) noexcept
    {
        const auto request = token;
        const auto value = bits;
        const auto field = scalar;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::Set(image, entry->storage, generation, field, value, owner, &next);
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::RetireOne(std::uintptr_t image, std::uint64_t generation, const Token& token, CheckOwner owner,
                                                                Receipt* receipt) noexcept
    {
        const auto request = token;
        Receipt    next;
        if (receipt)
            *receipt = next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        auto*                                       entry = Find(generation, request);
        if (!entry)
            return ClockResult::OwnerConflict;
        const auto result = NativeAnimationAssociationCalls::Retire(image, entry->storage, generation, owner, &next);
        if (entry->storage.CanDiscard())
            for (auto at = records.begin(); at != records.end(); ++at)
                if (at->get() == entry)
                {
                    records.erase(at);
                    break;
                }
        Publish();
        if (receipt)
            *receipt = next;
        return result;
    }
    ClockResult NativeAnimationAssociationController::Retire(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, std::uint32_t budget,
                                                             RetireProgress& output) noexcept
    {
        RetireProgress next;
        if (!AnimationAssociationControllerDetail::Enter(busy))
            return ClockResult::Busy;
        AnimationAssociationControllerDetail::Guard guard{busy};
        ClockResult                                 result = ClockResult::Ok;
        const auto                                  limit = (std::min)(static_cast<std::size_t>(budget), records.size());
        for (std::size_t index = 0; index != limit; ++index)
        {
            auto  at = records.begin();
            auto& entry = **at;
            ++next.scanned;
            if (entry.token.ownerGeneration != generation)
            {
                records.splice(records.end(), records, at);
                continue;
            }
            Receipt receipt;
            ++next.attempted;
            const auto current = NativeAnimationAssociationCalls::Retire(image, entry.storage, generation, owner, &receipt);
            if (entry.storage.CanDiscard())
            {
                records.erase(at);
                ++next.released;
            }
            else
                records.splice(records.end(), records, at);
            // Entered failures stop the enclosing runtime. Known blocked rows
            // rotate fairly; they never authorize an unrelated resource free.
            if (current != ClockResult::Ok && result == ClockResult::Ok)
                result = current;
            if (receipt.entered && current != ClockResult::Ok)
                break;
        }
        Publish();
        for (const auto& entry : records)
            if (entry->storage.IsRetained())
                ++next.retained;
        output = next;
        return result;
    }
}
