/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise real owned peers and retained cleanup failure/retry
 *****************************************************************************/
#include "../../Client/game_de/NativePeerThreads.h"
#include <detours.h>
#include <atomic>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace
{
    std::atomic<unsigned> parkedCalls{0}, allocationsWhileParked{0}, freesWhileParked{0};
}

void* operator new(std::size_t size)
{
    if (parkedCalls.load())
        allocationsWhileParked.fetch_add(1);
    if (void* memory = std::malloc(size ? size : 1))
        return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void* memory) noexcept
{
    if (memory && parkedCalls.load())
        freesWhileParked.fetch_add(1);
    std::free(memory);
}
void operator delete[](void* memory) noexcept
{
    ::operator delete(memory);
}
void operator delete(void* memory, std::size_t) noexcept
{
    ::operator delete(memory);
}
void operator delete[](void* memory, std::size_t) noexcept
{
    ::operator delete(memory);
}

namespace
{
    using Peers = mta::game_de::NativePeerThreads;
    unsigned checks = 0;
    unsigned failed = 0;
    void     Check(bool valid, const char* label)
    {
        ++checks;
        if (!valid)
        {
            ++failed;
            std::printf("FAILED: %s\n", label);
        }
    }

    struct Worker
    {
        std::atomic<bool>     stop{false};
        std::atomic<unsigned> ticks{0};
        HANDLE                handle = nullptr;
        DWORD                 id = 0;
        static DWORD WINAPI   Run(void* context)
        {
            auto& worker = *static_cast<Worker*>(context);
            while (!worker.stop.load())
            {
                worker.ticks.fetch_add(1);
                Sleep(0);
            }
            return 0;
        }
        bool Start() noexcept
        {
            handle = CreateThread(nullptr, 0, Run, this, 0, &id);
            return handle != nullptr;
        }
        void Finish() noexcept
        {
            stop.store(true);
            if (handle)
            {
                WaitForSingleObject(handle, 3000);
                CloseHandle(handle);
                handle = nullptr;
            }
        }
    };

    std::atomic<unsigned> suspendCalls{0}, contextCalls{0}, resumeCalls{0}, closeCalls{0}, registerCalls{0};
    std::atomic<unsigned> suspendFailAt{0}, contextFailures{0}, resumeFailures{0}, closeFailures{0}, registerFailures{0};
    std::atomic<DWORD>    terminateId{0};
    Worker*               terminatedWorker = nullptr;

    bool Consume(std::atomic<unsigned>& counter) noexcept
    {
        auto value = counter.load();
        while (value)
        {
            if (value == UINT_MAX)
                return true;
            if (counter.compare_exchange_weak(value, value - 1))
                return true;
        }
        return false;
    }

    DWORD Suspend(HANDLE handle) noexcept
    {
        const auto call = suspendCalls.fetch_add(1) + 1;
        if (suspendFailAt.load() == call)
            return static_cast<DWORD>(-1);
        const auto result = SuspendThread(handle);
        if (result != static_cast<DWORD>(-1))
            parkedCalls.fetch_add(1);
        return result;
    }
    BOOL Context(HANDLE handle, CONTEXT* context) noexcept
    {
        contextCalls.fetch_add(1);
        return Consume(contextFailures) ? FALSE : GetThreadContext(handle, context);
    }
    DWORD Resume(HANDLE handle) noexcept
    {
        resumeCalls.fetch_add(1);
        if (Consume(resumeFailures))
            return static_cast<DWORD>(-1);
        if (terminateId.load() && GetThreadId(handle) == terminateId.load())
        {
            terminatedWorker->stop.store(true);
            const auto result = ResumeThread(handle);
            if (result != static_cast<DWORD>(-1))
                parkedCalls.fetch_sub(1);
            WaitForSingleObject(handle, 3000);
            // Model a failed resume observed after this owned peer has exited.
            // The actual signalled handle lets the production wait path prove
            // it can close the record rather than retain a dead thread.
            return static_cast<DWORD>(-1);
        }
        const auto result = ResumeThread(handle);
        if (result != static_cast<DWORD>(-1))
            parkedCalls.fetch_sub(1);
        return result;
    }
    DWORD Wait(HANDLE handle, DWORD milliseconds) noexcept
    {
        return WaitForSingleObject(handle, milliseconds);
    }
    BOOL Close(HANDLE handle) noexcept
    {
        closeCalls.fetch_add(1);
        return Consume(closeFailures) ? FALSE : CloseHandle(handle);
    }
    LONG Register(HANDLE handle) noexcept
    {
        registerCalls.fetch_add(1);
        return Consume(registerFailures) ? ERROR_ACCESS_DENIED : DetourUpdateThreadExternallySuspended(handle);
    }
    const Peers::Operations counted{Suspend, Context, Resume, Wait, Close, Register};

