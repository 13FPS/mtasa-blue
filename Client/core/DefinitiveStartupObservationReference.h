/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Composite read-only startup facts using the existing frame owner
 *****************************************************************************/
#pragma once
#include "DefinitiveApplicationFrameReference.h"
#include <game/CGameStartupObservation.h>

class CGameAdapterModule;
namespace DefinitiveStartupObservationReference
{
    // Keep the 128-word frame report byte-for-byte compatible. Startup metadata
    // is an additional diagnostic, never permission to run native StartGame.
    using namespace DefinitiveApplicationFrameReference;
    inline constexpr std::uint32_t ApiVersion = 1, FieldCount = 192;
    inline constexpr std::uint32_t MaximumDurationMs = 10000, ReadyTimeoutMs = 10000;
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using Request = DefinitiveApplicationFrameReference::Request;
    enum StartupField : std::uint32_t
    {
        StartupReads = 128,
        StartupStateValid,
        ObservationsLow,
        ObservationsHigh,
        RefusedObservationsLow,
        RefusedObservationsHigh,
        LoadingTypeObservedLow,
        LoadingTypeObservedHigh,
        LoadingCompatibleLow,
        LoadingCompatibleHigh,
        SimpleFadeTypeObservedLow,
        SimpleFadeTypeObservedHigh,
        SimpleFadeCompatibleLow,
        SimpleFadeCompatibleHigh,
        OwnerObservedLow,
        OwnerObservedHigh,
        WorldObservedLow,
        WorldObservedHigh,
        LastFacts,
        LastRefusals,
        StartupReadFailures,
        InvalidStartupSamples,
    };
    inline DefinitiveApplicationFrameReference::Snapshot FramePrefix(const Snapshot& values) noexcept
    {
        DefinitiveApplicationFrameReference::Snapshot prefix{};
        for (std::size_t i = 0; i < prefix.size(); ++i)
            prefix[i] = values[i];
        return prefix;
    }
    inline bool Retired(const Snapshot& values) noexcept
    {
        return DefinitiveApplicationFrameReference::Retired(FramePrefix(values));
    }
    inline bool Completed(const Snapshot& values) noexcept;
    inline void SetPair(Snapshot& values, StartupField low, std::uint64_t value) noexcept
    {
        values[low] = static_cast<std::uint32_t>(value);
        values[low + 1] = static_cast<std::uint32_t>(value >> 32);
    }
    inline void Increment(std::uint32_t& value) noexcept
    {
        if (value != 0xffffffffu)
            ++value;
    }
    inline bool ValidObservation(const mta::game_de::StartupObservationStateV1& state, std::uint64_t generation, std::uint32_t engineThread,
                                 const mta::game_de::StartupObservationStateV1* previous, bool validateFrame = true) noexcept
    {
        using namespace mta::game_de;
        const auto                                    facts = state.lastFacts;
        const auto                                    refusals = state.lastRefusals;
        DefinitiveApplicationFrameReference::Snapshot prefix{};
        if (state.size != sizeof(state) || state.version != StartupObservationVersionV1 || !generation || state.generation != generation ||
            state.observations != state.frame.frames || state.refusedObservations > state.observations ||
            state.loadingTypeObserved > state.observations - state.refusedObservations || state.loadingCompatible > state.loadingTypeObserved ||
            state.simpleFadeTypeObserved > state.observations - state.refusedObservations || state.simpleFadeCompatible > state.simpleFadeTypeObserved ||
            state.ownerObserved > state.observations - state.refusedObservations || state.worldObserved > state.ownerObserved || facts > 63 || refusals > 15 ||
            ((facts & StartupLoadingTypeMatches) && !(facts & StartupLoadingTypeObserved)) ||
            ((facts & StartupSimpleFadeTypeMatches) && !(facts & StartupSimpleFadeTypeObserved)) ||
            ((facts & StartupWorldLinkObserved) && !(facts & StartupGameModeOwnerObserved)) ||
            ((facts & StartupLoadingTypeObserved) && (refusals & StartupLoadingTypeRefused)) ||
            ((facts & StartupSimpleFadeTypeObserved) && (refusals & StartupSimpleFadeTypeRefused)) ||
            ((facts & StartupGameModeOwnerObserved) && (refusals & StartupGameModeOwnerRefused)) ||
            ((facts & StartupWorldLinkObserved) && (refusals & StartupWorldLinkRefused)) ||
            ((facts & StartupLoadingTypeObserved) && !state.loadingTypeObserved) || ((facts & StartupLoadingTypeMatches) && !state.loadingCompatible) ||
            ((facts & StartupSimpleFadeTypeObserved) && !state.simpleFadeTypeObserved) ||
            ((facts & StartupSimpleFadeTypeMatches) && !state.simpleFadeCompatible) || ((facts & StartupGameModeOwnerObserved) && !state.ownerObserved) ||
            ((facts & StartupWorldLinkObserved) && !state.worldObserved) || (!state.observations && (facts || refusals)) ||
            (state.observations && !facts && !refusals && !state.refusedObservations) ||
            ((facts || refusals) && ((!(facts & StartupLoadingTypeObserved) && !(refusals & StartupLoadingTypeRefused)) ||
                                     (!(facts & StartupSimpleFadeTypeObserved) && !(refusals & StartupSimpleFadeTypeRefused)) ||
                                     (!(facts & StartupGameModeOwnerObserved) && !(refusals & StartupGameModeOwnerRefused)) ||
                                     (!(facts & StartupWorldLinkObserved) && !(refusals & StartupWorldLinkRefused)))) ||
            (validateFrame && !DefinitiveApplicationFrameReference::CopyObservation(prefix, state.frame, generation, engineThread)))
            return false;
        for (const auto value : state.reserved)
            if (value)
                return false;
        if (previous && (state.observations < previous->observations || state.refusedObservations < previous->refusedObservations ||
                         state.loadingTypeObserved < previous->loadingTypeObserved || state.loadingCompatible < previous->loadingCompatible ||
                         state.simpleFadeTypeObserved < previous->simpleFadeTypeObserved || state.simpleFadeCompatible < previous->simpleFadeCompatible ||
                         state.ownerObserved < previous->ownerObserved || state.worldObserved < previous->worldObserved))
            return false;
        return true;
    }
    inline bool Completed(const Snapshot& values) noexcept
    {
        if (!DefinitiveApplicationFrameReference::Completed(FramePrefix(values)) || values[StartupStateValid] != 1 || values[StartupReads] < 2 ||
            !(values[ObservationsLow] || values[ObservationsHigh]) || values[StartupReadFailures] || values[InvalidStartupSamples])
            return false;
        for (std::size_t i = 88; i < 96; ++i)
            if (values[i])
                return false;
        for (std::size_t i = 125; i < 128; ++i)
            if (values[i])
                return false;
        for (std::size_t i = 150; i < values.size(); ++i)
            if (values[i])
                return false;
        const auto pair = [&](std::uint32_t index) noexcept
        { return static_cast<std::uint64_t>(values[index]) | (static_cast<std::uint64_t>(values[index + 1]) << 32); };
        // The worker evaluates a copied report after Core publishes it. Reuse
        // extended schema validation rather than trusting a success bit alone.
        // The original prefix owns frame validity; its EngineThread can already
        // be cleared by retirement, so do not recertify that old frame identity.
        mta::game_de::StartupObservationStateV1 state;
        state.generation = state.frame.generation = pair(GenerationLow);
        state.frame.frames = pair(FramesLow);
        state.frame.engineThread = values[EngineThread];
        state.frame.nativeState = values[RawNativeState];
        state.frame.frontendFrames = pair(FrontendFramesLow);
        state.frame.playingFrames = pair(PlayingFramesLow);
        state.frame.loadingLookups = pair(LoadingLookupsLow);
        state.frame.loadingKeyPresent = pair(LoadingKeyPresentLow);
        state.frame.loadingNonNull = pair(LoadingNonNullLow);
        state.frame.simpleFadeLookups = pair(SimpleFadeLookupsLow);
        state.frame.simpleFadeKeyPresent = pair(SimpleFadeKeyPresentLow);
        state.frame.simpleFadeNonNull = pair(SimpleFadeNonNullLow);
        state.frame.lookupFailures = pair(LookupFailuresLow);
        state.frame.lastLookupMask = values[LastLookupMask];
        state.frame.deltaBits = values[DeltaBits];
        state.observations = pair(ObservationsLow);
        state.refusedObservations = pair(RefusedObservationsLow);
        state.loadingTypeObserved = pair(LoadingTypeObservedLow);
        state.loadingCompatible = pair(LoadingCompatibleLow);
        state.simpleFadeTypeObserved = pair(SimpleFadeTypeObservedLow);
        state.simpleFadeCompatible = pair(SimpleFadeCompatibleLow);
        state.ownerObserved = pair(OwnerObservedLow);
        state.worldObserved = pair(WorldObservedLow);
        state.lastFacts = values[LastFacts];
        state.lastRefusals = values[LastRefusals];
        return ValidObservation(state, state.generation, values[EngineThread], nullptr, false);
    }
    namespace Detail
    {
        struct Publication
        {
            Snapshot                                  values{};
            DefinitiveInputReference::PublishCallback publish = nullptr;
        };
        inline thread_local Publication* currentPublication = nullptr;
        inline void                      Publish(const std::uint32_t* values, std::uint32_t count)
        {
            auto* current = currentPublication;
            if (!current || count != DefinitiveApplicationFrameReference::FieldCount)
                throw 1;
            for (std::size_t i = 0; i < count; ++i)
                current->values[i] = values[i];
            // Let publisher faults reach the existing owner's exception path;
            // cleanup still runs before this stack-owned context can disappear.
            if (current->publish)
                current->publish(current->values.data(), FieldCount);
        }
        struct PublicationScope
        {
            Publication* prior = currentPublication;
            explicit PublicationScope(Publication& value) noexcept { currentPublication = &value; }
            ~PublicationScope() { currentPublication = prior; }
        };
        template <typename Owner>
        struct FrameOwner
        {
            Owner&                                  owner;
            Publication&                            publication;
            mta::game_de::StartupObservationStateV1 previous;
            bool                                    havePrevious = false;
            std::uint32_t                           engineThread = 0;
            bool                                    Begin(const mta::game_de::ClockServiceRequestV1& request, mta::game_de::ClockServiceStateV1& state)
            {
                const bool result = owner.Begin(request, state);
                engineThread = state.engineThread;
                return result;
            }
            bool Renew(std::uint64_t generation) { return owner.Renew(generation); }
            bool Read(mta::game_de::ClockServiceStateV1& state)
            {
                const bool result = owner.Read(state);
                if (result)
                    engineThread = state.engineThread;
                return result;
            }
            bool ReadObservation(std::uint64_t generation, mta::game_de::ApplicationFrameObservationStateV1& output)
            {
                auto& values = publication.values;
                Increment(values[StartupReads]);
                mta::game_de::StartupObservationStateV1 state;
                if (!owner.ReadObservation(generation, state))
                {
                    values[StartupStateValid] = 0;
                    Increment(values[StartupReadFailures]);
                    return false;
                }
                if (!ValidObservation(state, generation, engineThread, havePrevious ? &previous : nullptr))
                {
                    values[StartupStateValid] = 0;
                    Increment(values[InvalidStartupSamples]);
                    return false;
                }
                previous = state;
                havePrevious = true;
                values[StartupStateValid] = 1;
                SetPair(values, ObservationsLow, state.observations);
                SetPair(values, RefusedObservationsLow, state.refusedObservations);
                SetPair(values, LoadingTypeObservedLow, state.loadingTypeObserved);
                SetPair(values, LoadingCompatibleLow, state.loadingCompatible);
                SetPair(values, SimpleFadeTypeObservedLow, state.simpleFadeTypeObserved);
                SetPair(values, SimpleFadeCompatibleLow, state.simpleFadeCompatible);
                SetPair(values, OwnerObservedLow, state.ownerObserved);
                SetPair(values, WorldObservedLow, state.worldObserved);
                values[LastFacts] = state.lastFacts;
                values[LastRefusals] = state.lastRefusals;
                output = state.frame;
                return true;
            }
            bool End(std::uint64_t generation) { return owner.End(generation); }
            bool Reset() { return owner.Reset(); }
            bool Focused() { return owner.Focused(); }
        };
    }
    template <typename Owner, typename Clock>
    Snapshot RunLifecycle(Owner& owner, Clock& clock, const Request& request) noexcept
    {
        if (Detail::currentPublication)
        {
            // A publisher may reenter Core. Refuse that nested workload without
            // replacing the active owner's callback context or admitting Begin.
            Snapshot refused{};
            refused[Version] = ApiVersion;
            refused[Phase] = 2;
            refused[CleanupComplete] = 1;
            refused[Reason] = static_cast<std::uint32_t>(StopReason::InvalidRequest);
            return refused;
        }
        Detail::Publication publication;
        publication.publish = request.publish;
        Detail::PublicationScope  scope(publication);
        Detail::FrameOwner<Owner> frameOwner{owner, publication};
        const Request             frameRequest{request.durationMs, request.cancelled, request.publish ? &Detail::Publish : nullptr};
        const auto                prefix = DefinitiveApplicationFrameReference::RunLifecycle(frameOwner, clock, frameRequest);
        for (std::size_t i = 0; i < prefix.size(); ++i)
            publication.values[i] = prefix[i];
        return publication.values;
    }
    Snapshot Run(CGameAdapterModule& adapter, const DefinitiveInputReference::Request& request) noexcept;
    Snapshot PublishNotEntered(const DefinitiveInputReference::Request& request, bool adapterStopped) noexcept;
}
