/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA cover cleanup with exact accepted-prefix ownership
 ****************************************************************************/
#include "NativeWorldCoverDomain.h"
namespace mta::game_de
{
    namespace CoverDomainDetail
    {
        namespace Memory = NativeWorldCoverMemory;
        namespace Context = NativeStreamingCompletionContext;
        struct Guard
        {
            std::atomic_flag&  lock;
            std::atomic<bool>& active;
            bool               held;
            Guard(std::atomic_flag& flag, std::atomic<bool>& busy) : lock(flag), active(busy), held(!flag.test_and_set())
            {
                if (held)
                    active.store(true);
            }
            ~Guard()
            {
                if (held)
                {
                    active.store(false);
                    lock.clear();
                }
            }
        };
        struct Scope
        {
            bool held = Context::TryEnter();
            ~Scope()
            {
                if (held)
                    Context::Leave();
            }
        };
        ClockResult Owner(std::uintptr_t image, std::uint64_t generation, NativeWorldCoverDomain::CheckOwner owner,
                          NativeWorldCoverDomain::CheckOwner after) noexcept
        {
            if (!image || !generation || !owner || !after)
                return ClockResult::InvalidArgument;
            const auto result = after();
            return result == ClockResult::Ok ? Context::Owner(image, owner) : result;
        }
        ClockResult Sample(std::uintptr_t image, std::uint64_t generation, NativeWorldCoverDomain::CheckOwner owner, NativeWorldCoverDomain::CheckOwner after,
                           Memory::Value& value, const Memory::Value* retained)
        {
            auto result = Owner(image, generation, owner, after);
            if (result != ClockResult::Ok || (result = Memory::Capture(image, value, retained)) != ClockResult::Ok ||
                (result = Owner(image, generation, owner, after)) != ClockResult::Ok)
                return result;
            return Memory::Seal(image, value) ? ClockResult::Ok : ClockResult::BindingChanged;
        }
    }
    ClockResult NativeWorldCoverDomain::Ready(std::uintptr_t selectedImage, std::uint64_t selectedGeneration, CheckOwner owner, CheckOwner after) noexcept
    {
        using namespace CoverDomainDetail;
        Guard guard(operation, active);
        Scope scope;
        if (!guard.held || !scope.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::BindingChanged;
        if (expected && (image != selectedImage || generation != selectedGeneration))
            return ClockResult::BindingChanged;
        try
        {
            Memory::Value current;
            const auto    result = Sample(selectedImage, selectedGeneration, owner, after, current, expected.get());
            return result != ClockResult::Ok ? result : expected && !Memory::Same(*expected, current) ? ClockResult::BindingChanged : ClockResult::Ok;
        }
        catch (...)
        {
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeWorldCoverDomain::RemoveAll(std::uintptr_t selectedImage, std::uint64_t selectedGeneration, CheckOwner owner, CheckOwner after,
                                                  NativeWorldCoverCalls::Invoker invoke) noexcept
    {
        using namespace CoverDomainDetail;
        Guard guard(operation, active);
        Scope scope;
        if (!guard.held || !scope.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::BindingChanged;
        if (expected && (image != selectedImage || generation != selectedGeneration))
            return ClockResult::BindingChanged;
        try
        {
            Memory::Value current;
            auto          result = Sample(selectedImage, selectedGeneration, owner, after, current, expected.get());
            if (result != ClockResult::Ok)
                return result;
            if (expected)
            {
                if (!Memory::Same(*expected, current))
                    return ClockResult::BindingChanged;
            }
            else
            {
                if (current.order.empty())
                    return ClockResult::Ok;
                expected = std::make_unique<Memory::Value>(current);
                // A CPU receipt is not a retained UObject lease. Re-capture
                // genuine membership and raw current owners for every retry.
                expected->world = {};
                image = selectedImage;
                generation = selectedGeneration;
                acknowledged = 0;
            }
            for (;;)
            {
                const bool    clear = acknowledged == expected->owners.size();
                Memory::Value next = *expected;
                if (!(clear ? Memory::ExpectClear(next) : Memory::ExpectRemove(image, next, expected->owners[acknowledged].address)))
                    return ClockResult::BindingChanged;
                result = Owner(image, generation, owner, after);
                if (result != ClockResult::Ok || !Memory::Seal(image, current))
                {
                    if (!acknowledged)
                    {
                        expected.reset();
                        pending.store(false);
                    }
                    return result != ClockResult::Ok ? result : ClockResult::BindingChanged;
                }
                NativeWorldCoverCalls::Receipt receipt;
                // Intent precedes the entry. A known pre-entry refusal can
                // retire only an untouched first attempt; an acknowledged
                // prefix remains tied to its exact current native resources.
                entering = true;
                pending.store(true);
                result = NativeWorldCoverCalls::Invoke(clear ? NativeWorldCoverCalls::Operation::ClearList : NativeWorldCoverCalls::Operation::RemoveForEntity,
                                                       image, clear ? 0 : expected->owners[acknowledged].address, receipt, invoke);
                if (!receipt.entered)
                {
                    entering = false;
                    if (!acknowledged)
                    {
                        expected.reset();
                        pending.store(false);
                    }
                    return result;
                }
                if (result != ClockResult::Ok || !receipt.returned)
                {
                    uncertain.store(true);
                    return result;
                }
                Memory::Value observed;
                result = Sample(image, generation, owner, after, observed, &next);
                if (result != ClockResult::Ok || !Memory::Same(next, observed))
                {
                    uncertain.store(true);
                    return result != ClockResult::Ok ? result : ClockResult::BindingChanged;
                }
                entering = false;
                if (clear)
                {
                    expected.reset();
                    acknowledged = 0;
                    pending.store(false);
                    return ClockResult::Ok;
                }
                ++acknowledged;
                *expected = std::move(next);
                expected->world = {};
                current = std::move(observed);
                // Explicit calls may resume after this next pre-entry owner
                // check fails. Never re-enter an acknowledged original body.
            }
        }
        catch (...)
        {
            if (entering)
            {
                uncertain.store(true);
                pending.store(true);
            }
            else if (!acknowledged)
            {
                expected.reset();
                pending.store(false);
            }
            return ClockResult::ReadFailure;
        }
    }
}
