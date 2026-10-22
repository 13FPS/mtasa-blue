/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve real scene effects with fresh typed world/settings checks
 ****************************************************************************/
#include "NativeStreamingSceneCalls.h"
#include "NativeStreamingSceneProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeClassChainAccess.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
namespace mta::game_de::NativeStreamingSceneCalls
{
    namespace SceneDetail
    {
        namespace P = NativeStreamingSceneProfile;
        namespace D = NativeStreamingCompletionContext;
        struct Value
        {
            D::Value       graph;
            std::uintptr_t persistentLevel = 0, settings = 0, settingsClass = 0, target = 0;
        };
        bool Profile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> bytes{};
                if (!NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return D::Profile(image);
        }
        const NativeUObjectMembershipAccess::Snapshot* Member(const D::Value& graph, std::uintptr_t object) noexcept
        {
            for (std::size_t index = 0; index < graph.count; ++index)
                if (graph.members[index].objectAddress == object)
                    return &graph.members[index];
            return nullptr;
        }
        ClockResult Capture(std::uintptr_t image, Value& output) noexcept
        {
            Value      next;
            const auto result = D::Capture(image, next.graph);
            if (result != ClockResult::Ok)
                return result;
            // The exact native entry follows CURRENT UWorld.PersistentLevel
            // then verifies GTAWorldSettings before using its native target.
            // The game-mode outer level and persistent level need not be the
            // same pointer; each is independently observed and typed here.
            if (!D::At(next.graph.world, P::WorldLevelOffset, next.persistentLevel) || !next.persistentLevel ||
                !D::Writable(next.persistentLevel, P::LevelSettingsOffset + 8) || !D::Add(image, next.persistentLevel, next.graph) ||
                !D::At(next.persistentLevel, P::LevelSettingsOffset, next.settings) || !next.settings ||
                !D::Writable(next.settings, P::SettingsTargetOffset + 8) || !D::Add(image, next.settings, next.graph) ||
                !NativeImageAccess::ReadImage(image, image + P::SettingsClassRva, &next.settingsClass, 8, true) || !next.settingsClass ||
                !D::Add(image, next.settingsClass, next.graph) || !D::At(next.settings, P::SettingsTargetOffset, next.target) ||
                !D::Writable(next.target, P::TargetBytes))
                return ClockResult::BindingChanged;
            for (std::size_t index = 0; index < next.graph.count; ++index)
                if (!D::Add(image, next.graph.members[index].classAddress, next.graph))
                    return ClockResult::BindingChanged;
            const auto*            level = Member(next.graph, next.persistentLevel);
            const auto*            settings = Member(next.graph, next.settings);
            bool                   levelMatches = false, settingsMatch = false;
            NativeClassChainAccess classes(&D::Copy);
            if (!level || !settings || !classes.IsChildOf(level->classAddress, next.graph.levelClass, levelMatches) || !levelMatches ||
                !classes.IsChildOf(settings->classAddress, next.settingsClass, settingsMatch) || !settingsMatch || !D::Fence(image, next.graph))
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
        bool Same(const Value& first, const Value& next, bool mode) noexcept
        {
            return first.persistentLevel == next.persistentLevel && first.settings == next.settings && first.settingsClass == next.settingsClass &&
                   first.target == next.target && D::Same(first.graph, next.graph, mode);
        }
        ClockResult Position(const CVector* source, CVector& output) noexcept
        {
            static_assert(sizeof(CVector) == 12 && std::is_trivially_copyable_v<CVector>);
            if (!source)
                return ClockResult::InvalidArgument;
            CVector first, second;
            if (!D::Copy(reinterpret_cast<std::uintptr_t>(source), &first, sizeof(first)) ||
                !D::Copy(reinterpret_cast<std::uintptr_t>(source), &second, sizeof(second)))
                return ClockResult::ReadFailure;
            if (std::memcmp(&first, &second, sizeof(first)))
                return ClockResult::BindingChanged;
            for (const float component : {first.fX, first.fY, first.fZ})
                if (!std::isfinite(component) ||
                    std::fabs(static_cast<double>(component)) * P::CoordinateScale > static_cast<double>((std::numeric_limits<float>::max)()))
                    return ClockResult::InvalidArgument;
            output = first;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_STREAMING_SCENE_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, const CVector* position, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(position);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }
    ClockResult Load(std::uintptr_t image, const CVector* source, CheckOwner owner, Receipt* output, Invoke invoke) noexcept
    {
        namespace D = NativeStreamingCompletionContext;
        Receipt receipt;
        receipt.entryRva = NativeStreamingSceneProfile::EntryRva;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        CVector position;
        auto    result = SceneDetail::Position(source, position);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.position = {position.fX, position.fY, position.fZ};
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        if (!SceneDetail::Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        SceneDetail::Value first, confirmed;
        if ((result = SceneDetail::Capture(image, first)) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok ||
            (result = SceneDetail::Capture(image, confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!SceneDetail::Same(first, confirmed, true) || !D::Fence(image, first.graph) || !SceneDetail::Profile(image))
            return finish(ClockResult::BindingChanged);
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.root = first.graph.root;
        receipt.gameMode = first.graph.gameMode;
        receipt.level = first.graph.level;
        receipt.world = first.graph.world;
        receipt.persistentLevel = first.persistentLevel;
        receipt.settings = first.settings;
        receipt.settingsClass = first.settingsClass;
        receipt.target = first.target;
        receipt.modeBefore = first.graph.mode;
#if defined(MTA_DE_STREAMING_SCENE_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_STREAMING_SCENE_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &SceneDetail::NativeCall;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeFunction>(image + receipt.entryRva), &position, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        // Genuine scene loading removes/requests models, changes stream flags,
        // services callbacks and writes the scene target. Reacquire the whole
        // current graph before acknowledging; never replay an old scene/queue.
        if ((result = D::Owner(image, owner)) != ClockResult::Ok || (result = SceneDetail::Capture(image, confirmed)) != ClockResult::Ok)
            return finish(result);
        receipt.modeAfter = confirmed.graph.mode;
        if (!SceneDetail::Same(first, confirmed, false) || !D::Fence(image, confirmed.graph) || !SceneDetail::Profile(image))
            return finish(ClockResult::BindingChanged);
        return finish(NativeStartGameCalls::CheckThread(image));
    }
}
