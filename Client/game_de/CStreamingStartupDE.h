/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain MTA startup's actual streaming receiver with explicit limits
 ****************************************************************************/
#pragma once
#include "CStreamingDE.h"
#include <type_traits>

namespace mta::game_de
{
    // Initialize needs the existing special-model request after publication.
    // Keep every qualified SDK operation inherited from its real owner path;
    // this separate receiver refuses the unqualified full-service operations.
    class CStreamingStartupDE final : public CStreamingDE
    {
    public:
        explicit CStreamingStartupDE(NativeStreamingModelAccess& owner) noexcept : CStreamingDE(owner), owner(&owner) {}
        CStreamingStartupDE(const CStreamingStartupDE&) = delete;
        CStreamingStartupDE& operator=(const CStreamingStartupDE&) = delete;
        CStreamingStartupDE(CStreamingStartupDE&&) = delete;
        CStreamingStartupDE& operator=(CStreamingStartupDE&&) = delete;

        // CPU association only. Current native readiness still belongs to the
        // retained runtime; this check grants no engine/generation lease.
        bool IsOwnedBy(const NativeStreamingModelAccess& candidate) const noexcept { return owner == &candidate; }

        CStreamingInfo* GetStreamingInfo(uint32 id) override;
        void            ReinitStreaming() override;
        unsigned char   AddArchive(const wchar_t* path) override;
        void            RemoveArchive(unsigned char archive) override;
        void            SetStreamingInfo(unsigned int id, unsigned char stream, unsigned int offset, unsigned short size, unsigned int next = -1) override;
        bool            SetStreamingBufferSize(uint32 size) override;
        uint32          GetStreamingBufferSize() override;
        void            MakeSpaceFor(std::uint32_t bytes) override;
        std::uint32_t   GetMemoryUsed() const override;
        void            RemoveBigBuildings() override;

    private:
        [[noreturn]] static void          Unsupported(const char* operation);
        NativeStreamingModelAccess* const owner;
    };
    static_assert(std::is_abstract_v<CStreamingDE> && !std::is_abstract_v<CStreamingStartupDE>);
    static_assert(std::is_base_of_v<CStreaming, CStreamingStartupDE>);
}
