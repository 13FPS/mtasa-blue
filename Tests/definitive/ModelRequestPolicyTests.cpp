/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA streaming retries and wrapper/native reference order
 ****************************************************************************/
#include <game/CModelRequestPolicy.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using Policy = MtaGame::CModelRequestPolicy;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, why);
            std::exit(EXIT_FAILURE);
        }
    }
    struct RequestHost
    {
        std::vector<std::string> events;
        std::vector<bool>        priorities, answers;
        unsigned                 queries = 0, loads = 0, requests = 0, reports = 0, dependencies = 0, throwAt = 0;
        int                      loadedAfter = 0, attempts = -1;
        bool                     initiallyLoaded = false, dependencyLoads = false, reportLoaded = false;
        std::uint32_t            flags = 0;
        EModelRequestType        expectedType = NON_BLOCKING;
        const char*              expectedTag = "policy";
        void                     Event(std::string value)
        {
            events.push_back(std::move(value));
            if (throwAt == events.size())
                throw std::runtime_error("owned callback fault");
        }
        void Run(EModelRequestType type = BLOCKING, const char* tag = "policy")
        {
            expectedType = type;
            expectedTag = tag;
            Policy::Request(
                type, tag,
                [&]() -> bool
                {
                    Event("query");
                    ++queries;
                    if (queries <= answers.size())
                        return answers[queries - 1];
                    return initiallyLoaded || (loadedAfter > 0 && loads >= static_cast<unsigned>(loadedAfter));
                },
                [&](EModelRequestType dependencyType, const char* dependencyTag)
                {
                    Event("dependencies");
                    ++dependencies;
                    Check(dependencyType == expectedType && dependencyTag == expectedTag, "dependency work receives the original type and tag address");
                    if (dependencyLoads)
                        initiallyLoaded = true;
                },
                [&](std::uint32_t value)
                {
                    Event("request:" + std::to_string(value));
                    ++requests;
                    flags = value;
                },
                [&](bool priority, const char* passedTag)
                {
                    Event(priority ? "load:1" : "load:0");
                    ++loads;
                    priorities.push_back(priority);
                    Check(passedTag == expectedTag, "every blocking native load receives the same tag address, including null");
                },
                [&](bool loaded, int count, const char* passedTag)
                {
                    Event(std::string("report:") + (loaded ? "1:" : "0:") + std::to_string(count));
                    ++reports;
                    reportLoaded = loaded;
                    attempts = count;
                    Check(passedTag == expectedTag, "late blocking report preserves the original tag address");
                });
        }
    };
    void RequestCases()
    {
        RequestHost loaded;
        loaded.initiallyLoaded = true;
        loaded.Run();
        Check(loaded.events == std::vector<std::string>{"query"}, "already loaded requests perform no dependencies, streaming or report work");
        for (const auto type : {NON_BLOCKING})
            for (const char* tag : {"policy", static_cast<const char*>(nullptr)})
            {
                RequestHost host;
                host.dependencyLoads = true;
                host.Run(type, tag);
                Check(host.events == std::vector<std::string>{"query", "dependencies", "request:6"} && host.requests == 1 && !host.loads && !host.reports,
                      "nonblocking requests retain flag06 and do not add a post-dependency loaded check");
            }
        for (int target = 1; target <= 12; ++target)
        {
            RequestHost host;
            host.loadedAfter = target;
            host.Run();
            const unsigned expectedLoads = static_cast<unsigned>(std::min(target, 11));
            Check(host.requests == 1 && host.flags == 0x16 && host.loads == expectedLoads,
                  "blocking request uses flag16 and at most the initial load plus ten retries");
            Check(host.events[0] == "query" && host.events[1] == "dependencies" && host.events[2] == "request:22" && host.events[3] == "load:1",
                  "dependency/native request/initial priority load ordering remains unchanged");
            for (unsigned i = 0; i < host.priorities.size(); ++i)
                Check(host.priorities[i] == (i == 0 || i < 3 || (i & 1)),
                      "retry priority follows the actual counter sequence rather than an invented three-attempt loop");
            if (target == 1)
                Check(host.queries == 2 && !host.reports, "immediate blocking success avoids retries and late-attempt reporting");
            else
            {
                Check(host.reports == 1 && host.attempts == std::min(target, 11) && host.reportLoaded == (target <= 11),
                      "successful and exhausted retry paths retain exact postincrement attempt counts and report selection");
                Check(host.queries == (target <= 10 ? static_cast<unsigned>(target + 3) : 13u),
                      "query call counts retain while short-circuit and final acknowledgement ordering");
            }
        }
        RequestHost changing;
        changing.answers = {false, false, true, false};
        changing.Run();
        Check(changing.loads == 1 && changing.queries == 4 && changing.reports == 1 && !changing.reportLoaded && changing.attempts == 1,
              "changing loaded callbacks are observed at every original boundary, including a different final report result");
        RequestHost baseline;
        baseline.loadedAfter = 4;
        baseline.Run();
        for (unsigned fault = 1; fault <= baseline.events.size(); ++fault)
        {
            RequestHost host;
            host.loadedAfter = 4;
            host.throwAt = fault;
            bool threw = false;
            try
            {
                host.Run();
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
            Check(threw && host.events.size() == fault && std::equal(host.events.begin(), host.events.end(), baseline.events.begin()),
                  "exceptions propagate from each real callback boundary without extra query, retry, report or rollback");
        }
    }

    struct ReferenceHost
    {
        std::uint32_t references = 0, pending = 0, model = 7;
        std::uint16_t nativeReferences = 0;
        bool          loaded = false, async = true, requestLoads = false, throwRequest = false, throwIncrement = false, throwQuery = false, throwRemove = false;
        unsigned      requests = 0, increments = 0, removes = 0;
        std::vector<std::string> events;
        EModelRequestType        requested = NON_BLOCKING;
        const char*              tagSeen = nullptr;
        void                     Increment()
        {
            events.push_back("increment");
            ++increments;
            ++nativeReferences;
            if (throwIncrement)
                throw std::runtime_error("accepted interface increment lost acknowledgement");
        }
        bool IsLoaded()
        {
            return Policy::IsLoaded(
                references, pending,
                [&]
                {
                    events.push_back("query");
                    if (throwQuery)
                        throw std::runtime_error("loaded query failed");
                    return loaded;
                },
                [&] { Increment(); });
        }
        void Add(EModelRequestType type = NON_BLOCKING, const char* tag = "reference")
        {
            Policy::AddRef(
                references, pending, type, tag, [&] { return IsLoaded(); },
                [&]
                {
                    events.push_back("async");
                    return async;
                },
                [&](EModelRequestType value, const char* passedTag)
                {
                    events.push_back("request");
                    ++requests;
                    requested = value;
                    tagSeen = passedTag;
                    if (requestLoads)
                        loaded = true;
                    if (throwRequest)
                        throw std::runtime_error("accepted request lost acknowledgement");
                },
                [&] { Increment(); });
        }
        void Remove()
        {
            Policy::RemoveRef(
                references, pending, model, [&] { return IsLoaded(); },
                [&]
                {
                    events.push_back("remove");
                    ++removes;
                    if (nativeReferences)
                        --nativeReferences;
                    if (throwRemove)
                        throw std::runtime_error("accepted removal lost acknowledgement");
                });
        }
    };
    void ReferenceCases()
    {
        ReferenceHost loaded;
        loaded.loaded = true;
        loaded.nativeReferences = 5;
        loaded.Add();
        Check(loaded.events == std::vector<std::string>{"query", "query", "increment"} && loaded.references == 1 && loaded.nativeReferences == 6 &&
                  !loaded.pending,
              "the first loaded wrapper reference performs two original observations then one genuine interface increment before its CPU increment");
        loaded.events.clear();
        loaded.Add();
        Check(loaded.events == std::vector<std::string>{"query"} && loaded.references == 2 && loaded.nativeReferences == 6,
              "additional wrapper references do not manufacture additional native references");
        loaded.events.clear();
        loaded.Remove();
        Check(loaded.references == 1 && loaded.events.empty() && !loaded.removes, "non-final wrapper removal avoids loaded/native callbacks");
        loaded.Remove();
        Check(!loaded.references && loaded.nativeReferences == 5 && loaded.events == std::vector<std::string>{"query", "remove"},
              "last loaded non-CJ wrapper reference observes then removes its one native reference");
        ReferenceHost pending;
        pending.Add();
        Check(pending.events == std::vector<std::string>{"query", "async", "request", "query"} && pending.references == 1 && pending.pending == 1 &&
                  !pending.increments && pending.requested == NON_BLOCKING,
              "unloaded asynchronous first reference records pending intent after request without a fake native increment");
        pending.events.clear();
        Check(!pending.IsLoaded() && pending.pending == 1 && !pending.increments, "an unloaded observation preserves pending native intent");
        pending.events.clear();
        pending.Add();
        Check(pending.events == std::vector<std::string>{"query", "async", "request"} && pending.references == 2 && pending.pending == 1 && !pending.increments,
              "another unloaded wrapper reference retains one pending native increment");
        pending.loaded = true;
        pending.events.clear();
        Check(pending.IsLoaded(), "the actual loaded acknowledgement can fulfill pending intent");
        Check(pending.events == std::vector<std::string>{"query", "increment"} && pending.nativeReferences == 1 && !pending.pending && pending.references == 2,
              "first loaded acknowledgement increments the genuine interface then clears pending while preserving all CPU references");
        Check(pending.IsLoaded() && pending.increments == 1, "later loaded queries do not replay the fulfilled native increment");
        ReferenceHost cancelled;
        cancelled.Add();
        cancelled.events.clear();
        cancelled.Remove();
        Check(!cancelled.references && !cancelled.pending && cancelled.events.empty() && !cancelled.increments && !cancelled.removes,
              "last unfulfilled reference cancels pending intent before any loaded query or native removal");
        ReferenceHost forced;
        forced.async = false;
        forced.requestLoads = true;
        forced.Add(NON_BLOCKING, nullptr);
        Check(forced.requested == BLOCKING && !forced.tagSeen && forced.references == 1 && !forced.pending && forced.increments == 1,
              "disabled asynchronous loading forces the real blocking request and preserves a null tag");
        ReferenceHost wrapped;
        wrapped.loaded = true;
        wrapped.references = (std::numeric_limits<std::uint32_t>::max)();
        wrapped.Add();
        Check(!wrapped.references && !wrapped.increments, "existing unsigned DWORD wrapper increment wraps without a new saturation policy");
        ReferenceHost nativeWrapped;
        nativeWrapped.loaded = true;
        nativeWrapped.nativeReferences = 0xffff;
        nativeWrapped.Add();
        Check(nativeWrapped.references == 1 && !nativeWrapped.nativeReferences,
              "the shared policy never clamps or replaces the caller's native WORD reference behavior");

        for (const auto count : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{2}, (std::numeric_limits<std::uint32_t>::max)()})
            for (const auto pendingValue : {std::uint32_t{0}, std::uint32_t{1}, std::uint32_t{7}})
                for (const auto model : {std::uint32_t{0}, std::uint32_t{7}, std::uint32_t{0xffffffff}})
                    for (const auto isLoaded : {false, true})
                    {
                        ReferenceHost host;
                        host.references = count;
                        host.pending = pendingValue;
                        host.model = model;
                        host.loaded = isLoaded;
                        host.nativeReferences = 1;
                        host.Remove();
                        const auto remaining = count ? count - 1 : 0;
                        const bool cancelledPending = !remaining && pendingValue;
                        const bool queried = !remaining && !cancelledPending && model != 0;
                        const bool removed = queried && isLoaded;
                        Check(host.references == remaining && host.pending == (cancelledPending ? 0 : pendingValue),
                              "counter/pending transitions preserve zero, final, non-final and full unsigned values");
                        Check(host.events.size() == static_cast<std::size_t>(queried) + static_cast<std::size_t>(removed) &&
                                  host.removes == static_cast<unsigned>(removed),
                              "CJ, absent/final pending intent and non-final references keep their original callback short-circuits");
                    }
        ReferenceHost pendingWord;
        pendingWord.references = 3;
        pendingWord.pending = 7;
        pendingWord.loaded = true;
        Check(pendingWord.IsLoaded() && !pendingWord.pending && pendingWord.increments == 1 && pendingWord.nativeReferences == 1,
              "pending DWORD is boolean intent: one increment acknowledges a noncanonical nonzero pending value");
    }
    void ReferenceExceptions()
    {
        ReferenceHost request;
        request.throwRequest = true;
        bool threw = false;
        try
        {
            request.Add();
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && request.requests == 1 && !request.references && !request.pending && !request.increments,
              "request exception leaves its accepted request visible but performs no later pending/native/CPU reference changes");
        ReferenceHost first;
        first.loaded = true;
        first.throwIncrement = true;
        threw = false;
        try
        {
            first.Add();
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && first.nativeReferences == 1 && first.increments == 1 && !first.references && !first.pending,
              "first native increment precedes the CPU reference increment and is not rolled back after a late callback exception");
        ReferenceHost pending;
        pending.references = 3;
        pending.pending = 7;
        pending.loaded = true;
        pending.throwIncrement = true;
        threw = false;
        try
        {
            pending.IsLoaded();
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && pending.nativeReferences == 1 && pending.increments == 1 && pending.pending == 7 && pending.references == 3,
              "pending acknowledgement clears only after the native effect returns and introduces no automatic retry on failure");
        ReferenceHost query;
        query.references = 1;
        query.throwQuery = true;
        threw = false;
        try
        {
            query.Remove();
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && !query.references && !query.removes, "final CPU decrement precedes the loaded-state callback even when that callback throws");
        ReferenceHost remove;
        remove.references = 1;
        remove.loaded = true;
        remove.nativeReferences = 1;
        remove.throwRemove = true;
        threw = false;
        try
        {
            remove.Remove();
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && !remove.references && !remove.nativeReferences && remove.removes == 1,
              "a late native removal exception preserves both preceding decrements without retry or resurrection");
    }
    void UpgradeCases()
    {
        static_assert(!Policy::IsUpgradeModel(999) && Policy::IsUpgradeModel(1000) && Policy::IsUpgradeModel(1193) && !Policy::IsUpgradeModel(1194));
        for (std::uint32_t model = 0; model < 20000; ++model)
            Check(Policy::IsUpgradeModel(model) == (model >= 1000 && model <= 1193),
                  "both classic streaming dispatches retain the exact native upgrade interval");
        Check(!Policy::IsUpgradeModel(0xffff) && !Policy::IsUpgradeModel(0x10000) && !Policy::IsUpgradeModel(0xffffffff),
              "upgrade dispatch preserves full unsigned keys rather than narrowing to a ped model WORD");
    }
}
int main()
{
    RequestCases();
    ReferenceCases();
    ReferenceExceptions();
    UpgradeCases();
    std::puts("Production MTA model request and reference policy fixture completed");
    std::printf("Passed %u actual MTA model request policy checks (%u-bit)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
    return EXIT_SUCCESS;
}
