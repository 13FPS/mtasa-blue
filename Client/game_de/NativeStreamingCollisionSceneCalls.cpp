/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep COL, IPL and blocking completion inside one admitted native owner
 ****************************************************************************/
#include "NativeStreamingCollisionSceneCalls.h"
#include "NativeStreamingCollisionSceneProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include <cstring>
#include <type_traits>

namespace mta::game_de::NativeStreamingCollisionSceneCalls
{
    namespace CollisionSceneDetail
    {
        namespace P = NativeStreamingCollisionSceneProfile;
        namespace D = NativeStreamingCompletionContext;

        bool Profile(std::uintptr_t image) noexcept
        {
            for (const auto& pin : P::Code)
            {
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                if (!NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            // The existing completion profile already pins the full native
            // LoadAll entry and its current world/owner dependencies. Keep the
            // same admission level as genuine full-scene loading; these whole
            // engine stages own their internal pool and region-tree traversal.
            return D::Profile(image);
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
            // Collision-only loading passes these bits to native comparisons.
            // Full-scene loading's separate UE coordinate scaling is not part
            // of this source operation and cannot impose an extra range here.
            std::memcpy(&output, &first, sizeof(output));
            return ClockResult::Ok;
        }

#if !defined(MTA_DE_STREAMING_COLLISION_SCENE_STORE_INVOKE)
        ClockResult NativeStoreCall(StoreFunction function, CVector* position, bool ignorePlayerVehicle, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(position, ignorePlayerVehicle);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
#if !defined(MTA_DE_STREAMING_COLLISION_SCENE_LOADALL_INVOKE)
        ClockResult NativeLoadAllCall(LoadAllFunction function, std::uint32_t& exception) noexcept
        {
            __try
            {
                function();
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

    ClockResult Load(std::uintptr_t image, const CVector* source, CheckOwner owner, Receipt* output, InvokeStore invokeStore,
                     InvokeLoadAll invokeLoadAll) noexcept
    {
        namespace P = NativeStreamingCollisionSceneProfile;
        namespace D = NativeStreamingCompletionContext;
        Receipt receipt;
        receipt.collision.entryRva = P::CollisionRva;
        receipt.ipl.entryRva = P::IplRva;
        receipt.loadAll.entryRva = P::LoadAllRva;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };

        CVector position;
        auto    result = CollisionSceneDetail::Position(source, position);
        if (result != ClockResult::Ok)
            return finish(result);
        std::memcpy(receipt.position.data(), &position, sizeof(position));
        // Both original store methods take CVector by value. Separate native
        // pointer arguments preserve that contract even if one stage changes
        // its own copy; caller mutation cannot tear the later stage's intent.
        CVector collisionPosition, iplPosition;
        std::memcpy(&collisionPosition, &position, sizeof(position));
        std::memcpy(&iplPosition, &position, sizeof(position));
        if (!owner)
            return finish(ClockResult::NotInitialized);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        if (!CollisionSceneDetail::Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        D::Value first, confirmed;
        if ((result = D::Capture(image, first)) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok ||
            (result = D::Capture(image, confirmed)) != ClockResult::Ok)
            return finish(result);
        if (!D::Same(first, confirmed, true) || !D::Fence(image, confirmed) || !CollisionSceneDetail::Profile(image))
            return finish(ClockResult::BindingChanged);
        // Capture/profile checks can consume the timing lease. Re-admit the
        // actual callback owner and state before entering the first stage.
        if ((result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);

        receipt.root = first.root;
        receipt.gameMode = first.gameMode;
        receipt.level = first.level;
        receipt.world = first.world;
        receipt.modeBefore = first.mode;

        const auto acknowledge = [&]() noexcept
        {
            auto status = D::Owner(image, owner);
            if (status != ClockResult::Ok || (status = D::Capture(image, confirmed)) != ClockResult::Ok)
                return status;
            receipt.modeAfter = confirmed.mode;
            // Native requests/removals/loading can change mode and queues.
            // Re-admit their current graph, retaining all preceding effects;
            // never restore an old graph or replay an accepted stage prefix.
            if (!D::Same(first, confirmed, false) || !D::Fence(image, confirmed) || !CollisionSceneDetail::Profile(image))
                return ClockResult::BindingChanged;
            // Close owner expiry/Stop/state changes after these bookends,
            // before the next stage or final successful acknowledgement.
            return D::Owner(image, owner);
        };

#if defined(MTA_DE_STREAMING_COLLISION_SCENE_STORE_INVOKE)
        const auto store = invokeStore ? invokeStore : &MTA_DE_STREAMING_COLLISION_SCENE_STORE_INVOKE;
#else
        const auto store = invokeStore ? invokeStore : &CollisionSceneDetail::NativeStoreCall;
#endif
#if defined(MTA_DE_STREAMING_COLLISION_SCENE_LOADALL_INVOKE)
        const auto loadAll = invokeLoadAll ? invokeLoadAll : &MTA_DE_STREAMING_COLLISION_SCENE_LOADALL_INVOKE;
#else
        const auto loadAll = invokeLoadAll ? invokeLoadAll : &CollisionSceneDetail::NativeLoadAllCall;
#endif

        receipt.entered = true;
        receipt.collision.entered = true;
        result = store(reinterpret_cast<StoreFunction>(image + P::CollisionRva), &collisionPosition, true, receipt.collision.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.collision.returned = true;
        if ((result = acknowledge()) != ClockResult::Ok)
            return finish(result);

        receipt.ipl.entered = true;
        result = store(reinterpret_cast<StoreFunction>(image + P::IplRva), &iplPosition, true, receipt.ipl.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.ipl.returned = true;
        if ((result = acknowledge()) != ClockResult::Ok)
            return finish(result);

        // Calling the public LoadAll primitive here would reacquire this same
        // gate and return Busy. Invoke the proved whole native entry under the
        // admission already held for the complete source-ordered operation.
        receipt.loadAll.entered = true;
        result = loadAll(reinterpret_cast<LoadAllFunction>(image + P::LoadAllRva), receipt.loadAll.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.loadAll.returned = true;
        receipt.returned = true;
        if ((result = acknowledge()) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
