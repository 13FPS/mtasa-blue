/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual mutable GetName dispatch and immediate borrowed-pointer intent
 ****************************************************************************/
#include "../../Client/game_de/CAnimBlockDE.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <initializer_list>
using namespace mta::game_de;
namespace
{
    unsigned checks = 0;
    void     Check(bool ok, const char* why)
    {
        ++checks;
        if (!ok)
        {
            std::fprintf(stderr, "FAIL %u: %s\n", checks, why);
            std::exit(1);
        }
    }
    struct Owner : NativeAnimationBlockAccess
    {
        unsigned                     names = 0, metadata = 0;
        ClockResult                  result = ClockResult::Ok;
        NativeAnimationBlockIdentity received{};
        char                         storage[16]{'P', 'E', 'D', 0};
        char*                        offered = storage;
        ClockResult                  ReadAnimationBlock(const NativeAnimationBlockIdentity&, NativeAnimationBlockValues&) noexcept override
        {
            ++metadata;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadAnimationBlockName(const NativeAnimationBlockIdentity& identity, char*& output) noexcept override
        {
            ++names;
            received = identity;
            output = offered;
            return result;
        }
    };
    class Receiver final : public CAnimBlockDE
    {
    public:
        Receiver(NativeAnimationBlockAccess& owner, const NativeAnimationBlockIdentity& identity) : CAnimBlockDE(owner, identity) {}
        CAnimBlockSAInterface*          GetInterface() override { std::abort(); }
        void                            AddRef() override { std::abort(); }
        unsigned short                  GetRefs() override { std::abort(); }
        void                            Request(EModelRequestType, bool) override { std::abort(); }
        CAnimBlendHierarchySAInterface* GetAnimationHierarchyInterface(std::size_t) override { std::abort(); }
    };
}
int main()
{
    NativeAnimationBlockIdentity identity;
    identity.index = 17;
    identity.name[0] = 'P';
    identity.name[1] = 'E';
    identity.name[2] = 'D';
    identity.ownerGeneration = 0xabcdef1234567890ull;
    Owner       owner;
    Receiver    sdk(owner, identity);
    CAnimBlock& base = sdk;
    Check(!owner.names && !owner.metadata, "construction cannot preborrow or cache a native name");
    identity.index = 99;
    identity.name[0] = 'X';
    identity.ownerGeneration = 0;
    char* first = base.GetName();
    Check(first == owner.storage && owner.names == 1 && !owner.metadata && owner.received.index == 17 && owner.received.name[0] == 'P' &&
              owner.received.ownerGeneration == 0xabcdef1234567890ull,
          "actual SDK getter calls one mandatory name owner with immutable full copied identity/generation");
    first[0] = 'X';
    Check(owner.storage[0] == 'X', "SDK returns exact mutable receiver storage, not a CPU string copy");
    char replacement[16]{'N', 'E', 'W', 0};
    owner.offered = replacement;
    Check(base.GetName() == replacement && owner.names == 2 && !owner.metadata,
          "each call observes fresh borrowed pointer; wrapper never caches old native text");
    for (const auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                              ClockResult::OwnerConflict, ClockResult::InvalidArgument})
    {
        owner.result = status;
        const auto before = owner.names;
        bool       threw = false;
        try
        {
            base.GetName();
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && owner.names == before + 1 && !owner.metadata, "refused output is never used; SDK does not retry or copy a name");
    }
    owner.result = ClockResult::Ok;
    owner.offered = nullptr;
    const auto before = owner.names;
    bool       nullRefused = false;
    try
    {
        base.GetName();
    }
    catch (const std::runtime_error&)
    {
        nullRefused = true;
    }
    Check(nullRefused && owner.names == before + 1, "an accepted null cannot masquerade as actual inline native name storage");
    NativeAnimationBlockAccess   unavailable;
    NativeAnimationBlockIdentity other;
    Receiver                     missing(unavailable, other);
    bool                         refused = false;
    try
    {
        missing.GetName();
    }
    catch (const std::runtime_error&)
    {
        refused = true;
    }
    Check(refused, "default native name interface is explicit refusal rather than wrapper CPU-buffer fallback");
    std::puts("Actual animation block borrowed name SDK fixture completed");
    std::printf("Passed %u animation block borrowed name SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
}
