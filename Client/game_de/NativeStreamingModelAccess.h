/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep MTA streaming intents with the existing admitted engine owner
 ****************************************************************************/
#pragma once
#include "NativeModelLoadedReader.h"
#include "NativeStreamingModelGroupsReader.h"

class CVector;

namespace mta::game_de
{
    class NativeStreamingModelAccess
    {
    public:
        virtual ~NativeStreamingModelAccess() = default;
        // An older owner cannot silently turn a synchronous loading intent
        // into acceptance. Priority intent remains distinct from loading all.
        virtual ClockResult ReadStreamingModelGroups(NativeStreamingModelGroupsReader::Value&) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult LoadStreamingModels(bool) noexcept { return ClockResult::NotInitialized; }
        // Scene completion has genuine world/settings effects beyond loading
        // model rows. An older owner must refuse this separate SDK intent.
        virtual ClockResult LoadStreamingScene(const CVector*) noexcept { return ClockResult::NotInitialized; }
        // Removal is a direct streaming intent. This does not decrement a
        // model wrapper/reference count or infer final-release ownership.
        virtual ClockResult RemoveStreamingModel(std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        // Older/read-only owners cannot turn a request into a successful no-op.
        // Native entry, partial effects and cleanup remain the runtime's concern.
        virtual ClockResult RequestStreamingModel(std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        // Special-model rename is destructive existing-slot work, not a model
        // factory or an ordinary request with an ignored name.
        virtual ClockResult RequestStreamingSpecialModel(std::uint32_t, const char*, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadStreamingModelLoaded(std::uint32_t, NativeModelLoadedReader::Value&) noexcept { return ClockResult::NotInitialized; }
        // Upgrade requests also own their paired model. Keeping that operation
        // distinct prevents an older owner from accepting only half the work.
        virtual ClockResult RequestStreamingUpgrade(std::uint32_t, std::uint32_t) noexcept { return ClockResult::NotInitialized; }
        virtual ClockResult ReadStreamingUpgradeLoaded(std::uint32_t, bool&) noexcept { return ClockResult::NotInitialized; }
    };
}