    void Reset() noexcept
    {
        suspendCalls.store(0);
        contextCalls.store(0);
        resumeCalls.store(0);
        closeCalls.store(0);
        registerCalls.store(0);
        suspendFailAt.store(0);
        contextFailures.store(0);
        resumeFailures.store(0);
        closeFailures.store(0);
        registerFailures.store(0);
        terminateId.store(0);
    }
    bool Allow(const CONTEXT& context, void* output) noexcept
    {
        auto& count = *static_cast<unsigned*>(output);
        ++count;
        return (context.ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL;
    }
    bool Refuse(const CONTEXT&, void*) noexcept
    {
        return false;
    }
    void Clean(const Peers& owner, const char* label)
    {
        const auto state = owner.Read();
        Check(!state.busy && !state.prepared && !state.parked && !state.retained && !owner.HasRecovery(), label);
    }
}

int main()
{
    Worker first, second;
    if (!first.Start() || !second.Start())
        return 2;
    while (!first.ticks.load() || !second.ticks.load())
        Sleep(0);
    DWORD before = 0;
    GetProcessHandleCount(GetCurrentProcess(), &before);
    {
        Peers       owner;
        Peers::Hold hold(owner);
        const bool  prepared = hold.Prepare();
        const auto  state = owner.Read();
        unsigned    contexts = 0;
        const bool  suspended = prepared && hold.Suspend(Allow, &contexts);
        const bool  released = hold.Release();
        Check(prepared && state.busy && state.prepared >= 2, "default operations open every owned peer before suspension");
        Check(suspended && contexts == state.prepared && released, "real ordinary peers suspend, capture context and resume");
        Clean(owner, "default lifecycle retires all handles");
    }
    {
        Reset();
        Peers       owner(counted);
        Peers::Hold firstHold(owner), competing(owner);
        const bool  prepared = firstHold.Prepare();
        const bool  refused = !competing.Prepare();
        const auto  state = owner.Read();
        const bool  released = firstHold.Release();
        Check(prepared && refused && state.busy && state.prepared >= 2 && released, "competing hold cannot steal prepared owner gate");
        Clean(owner, "prepared-only release closes untouched handles");
    }
    {
        Reset();
        Peers       owner(counted);
        Peers::Hold hold(owner);
        const bool  prepared = hold.Prepare();
        const bool  refused = prepared && !hold.Suspend(Refuse, nullptr);
        Check(refused && !parkedCalls.load() && contextCalls.load() == 1, "context policy refusal resumes every parked peer before returning");
        Clean(owner, "context refusal leaves no recovery on successful resume");
    }
    {
        Reset();
        suspendFailAt.store(2);
        Peers       owner(counted);
        Peers::Hold hold(owner);
        unsigned    contexts = 0;
        const bool  refused = hold.Prepare() && !hold.Suspend(Allow, &contexts);
        Check(refused && suspendCalls.load() == 2 && resumeCalls.load() == 1 && !parkedCalls.load(),
              "partial suspend failure resumes parked peer and closes untouched peers");
        Clean(owner, "partial suspend failure cleans handles");
    }
    {
        Reset();
        contextFailures.store(1);
        Peers       owner(counted);
        Peers::Hold hold(owner);
        unsigned    contexts = 0;
        const bool  refused = hold.Prepare() && !hold.Suspend(Allow, &contexts);
        Check(refused && !contexts && !parkedCalls.load(), "GetThreadContext failure resumes partial hold");
        Clean(owner, "context OS failure cleans handles");
    }
    {
        Reset();
        Peers    owner(counted);
        unsigned contexts = 0;
        bool     prepared = false, suspended = false, failedRelease = false;
        {
            Peers::Hold hold(owner);
            prepared = hold.Prepare();
            suspended = prepared && hold.Suspend(Allow, &contexts);
            resumeFailures.store(1);
            failedRelease = !hold.Release();
        }
        const auto retained = owner.Read();
        bool       refused = false;
        {
            Peers::Hold blocked(owner);
            refused = !blocked.Prepare();
        }
        const auto stillRetained = owner.Read();
        const bool recovered = owner.RetryRecover();
        Check(prepared && suspended && failedRelease && retained.retained == 1 && retained.parked == 1 && !retained.busy,
              "failed resume survives scoped hold destruction in durable owner");
        Check(refused && stillRetained.retained == retained.retained && stillRetained.parked == retained.parked,
              "new prepare cannot erase earlier failed-resume ownership");
        Check(recovered && !parkedCalls.load(), "explicit recovery resumes and closes retained peer");
        Clean(owner, "resume retry clears recovery after actual success");
    }
    {
        Reset();
        Peers       owner(counted);
        Peers::Hold hold(owner);
        unsigned    contexts = 0;
        const bool  suspended = hold.Prepare() && hold.Suspend(Allow, &contexts);
        // Recovery caps rounds across every retained peer, so use a permanent
        // failure until the bounded call completes instead of a per-call budget.
        resumeFailures.store(UINT_MAX);
        const bool failedRelease = !hold.Release();
        const auto retainedBeforeRetry = owner.Read().retained;
        const auto beforeRetry = resumeCalls.load();
        const bool boundedFailure = !owner.RetryRecover(1000);
        const auto attempts = resumeCalls.load() - beforeRetry;
        const auto retained = owner.Read();
        resumeFailures.store(0);
        const bool recovered = owner.RetryRecover(1);
        Check(suspended && failedRelease && retainedBeforeRetry && boundedFailure && attempts == Peers::MaximumRecoveryAttempts * retainedBeforeRetry &&
                  retained.retained == retainedBeforeRetry,
              "permanent resume failure keeps ownership and retry work is capped");
        Check(recovered && !parkedCalls.load(), "later explicit retry can finish bounded failure recovery");
        Clean(owner, "bounded recovery retires only after success");
    }
    {
        Reset();
        Peers       owner(counted);
        Peers::Hold hold(owner);
        unsigned    contexts = 0;
        const bool  suspended = hold.Prepare() && hold.Suspend(Allow, &contexts);
        closeFailures.store(1);
        const bool failedRelease = !hold.Release();
        const auto retained = owner.Read();
        const auto priorResumeCalls = resumeCalls.load();
        const bool recovered = owner.RetryRecover();
        Check(suspended && failedRelease && retained.retained == 1 && !retained.parked, "failed CloseHandle retains already resumed handle");
        Check(recovered && resumeCalls.load() == priorResumeCalls, "close-only recovery never adds another resume");
        Clean(owner, "close retry retires handle");
    }
    {
        Reset();
        const LONG  begin = DetourTransactionBegin();
        Peers       owner(counted);
        Peers::Hold hold(owner);
        unsigned    contexts = 0;
        const bool  suspended = begin == NO_ERROR && hold.Prepare() && hold.Suspend(Allow, &contexts);
        const bool  registered = suspended && hold.RegisterDetours();
        const LONG  aborted = begin == NO_ERROR ? DetourTransactionAbort() : ERROR_INVALID_OPERATION;
        const auto  stillParked = parkedCalls.load();
        const bool  released = hold.Release();
        Check(suspended && registered && registerCalls.load() == contexts, "real Detours registers externally suspended owned peers");
        Check(aborted == NO_ERROR && stillParked == contexts && released, "Detours abort does not steal external suspend ownership");
        Clean(owner, "Detours registration retires through explicit owner");
    }
    {
        Reset();
        Peers       owner(counted);
        Peers::Hold hold(owner);
        unsigned    contexts = 0;
        const bool  suspended = hold.Prepare() && hold.Suspend(Allow, &contexts);
        registerFailures.store(1);
        const bool refused = suspended && !hold.RegisterDetours();
        const bool released = hold.Release();
        Check(refused && released && !parkedCalls.load(), "registration failure leaves cleanup with original owner");
        Clean(owner, "registration refusal retires all peers");
    }
    {
        Reset();
        terminatedWorker = &first;
        terminateId.store(first.id);
        Peers       owner(counted);
        Peers::Hold hold(owner);
        unsigned    contexts = 0;
        const bool  suspended = hold.Prepare() && hold.Suspend(Allow, &contexts);
        const bool  released = hold.Release();
        Check(suspended && released && WaitForSingleObject(first.handle, 0) == WAIT_OBJECT_0,
              "failed resume of terminated owned peer retires a signalled handle");
        Clean(owner, "terminated peer needs no retained recovery");
        terminateId.store(0);
    }
    DWORD after = 0;
    GetProcessHandleCount(GetCurrentProcess(), &after);
    Check(after == before, "all suspension snapshot/open handles are retired");
    Check(!parkedCalls.load(), "no owned suspend count remains");
    Check(!allocationsWhileParked.load() && !freesWhileParked.load(), "no C++ heap allocation or free occurs while owned peers are parked");
    first.Finish();
    second.Finish();
    if (failed)
    {
        std::printf("Failed %u of %u production peer-thread ownership checks\n", failed, checks);
        return 1;
    }
    std::printf("Passed %u production peer-thread ownership checks\n", checks);
    return 0;
}
