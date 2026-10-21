/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA ped slot/count and caller ownership contracts
 *****************************************************************************/
#include <game/CPedRegistryPolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <type_traits>

namespace
{
    using Policy = MtaGame::CPedRegistryPolicy;
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
    struct PedTag
    {
        unsigned              id = 0;
        std::function<void()> onDestroy;
        ~PedTag()
        {
            if (onDestroy)
                onDestroy();
        }
    };
    // Ordinary C++ objects below are pointer identities only. No CPed, native
    // interface or client entity is fabricated or invoked by this CPU test.
    struct ClientTag
    {
        unsigned id = 0;
    };
    CClientEntity* Client(ClientTag& value)
    {
        return reinterpret_cast<CClientEntity*>(&value);
    }
    template <std::size_t Size>
    void Cases()
    {
        static_assert(Size >= 4);
        std::array<SClientEntity<PedTag>, Size> entries{};
        unsigned long                           count = 0;
        PedTag                                  first{1, {}}, second{2, {}}, third{3, {}}, replacement{4, {}};
        ClientTag                               clientA{1}, clientB{2};
        for (const auto& slot : entries)
            Check(!slot.pEntity && !slot.pClientEntity, "ordinary SDK slot array is explicitly initialized");
        Check(Policy::MayCreate(count, Size), "empty count admits caller creation");
        Check(Policy::Register(entries, count, Size - 1, &first, Client(clientA)) && count == 1, "sparse last native slot registers independently of count");
        Check(Policy::Register(entries, count, 0, &second, Client(clientA)) && count == 2, "local index zero registration is ordinary slot policy");
        Check(Policy::Register(entries, count, 2, &third, nullptr) && count == 3, "a wrapper may have no client association");
        Check(Policy::Register(entries, count, 2, &replacement, Client(clientB)) && count == 4,
              "duplicate index replaces pointers and increments established count");
        Check(entries[2].pEntity == &replacement && entries[2].pClientEntity == Client(clientB) && first.id == 1 && third.id == 3,
              "replacement does not destroy a caller-owned wrapper");
        const auto beforeInvalid = count;
        Check(!Policy::Register(entries, count, Size, &first, nullptr) && count == beforeInvalid, "out-of-range registration preserves count");
        Check(!Policy::Register(entries, count, (std::numeric_limits<std::size_t>::max)(), &first, nullptr) && count == beforeInvalid,
              "malformed large slot cannot index the registry");
        Check(!Policy::Find(entries, 1) && !Policy::Find(entries, Size), "empty and invalid slot lookups refuse");
        Check(Policy::Find(entries, Size - 1) == &entries[Size - 1], "actual SDK slot view preserves its stable address");
        Check(Policy::AssociateIfPresent(entries, Size - 1, Client(clientB)) && count == beforeInvalid && entries[Size - 1].pEntity == &first &&
                  entries[Size - 1].pClientEntity == Client(clientB),
              "existing native-interface AddPed branch reassigns client without creating or counting a wrapper");
        Check(!Policy::AssociateIfPresent(entries, 1, Client(clientA)) && !entries[1].pClientEntity && count == beforeInvalid,
              "missing slot association leaves creation to the actual caller");
        const auto highSlot = static_cast<std::uint32_t>(Size - 1);
        for (std::uint32_t low = 0; low != 256; ++low)
        {
            const auto handle = (highSlot << 8) | low;
            unsigned   resolves = 0;
            auto*      found = Policy::FindFromRef(entries, handle,
                                                   [&](std::uint32_t observed)
                                                   {
                                                  ++resolves;
                                                  Check(observed == handle, "full opaque handle reaches native resolver without truncation");
                                                  return true;
                                              });
            Check(found == &first && resolves == 1, "resolved handle selects the indexed wrapper after exactly one native resolution");
            resolves = 0;
            found = Policy::FindFromRef(entries, handle,
                                        [&](std::uint32_t)
                                        {
                                            ++resolves;
                                            return false;
                                        });
            Check(!found && resolves == 1 && entries[Size - 1].pEntity == &first,
                  "failed generation/native resolution cannot be replaced by a cached CPU wrapper");
        }
        unsigned resolves = 0;
        Check(!Policy::FindFromRef(entries, UINT32_MAX,
                                   [&](std::uint32_t)
                                   {
                                       ++resolves;
                                       return true;
                                   }) &&
                  resolves == 1,
              "resolver runs first and an unregistered large index is still bounded");
        resolves = 0;
        Check(!Policy::FindFromRef(entries, 1,
                                   [&](std::uint32_t)
                                   {
                                       ++resolves;
                                       return false;
                                   }) &&
                  resolves == 1,
              "handle1 is not an ambient player fallback");
        Check(Policy::FindFromRef(entries, 1, [](std::uint32_t) { return true; }) == &second, "handle1 can select slotzero only after real resolver success");
        Policy::ResetCount(count);
        Check(count == 0 && entries[0].pEntity == &second && entries[Size - 1].pClientEntity == Client(clientB),
              "InitLocalPlayer reset clears count only and preserves existing wrappers and client pairs");
        Check(Policy::FindFromRef(entries, 1, [](std::uint32_t) { return true; }) == &second, "native-first lookup is independent of reset count");
        Check(Policy::AssociateIfPresent(entries, 0, Client(clientB)) && count == 0 && entries[0].pEntity == &second,
              "local re-association after count reset does not invent a new registration");
        Policy::InvalidateLocalClient(entries);
        Check(!entries[0].pClientEntity && entries[0].pEntity == &second && entries[Size - 1].pClientEntity == Client(clientB) && count == 0,
              "invalidating local client preserves wrapper, count and other associations");
        Check(Policy::Register(entries, count, 1, &third, Client(clientA)) && count == 1, "creation uses count rather than reconstructed occupancy");
        bool resolverRan = false;
        Check(!Policy::FindFromRef(entries, 1u << 8,
                                   [&](std::uint32_t)
                                   {
                                       resolverRan = true;
                                       Policy::Clear(entries, 1);
                                       return true;
                                   }) &&
                  resolverRan,
              "slot selection observes state only after the native callback returns");
        count = static_cast<unsigned long>(Size);
        Check(!Policy::MayCreate(count, Size), "full count prevents caller allocation before native creation");
        ++count;
        Check(!Policy::MayCreate(count, Size), "count above capacity remains a creation refusal");
        count = 0;
        Policy::FinishRemoval(count);
        Check(count == (std::numeric_limits<unsigned long>::max)(), "shared count preserves existing unsigned decrement semantics without clamping");
    }
    void DestructionOrder()
    {
        std::array<SClientEntity<PedTag>, 4> entries{};
        unsigned long                        count = 0;
        auto                                 owned = std::make_unique<PedTag>();
        unsigned                             destroys = 0;
        owned->onDestroy = [&]
        {
            ++destroys;
            Check(!entries[3].pEntity && !entries[3].pClientEntity && count == 1,
                  "wrapper destructor sees slot retired while count is still maintained by caller");
            Check(!Policy::FindFromRef(entries, 3u << 8, [](std::uint32_t) { return true; }),
                  "reentrant lookup during caller destruction cannot rediscover retired wrapper");
        };
        Check(Policy::Register(entries, count, 3, owned.get(), nullptr), "register genuine ordinary CPU lifetime for ownership test");
        auto* detached = Policy::Clear(entries, 3);
        Check(detached == owned.get() && destroys == 0 && count == 1, "clearing never runs a native or C++ destructor or decrements count early");
        owned.reset();
        Policy::FinishRemoval(count);
        Check(destroys == 1 && count == 0, "actual caller owns destruction and completes count retirement afterward");
        Check(!Policy::Clear(entries, 4) && !Policy::Clear(entries, 3) && count == 0, "invalid/already-cleared slots do not manufacture removed wrappers");
    }
}
int main()
{
    using Slot = SClientEntity<PedTag>;
    static_assert(std::is_standard_layout_v<Slot> && std::is_trivially_copyable_v<Slot>);
    static_assert(sizeof(Slot) == 2 * sizeof(void*) && alignof(Slot) == alignof(void*));
    static_assert(offsetof(Slot, pEntity) == 0 && offsetof(Slot, pClientEntity) == sizeof(void*));
    Cases<4>();
    Cases<140>();
    Cases<1000>();
    DestructionOrder();
    std::array<SClientEntity<PedTag>, 0> empty{};
    unsigned long                        count = 0;
    Check(!Policy::Find(empty, 0) && !Policy::Clear(empty, 0) && !Policy::MayCreate(count, 0), "zero-sized CPU view remains bounded");
    std::printf("Passed %u actual MTA ped registry policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
