/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual name/block factory composes source key and current hash search
 ****************************************************************************/
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CAnimBlockDE.h"
#include "../../Client/game_de/CAnimBlendHierarchyDE.h"
#include "../../Client/game_de/CAnimBlendSequenceDE.h"
#include "../../Client/game_de/CKeyGenDE.h"
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <initializer_list>
using namespace mta::game_de;
#include "PrivateAnimationFactoryFixture.generated.h"
namespace
{
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Block name SDK check %u failed\n", checks);
            std::exit(1);
        }
    }
    template <class Action>
    void Refuses(Action action, ClockResult expected)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused);
    }
    struct Owner final : NativeAnimationManagerAccess, NativeAnimationBlockAccess, NativeAnimationHierarchyAccess
    {
        const std::uint64_t          generation = (std::numeric_limits<std::uint64_t>::max)() - 23;
        const std::uintptr_t         row = (std::numeric_limits<std::uintptr_t>::max)() - 0x6000;
        NativeAnimationBlockIdentity identity{3, {'b', 'a', 'n', 'k', 0}, generation};
        std::uint32_t                received = 0;
        std::uintptr_t               found = row;
        unsigned                     finds = 0, resolves = 0, forbidden = 0;
        ClockResult                  findStatus = ClockResult::Ok;
        char*                        mutateName = nullptr;
        ClockResult FindAnimationBlockHierarchy(const NativeAnimationBlockIdentity& token, std::uint32_t hash, std::uintptr_t& output) noexcept override
        {
            ++finds;
            received = hash;
            if (token.ownerGeneration != generation || token.index != identity.index || token.name != identity.name)
                return ClockResult::BindingChanged;
            if (mutateName)
                *mutateName = '?';
            if (findStatus == ClockResult::Ok)
                output = found;
            return findStatus;
        }
        ClockResult ResolveBankAnimationHierarchyReference(std::uint64_t gen, std::uintptr_t raw,
                                                           NativeAnimationHierarchyReferenceToken& output) noexcept override
        {
            ++resolves;
            if (gen != generation || raw != row)
                return ClockResult::BindingChanged;
            output = {gen, raw};
            return ClockResult::Ok;
        }
        ClockResult ReadAnimationHierarchy(const NativeAnimationHierarchyReferenceToken&, NativeAnimationHierarchyValues& output) noexcept override
        {
            output.interfaceAddress = row;
            return ClockResult::Ok;
        }
        ClockResult ReadAnimationBlockHierarchy(const NativeAnimationBlockIdentity&, std::size_t, std::uintptr_t&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity&, NativeAnimationBlockValues&) noexcept override
        {
            ++forbidden;
            return ClockResult::NotInitialized;
        }
    };
    class Block final : public CAnimBlockDE
    {
    public:
        explicit Block(Owner& service) : CAnimBlockDE(service, service.identity), owner(service) {}
        void                   Request(EModelRequestType, bool) override { std::abort(); }
        CAnimBlockSAInterface* GetInterface() override
        {
            ++owner.forbidden;
            return nullptr;
        }

    private:
        Owner& owner;
    };
}
int main()
{
    Owner                                  owner;
    CKeyGenDE                              keys;
    PrivateAnimationFactoryManagerReceiver manager(owner, owner, owner, owner.generation);
    std::unique_ptr<CAnimBlock>            block = std::make_unique<Block>(owner);
    struct Case
    {
        const char* input;
        const char* normalized;
    };
    const Case cases[] = {{"", ""}, {"mIx_a9", "MIX_A9"}, {"miX\xe9\xff", "MIX\xe9\xff"}, {"az[]_09", "AZ[]_09"}};
    for (const auto& value : cases)
    {
        auto result = manager.GetAnimation(value.input, block);
        Check(result && owner.received == keys.GetKey(value.normalized));
        Check(result->GetInterface() == reinterpret_cast<CAnimBlendHierarchySAInterface*>(owner.row));
    }
    const std::string longName(4097, 'c'), longNormalized(4097, 'C');
    Check(manager.GetAnimation(longName.c_str(), block) && owner.received == keys.GetKey(longNormalized.c_str()));
    char embedded[] = {'m', 'i', 'x', 0, 't', 'a', 'i', 'l', 0};
    Check(manager.GetAnimation(embedded, block) && owner.received == keys.GetKey("MIX"));
    char copied[] = "cOpIeD";
    owner.mutateName = copied;
    Check(manager.GetAnimation(copied, block) && copied[0] == '?' && owner.received == keys.GetKey("COPIED"));
    owner.mutateName = nullptr;
    const auto beforeNull = owner.finds;
    bool       invalid = false;
    try
    {
        manager.GetAnimation(static_cast<const char*>(nullptr), block);
    }
    catch (const std::invalid_argument&)
    {
        invalid = true;
    }
    Check(invalid && owner.finds == beforeNull);
    owner.found = 0;
    const auto beforeMiss = owner.resolves;
    Check(!manager.GetAnimation("missing", block) && owner.received == keys.GetKey("MISSING"));
    Check(owner.resolves == beforeMiss);
    owner.found = owner.row;
    for (const auto status : {ClockResult::NotInitialized, ClockResult::ReadFailure, ClockResult::BindingChanged, ClockResult::WrongThread})
    {
        owner.findStatus = status;
        Refuses([&] { manager.GetAnimation("lookup", block); }, status);
        Check(owner.received == keys.GetKey("LOOKUP"));
    }
    owner.findStatus = ClockResult::Ok;
    PrivateAnimationFactoryManagerReceiver observer(owner, owner.generation);
    const auto                             beforeObserver = owner.finds;
    Refuses([&] { observer.GetAnimation("lookup", block); }, ClockResult::NotInitialized);
    Check(owner.finds == beforeObserver && owner.forbidden == 0);
    std::puts("Actual animation block name factory SDK fixture completed");
    std::printf("Passed %u block name SDK checks (%zu-bit; transport only, no native engine code)\n", checks, sizeof(void*) * 8);
    return 0;
}
