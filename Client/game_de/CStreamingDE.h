/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Use MTA's streaming SDK for qualified DE model requests and queries
 ****************************************************************************/
#pragma once
#include <windows.h>
#include "NativeStreamingModelAccess.h"
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <CVector.h>
#include <game/CStreaming.h>
#include <type_traits>

namespace mta::game_de
{
    // The complete streaming service still requires its native archive, memory,
    // and collision-scene operations. Keep those SDK methods pure rather
    // than publishing this slice as a complete CStreaming instance.
    class CStreamingDE : public CStreaming
    {
    public:
        void          RequestModel(DWORD model, DWORD flags) override;
        void          RequestSpecialModel(DWORD model, const char* name, DWORD channel) override;
        void          RemoveModel(std::uint32_t model) override;
        bool          HasModelLoaded(DWORD model) override;
        void          LoadAllRequestedModels(bool onlyPriority, const char* tag) override;
        void          LoadScene(const CVector* position) override;
        std::uint32_t GetNumPedsLoaded() const noexcept override;
        bool          IsModelInLoadedPedGroup(std::uint16_t model) const noexcept override;
        std::uint32_t GetNumLoadedVehicles() const noexcept override;
        bool          IsModelInLoadedVehicleGroup(std::uint16_t model) const noexcept override;

    protected:
        explicit CStreamingDE(NativeStreamingModelAccess& access) noexcept : access(access) {}

    private:
        static void                             Require(ClockResult result, const char* operation);
        NativeStreamingModelGroupsReader::Value ReadGroups() const noexcept;
        // Borrow the existing callback owner. This wrapper owns no native
        // model, streaming row, request queue or model-reference lease.
        NativeStreamingModelAccess& access;
    };
    static_assert(sizeof(DWORD) == sizeof(std::uint32_t));
    static_assert(std::is_abstract_v<CStreamingDE> && std::is_base_of_v<CStreaming, CStreamingDE>);
}
