/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual shared shader matching and no-texture preparation
 ****************************************************************************/
#include "../../Client/game_de/CRenderWarePreparationDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>
#include <vector>

unsigned NextActualSharedShaderChannelId();

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    struct Failure : std::runtime_error
    {
        explicit Failure(unsigned stage) : std::runtime_error("test callback refused"), stage(stage) {}
        unsigned stage;
    };
    template <class Action>
    void FailsAt(Action&& action, unsigned stage)
    {
        try
        {
            action();
        }
        catch (const Failure& original)
        {
            Check(original.stage == stage, "original callback refusal propagates without translation");
            return;
        }
        Check(false, "callback refusal must not become success");
    }
    template <class Tag>
    Tag* Opaque(std::uintptr_t value)
    {
        return reinterpret_cast<Tag*>(value);
    }
    void OwnedPreparationChecks()
    {
        static_assert(!std::is_copy_constructible_v<CRenderWarePreparationDE>);
        static_assert(!std::is_base_of_v<CRenderWare, CRenderWarePreparationDE>);
        static_assert(!std::is_abstract_v<CRenderWarePreparationDE>);
        CMatchChannel first;
        const auto    secondId = NextActualSharedShaderChannelId();
        CMatchChannel third;
        Check(secondId == first.m_uiId + 1 && third.m_uiId == secondId + 1,
              "actual channel counter storage is shared across two separately compiled consumers");
        for (unsigned owner = 0; owner < 3; ++owner)
        {
            CRenderWarePreparationDE preparation;
            Check(!preparation.GetNoTexture(), "construction does not perform later initialization");
            preparation.Initialize();
            auto* texture = preparation.GetNoTexture();
            Check(texture && reinterpret_cast<std::intptr_t>(texture->pD3DData) == -9 && !texture->texTag.m_bUsingTxdId && texture->texTag.m_usTxdId == 0 &&
                      reinterpret_cast<std::intptr_t>(texture->texTag.m_pTex) == -10 && texture->strTextureName == "unnamed",
                  "real STexInfo preserves the full-width source data -9, texture tag -10 and lowercase unnamed metadata");
            Check(texture->pAssociatedTexNameInfo && texture->pAssociatedTexNameInfo->strTextureName == "unnamed" &&
                      texture->pAssociatedTexNameInfo->usedByTexInfoList.size() == 1 &&
                      MapContains(texture->pAssociatedTexNameInfo->usedByTexInfoList, texture),
                  "actual matching insertion associates the owned record with the real texture-name registry");
            auto* identity = texture->pAssociatedTexNameInfo;
            for (unsigned repeat = 0; repeat < 4; ++repeat)
            {
                preparation.Initialize();
                Check(preparation.GetNoTexture() == texture && texture->pAssociatedTexNameInfo == identity && identity->usedByTexInfoList.size() == 1,
                      "explicit repeat uses actual registry membership rather than replacing, duplicating or replaying the record");
            }
        }
        std::puts("Production actual owned no-texture registry preparation fixture completed");
    }
    void ActualMatchingChecks()
    {
        CRenderWarePreparationDE preparation;
        preparation.Initialize();
        auto* texture = preparation.GetNoTexture();
        auto& matching = preparation.Matching();
        auto  layers = [&]() -> SShaderItemLayers& { return matching.GetShaderForTexAndEntity(texture, nullptr, TYPE_MASK_WORLD)->output; };
        Check(!layers().pBase && layers().layerList.empty(), "actual empty registry matching has no fabricated shader");
        auto* base = Opaque<CSHADERDUMMY>(0x1001);
        auto* named = Opaque<CSHADERDUMMY>(0x1002);
        auto* lowLayer = Opaque<CSHADERDUMMY>(0x1003);
        auto* highLayer = Opaque<CSHADERDUMMY>(0x1004);
        auto* entityShader = Opaque<CSHADERDUMMY>(0x1005);
        auto* entity = Opaque<CClientEntityBase>(0x2001);
        auto* unknown = Opaque<CClientEntityBase>(0x2002);
        matching.AppendAdditiveMatch(base, nullptr, SStringX("*"), 10, false, TYPE_MASK_ALL, 1, true, true);
        Check(layers().pBase == base && layers().bUsesVertexShader, "existing wildcard matching applies to the actual unnamed entry");
        matching.AppendAdditiveMatch(named, nullptr, SStringX("UNNAMED"), 20, false, TYPE_MASK_WORLD, 2, false, true);
        Check(layers().pBase == named, "existing case normalization, exact name match and shader priority remain active");
        matching.AppendSubtractiveMatch(named, nullptr, SStringX("unnamed"));
        Check(layers().pBase == base, "existing subtractive matching removes the named replacement while retaining wildcard fallback");
        matching.AppendAdditiveMatch(lowLayer, nullptr, SStringX("unn*"), 5, true, TYPE_MASK_WORLD, 3, false, true);
        matching.AppendAdditiveMatch(highLayer, nullptr, SStringX("u?named"), 7, true, TYPE_MASK_WORLD, 4, true, true);
        auto& combined = layers();
        Check(combined.pBase == base && combined.layerList.size() == 2 && combined.layerList[0] == lowLayer && combined.layerList[1] == highLayer &&
                  combined.bUsesVertexShader,
              "actual matching retains question/star wildcards, layer priority order and vertex shader aggregation");
        matching.AppendAdditiveMatch(entityShader, entity, SStringX("unnamed"), 50, false, TYPE_MASK_PED, 5, false, true);
        Check(matching.GetShaderForTexAndEntity(texture, entity, TYPE_MASK_PED)->output.pBase == entityShader &&
                  matching.GetShaderForTexAndEntity(texture, unknown, TYPE_MASK_PED)->output.pBase == base,
              "actual entity-specific replacement and unknown-entity fallback preserve original matching identity");
        matching.RemoveClientEntityRefs(entity);
        Check(matching.GetShaderForTexAndEntity(texture, entity, TYPE_MASK_PED)->output.pBase == base,
              "actual entity reference removal restores the nonentity replacement");
        for (auto* shader : std::array<CSHADERDUMMY*, 5>{{named, lowLayer, highLayer, entityShader, base}})
            matching.RemoveShaderRefs(shader);
        Check(!layers().pBase && layers().layerList.empty() && !layers().bUsesVertexShader,
              "actual shader removal invalidates replacement caches without destroying the owned no-texture record");
        Check(preparation.GetNoTexture() == texture && texture->pAssociatedTexNameInfo->usedByTexInfoList.size() == 1,
              "matching changes retain the real registry owner and its stable STexInfo association");
        std::puts("Production actual shared shader texture matching fixture completed");
    }
    void SourcePrefixChecks()
    {
        for (unsigned failed = 1; failed <= 4; ++failed)
        {
            std::unique_ptr<STexInfo>           record;
            CRenderWarePreparationDE            owner;
            CFastHashMap<CD3DDUMMY*, STexInfo*> published;
            unsigned                            creates = 0, inserts = 0;
            volatile unsigned                   refuseAt = failed;
            std::vector<unsigned>               trace;
            auto                                initialize = [&]
            {
                MtaGame::CTextureWatchInitializationPolicy::Initialize(
                    [&]
                    {
                        trace.push_back(1);
                        if (refuseAt == 1)
                            throw Failure(1);
                        return MapContains(published, FAKE_D3DTEXTURE_NO_TEXTURE);
                    },
                    [&]
                    {
                        trace.push_back(2);
                        ++creates;
                        record = std::make_unique<STexInfo>(FAKE_RWTEXTURE_NO_TEXTURE, SStringX("unnamed"), FAKE_D3DTEXTURE_NO_TEXTURE);
                        MapSet(published, record->pD3DData, record.get());
                        if (refuseAt == 2)
                            throw Failure(2);
                        return record.get();
                    },
                    [&](STexInfo* texture)
                    {
                        trace.push_back(3);
                        ++inserts;
                        if (refuseAt == 3)
                            throw Failure(3);
                        owner.Matching().InsertTexture(texture);
                        if (refuseAt == 4)
                            throw Failure(4);
                    });
            };
            FailsAt(initialize, failed);
            Check(trace == (failed == 1   ? std::vector<unsigned>{1}
                            : failed == 2 ? std::vector<unsigned>{1, 2}
                                          : std::vector<unsigned>{1, 2, 3}),
                  "source callbacks retain exactly contains/create/publish/insert order before original refusal");
            if (failed == 1)
                Check(!record && !creates && !inserts && published.empty(), "contains refusal precedes all allocation and publication");
            else
                Check(record && creates == 1 && MapFindRef(published, FAKE_D3DTEXTURE_NO_TEXTURE) == record.get() &&
                          (record->pAssociatedTexNameInfo != nullptr) == (failed == 4),
                      "published STexInfo and actual accepted matching insertion survive the later refusal without rollback");
            refuseAt = 0;
            trace.clear();
            initialize();
            Check(trace == (failed == 1 ? std::vector<unsigned>{1, 2, 3} : std::vector<unsigned>{1}) && creates == 1,
                  "only a fresh absent membership runs creation; an accepted published prefix is never automatically retried");
        }
        std::puts("Production actual texture registry initialization prefix fixture completed");
    }
}

int main()
{
    try
    {
        OwnedPreparationChecks();
        ActualMatchingChecks();
        SourcePrefixChecks();
        std::printf("Passed %u actual texture registry preparation checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return EXIT_FAILURE;
    }
}
