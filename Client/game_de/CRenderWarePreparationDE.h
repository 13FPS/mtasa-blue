/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own MTA's real CPU texture registry for DE renderer preparation
 ****************************************************************************/
#pragma once
#include <game/CShaderTextureWatch.h>
#include <game/CTextureWatchInitializationPolicy.h>
#include <memory>

namespace mta::game_de
{
    // This is exactly the CPU registry stage performed by SA Initialize. It
    // supplies no CRenderWare/CRenderer factory, native texture traversal,
    // rendering resource, callback admission or model draw permission.
    class CRenderWarePreparationDE final
    {
    public:
        CRenderWarePreparationDE() = default;
        CRenderWarePreparationDE(const CRenderWarePreparationDE&) = delete;
        CRenderWarePreparationDE& operator=(const CRenderWarePreparationDE&) = delete;

        void Initialize()
        {
            MtaGame::CTextureWatchInitializationPolicy::Initialize(
                [this] { return MapContains(dataTextures, FAKE_D3DTEXTURE_NO_TEXTURE); },
                [this] { return CreateTexInfo(FAKE_RWTEXTURE_NO_TEXTURE, FAKE_NAME_NO_TEXTURE, FAKE_D3DTEXTURE_NO_TEXTURE); },
                [this](STexInfo* texture) { matching.InsertTexture(texture); });
        }

        STexInfo*             GetNoTexture() const { return MapFindRef(dataTextures, FAKE_D3DTEXTURE_NO_TEXTURE); }
        CMatchChannelManager& Matching() noexcept { return matching; }

    private:
        // Each node below is owned by these actual matching maps. This DE
        // owner deletes metadata only; the opaque shader/entity tags are
        // never dereferenced or released as native/GPU resources.
        class OwnedMatching final : public CMatchChannelManager
        {
        public:
            ~OwnedMatching()
            {
                for (auto* channel : m_CreatedChannelList)
                    delete channel;
                for (const auto& entry : m_AllTextureList)
                    delete entry.second;
                for (const auto& entry : m_ShaderInfoMap)
                    delete entry.second;
            }
        };

        STexInfo* CreateTexInfo(const STexTag& tag, const SString& name, CD3DDUMMY* data)
        {
            // Retain the record before publishing either source map. Once
            // dataTextures contains it, a later matching failure must not
            // destroy it or cause Initialize to manufacture another record.
            records.emplace_back(std::make_unique<STexInfo>(tag, name, data));
            auto* texture = records.back().get();
            MapInsert(textures, texture->texTag.m_usTxdId, texture);
            MapSet(dataTextures, texture->pD3DData, texture);
            return texture;
        }

        // Declare matching last so its name/channel metadata dies before
        // the stable STexInfo records referenced by that metadata.
        std::vector<std::unique_ptr<STexInfo>> records;
        std::multimap<ushort, STexInfo*>       textures;
        CFastHashMap<CD3DDUMMY*, STexInfo*>    dataTextures;
        OwnedMatching                          matching{};
    };
}
