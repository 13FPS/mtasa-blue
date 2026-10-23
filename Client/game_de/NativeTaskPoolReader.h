/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate native task identity without publishing SDK task lifetime
 ****************************************************************************/
#pragma once
#include "NativePedPoolReader.h"

namespace mta::game_de
{
    class NativeTaskPoolReader final
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        struct Value
        {
            std::uintptr_t pool = 0, objects = 0, flags = 0, nativeTask = 0, vtable = 0, deletingDestructor = 0, getTypeMethod = 0, isSimpleMethod = 0;
            std::uint32_t  slot = 0, nativeType = 0;
            std::int32_t   capacity = 0, cursor = 0;
            unsigned char  generation = 0, ownsAllocations = 0;
            bool           simple = false;
        };
        NativeTaskPoolReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(std::uintptr_t nativeTask, Value&) const noexcept;
        ClockResult Recheck(const Value&) const noexcept;

        // Only the three constructors/vtables qualified by native ped setup
        // and the exact water event response are admitted. Values are synchronous opaque observations,
        // never CTask/CTaskSAInterface pointers or a task lifetime lease. Native
        // deletion runs before manager cells and pool flags are cleared, and
        // seven-bit generations can wrap; caching this Value grants no later
        // operation or wrapper-publication authority.
    private:
        ClockResult Capture(std::uintptr_t, Value&) const noexcept;
        ClockResult Owner() const noexcept;
        Binding     binding;
        CheckOwner  owner = nullptr;
    };
}
