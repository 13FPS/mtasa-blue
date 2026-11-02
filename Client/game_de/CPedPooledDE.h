/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Publish an opaque entity interface only for an existing controlled ped
 ****************************************************************************/
#pragma once
#include <windows.h>
#include <SharedUtil.Defines.h>
#include "NativePedPoolReader.h"
#include "NativePedReferenceToken.h"
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471)
#endif
#include <game/CPed.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <type_traits>
namespace mta::game_de
{
    class CPoolsDE;
    class NativeGameTimingAccess;
    class CCamTargetDE;
    class CCameraTargetDE;

    // This mixin shares the actual virtual CPed base with CPedVitalsDE and
    // its later slices. It supplies neither a complete ped nor a native ped
    // allocator: the existing enclosing factory must register the real object.
    // The pool and native owner outlive that object, and the enclosing owner
    // serializes borrowed SDK use with registration withdrawal as CPoolsDE requires.
    class CPedPooledDE : public virtual CPed
    {
    public:
        CEntitySAInterface* GetInterface() override;
        ~CPedPooledDE() override = default;
        CPedPooledDE(const CPedPooledDE&) = delete;
        CPedPooledDE& operator=(const CPedPooledDE&) = delete;
        CPedPooledDE(CPedPooledDE&&) = delete;
        CPedPooledDE& operator=(CPedPooledDE&&) = delete;

    protected:
        CPedPooledDE(CPoolsDE& pools, NativeGameTimingAccess& owner) noexcept : registry(pools), nativeOwner(owner) {}

    private:
        friend class CCamTargetDE;
        friend class CCameraTargetDE;
        using CameraTargetReader = ClockResult (*)(const void*, std::uintptr_t&) noexcept;
        static ClockResult ReadRegisteredCameraTarget(CPoolsDE&, NativeGameTimingAccess&, CameraTargetReader, const void*, CEntity*&) noexcept;
        // Actual entity and camera getters share registry/token/native bookends.
        // No unchecked GetInterface call on a foreign CEntity is needed.
        static ClockResult ReadRegisteredIdentity(CPoolsDE&, NativeGameTimingAccess&, CPed*, NativePedPoolReader::Value&, NativePedReferenceToken&) noexcept;
        static bool        SameReference(const NativePedReferenceToken&, const NativePedReferenceToken&) noexcept;
        static bool        SameIdentity(const NativePedPoolReader::Value&, const NativePedPoolReader::Value&) noexcept;
        CPoolsDE&          registry;
        NativeGameTimingAccess& nativeOwner;
    };
    static_assert(std::is_abstract_v<CPedPooledDE> && std::is_base_of_v<CPed, CPedPooledDE>);
}
