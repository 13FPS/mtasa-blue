/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native save reads while discarding native Rain load writes
 *****************************************************************************/
#include "NativeWeatherRainBridge.h"
#include <intrin.h>
#include <cstring>
#include <limits>

namespace mta::game_de::NativeWeatherRainBridge
{
    namespace
    {
        bool Relative(std::uintptr_t source, std::uintptr_t target, std::int32_t& output) noexcept
        {
            if (target >= source)
            {
                const auto distance = target - source;
                if (distance > static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()))
                    return false;
                output = static_cast<std::int32_t>(distance);
                return true;
            }
            const auto distance = source - target;
            if (distance > static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()) + 1)
                return false;
            output = distance == static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()) + 1 ? std::numeric_limits<std::int32_t>::min()
                                                                                                           : -static_cast<std::int32_t>(distance);
            return true;
        }
    }

    bool Build(std::uintptr_t image, std::uintptr_t bridge, const Context* context, Plan& output) noexcept
    {
        static_assert(sizeof(std::uintptr_t) == 8, "The qualified serializer and generated bridge are Win64 only");
        if (!image || image % 8 || !bridge || !context || reinterpret_cast<std::uintptr_t>(context) % alignof(Context) ||
            image > std::numeric_limits<std::uintptr_t>::max() - NativeRainRva || context->nativeRain != image + NativeRainRva ||
            context->saveVtable != image + SaveVtableRva || context->loadVtable != image + LoadVtableRva ||
            context->saveSerialize != image + SaveSerializeRva || context->loadSerialize != image + LoadSerializeRva ||
            bridge > std::numeric_limits<std::uintptr_t>::max() - Plan{}.bridge.size())
            return false;

        const auto entry = image + EntryRva;
        if (bridge < entry + StockEntry.size() && bridge + Plan{}.bridge.size() > entry)
            return false;

        std::int32_t displacement = 0;
        if (!Relative(image + EntryRva + 5, bridge, displacement))
            return false;

        Plan plan{};
        // CALL creates an ordinary return to d55; the short jump bypasses the
        // entire original pointer/byteswap block after the compiled body.
        plan.entry = {0xe8, 0, 0, 0, 0, 0xeb, 0x5a, 0x90};
        std::memcpy(plan.entry.data() + 1, &displacement, sizeof(displacement));

        // mov rdx,rdi; mov rcx,context; mov rax,Body; jmp rax. There is no
        // push, stack adjustment or nested CALL in this generated leaf range.
        plan.bridge = {0x48, 0x8b, 0xd7, 0x48, 0xb9, 0, 0, 0, 0, 0, 0, 0, 0, 0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xe0};
        const auto descriptor = reinterpret_cast<std::uintptr_t>(context);
        const auto body = reinterpret_cast<std::uintptr_t>(&Body);
        std::memcpy(plan.bridge.data() + 5, &descriptor, sizeof(descriptor));
        std::memcpy(plan.bridge.data() + 15, &body, sizeof(body));
        output = plan;
        return true;
    }

    void Body(Context* context, void* archive)
    {
        InterlockedIncrement64(&context->activeCalls);
        __try
        {
            const auto* flags = reinterpret_cast<const unsigned char*>(archive) + ArchiveFlagsOffset;
            const bool  loading = (flags[0] & 1) != 0;
            const bool  swapping = (flags[1] & 0x20) != 0;
            auto*       native = reinterpret_cast<void*>(context->nativeRain);
            const auto  vtable = *reinterpret_cast<const std::uintptr_t*>(archive);
            const auto  serialize = *reinterpret_cast<const Serialize*>(vtable + SerializeSlotOffset);
            const bool  qualified = loading ? vtable == context->loadVtable && reinterpret_cast<std::uintptr_t>(serialize) == context->loadSerialize
                                            : vtable == context->saveVtable && reinterpret_cast<std::uintptr_t>(serialize) == context->saveSerialize;
            if (!qualified)
                InterlockedExchange64(&context->unqualifiedArchive, 1);

            // A load must still advance the actual archive and preserve its
            // errors/callbacks, but its value must not overwrite accepted MTA
            // intent. A four-byte local avoids an escaped heap accumulator.
            // Original swapped saves already use a synchronous stack borrow.
            std::uint32_t temporary = 0;
            void*         buffer = native;
            if ((qualified && loading) || (!loading && swapping))
                buffer = &temporary;
            if (!loading && swapping)
            {
                std::memcpy(&temporary, native, sizeof(temporary));
                temporary = _byteswap_ulong(temporary);
            }

            serialize(archive, buffer, sizeof(temporary));
            if (!qualified && loading && swapping)
            {
                // Unknown archives get the original native memory/byteswap
                // behavior, not an invented synchronous scratch borrow. The
                // latched fault must make the retaining owner refuse further
                // success and retire conservatively; it is not a Rain grant.
                std::memcpy(&temporary, native, sizeof(temporary));
                temporary = _byteswap_ulong(temporary);
                std::memcpy(native, &temporary, sizeof(temporary));
            }
        }
        __finally
        {
            // A virtual Serialize can propagate an exception through the
            // original native function. Retire its module/context borrow on
            // exceptional unwind as well as normal return; never replay it.
            InterlockedDecrement64(&context->activeCalls);
        }
    }
}
