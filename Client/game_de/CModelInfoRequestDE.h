/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's real model request and wrapper-reference orchestration
 *****************************************************************************/
#pragma once

#include "CModelInfoDE.h"
#include <cstdint>
#include <type_traits>

class CGame;
class CStreaming;

namespace mta::game_de
{
    // A partial actual CModelInfo SDK layer. This owns only the classic wrapper
    // DWORD counters, not DE model metadata, a native lease or a second engine
    // owner. Missing render/dependency/reference effects remain mandatory.
    class CModelInfoRequestDE : public CModelInfoDE
    {
    public:
        void Request(EModelRequestType type, const char* tag) override;
        bool IsLoaded() override;
        void ModelAddRef(EModelRequestType type, const char* tag) override;
        void RemoveRef() override;
        int  GetRefCount() override;

        // A positive wrapper count has one backing interface reference; a
        // copied facade cannot duplicate that obligation without acquisition.
        CModelInfoRequestDE(const CModelInfoRequestDE&) = delete;
        CModelInfoRequestDE& operator=(const CModelInfoRequestDE&) = delete;

    protected:
        // The actual game and classification owner must outlive the facade.
        // Construction performs no native query, request or reference effect.
        CModelInfoRequestDE(std::uint32_t model, NativeModelClassificationAccess& classification, CGame& game) noexcept
            : CModelInfoDE(model, classification), game(game)
        {
        }

        // This must qualify BOTH real streaming readiness and, for DFFs, usable
        // render data. A non-null metadata/bundle or raw loaded byte is not the
        // missing second gate; native owner loss must refuse, not return false.
        virtual bool DoIsLoaded() = 0;
        // Preserve the actual vehicle-animation/skin7 dependencies before the
        // selected streaming request. No default successful no-op is supplied.
        virtual void RequestModelDependencies(EModelRequestType type, const char* tag) = 0;
        virtual void ReportBlockingModelLoad(bool loaded, int attempts, const char* tag) = 0;
        // These genuine interface effects must freshly qualify their native
        // lifetime and custom-clump/custom-collision policy. Neither the CPU
        // count nor DE's separate sparse DWORD map is a classic ushort field.
        // An entered failure must retire that existing owner so a later
        // DoIsLoaded cannot admit replay of an unacknowledged native effect.
        virtual void IncrementNativeModelInterfaceReference() = 0;
        virtual void RemoveNativeModelInterfaceReference() = 0;

    private:
        CStreaming&   RequireStreaming();
        CGame&        game;
        std::uint32_t references = 0;
        std::uint32_t pendingInterfaceReference = 0;
    };
    static_assert(sizeof(int) == sizeof(std::uint32_t));
    static_assert(std::is_abstract_v<CModelInfoRequestDE> && std::is_base_of_v<CModelInfo, CModelInfoRequestDE>);
}
