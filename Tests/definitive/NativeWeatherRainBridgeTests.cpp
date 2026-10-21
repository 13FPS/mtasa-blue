/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Real compiled Rain body with a controlled synchronous archive
 *****************************************************************************/
#include "../../Client/game_de/NativeWeatherRainBridge.h"
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
    namespace Rain = mta::game_de::NativeWeatherRainBridge;
    int  checks = 0;
    bool Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            std::printf("FAIL %d: %s\n", checks, reason);
        return condition;
    }
    struct Archive final
    {
        const std::uintptr_t*           vtable = nullptr;
        std::array<unsigned char, 0x20> padding{};
        unsigned char                   flags[2]{};
    };
    static_assert(offsetof(Archive, flags) == Rain::ArchiveFlagsOffset);
    Rain::Context* current = nullptr;
    void*          native = nullptr;
    bool           sawNative = false, sawFour = false, sawCount = false, loading = false, nested = false, raise = false;
    std::uint32_t  passed = 0;
    unsigned       calls = 0;
    void           Serialize(void* archive, void* bytes, std::uint64_t length)
    {
        ++calls;
        sawNative = bytes == native;
        sawFour = length == 4;
        sawCount = current->activeCalls == (nested ? 2 : 1);
        std::memcpy(&passed, bytes, sizeof(passed));
        if (raise)
            RaiseException(0xe0f03117, 0, 0, nullptr);
        if (nested)
            return;
        if (loading)
        {
            const std::uint32_t loaded = 0x10203040;
            std::memcpy(bytes, &loaded, sizeof(loaded));
        }
        // A native virtual implementation can modify archive flags while it
        // runs. The bridge must not reinterpret the completed operation using
        // that later value or accidentally post-write the real Rain cell.
        reinterpret_cast<Archive*>(archive)->flags[0] ^= 1;
        reinterpret_cast<Archive*>(archive)->flags[1] ^= 0x20;
    }
    void NestedSerialize(void* archive, void* bytes, std::uint64_t length)
    {
        if (current->activeCalls == 1)
        {
            nested = true;
            Rain::Body(current, archive);
            nested = false;
            sawCount = sawCount && current->activeCalls == 1;
            return;
        }
        Serialize(archive, bytes, length);
    }
    bool Raised(Rain::Context* context, Archive* archive)
    {
        __try
        {
            Rain::Body(context, archive);
        }
        __except (GetExceptionCode() == 0xe0f03117 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }
}

int main()
{
    bool                                                                               ok = true;
    std::array<std::uintptr_t, Rain::SerializeSlotOffset / sizeof(std::uintptr_t) + 1> table{};
    table.back() = reinterpret_cast<std::uintptr_t>(&Serialize);
    Archive archive{};
    archive.vtable = table.data();
    std::uint32_t value = 0xa1b2c3d4;
    Rain::Context context{};
    context.nativeRain = reinterpret_cast<std::uintptr_t>(&value);
    context.saveVtable = context.loadVtable = reinterpret_cast<std::uintptr_t>(table.data());
    context.saveSerialize = context.loadSerialize = reinterpret_cast<std::uintptr_t>(&Serialize);
    current = &context;
    native = &value;
    for (unsigned load = 0; load != 2; ++load)
    {
        for (unsigned swap = 0; swap != 2; ++swap)
        {
            archive.flags[0] = static_cast<unsigned char>(load ? 1 : 2);
            archive.flags[1] = static_cast<unsigned char>(swap ? 0x20 : 0);
            calls = 0;
            loading = load != 0;
            Rain::Body(&context, &archive);
            ok &= Check(calls == 1 && sawFour && sawCount, "exactly one real typed archive call with four bytes and a live activity count");
            ok &= Check(sawNative == (!load && !swap), "only unswapped saves use the actual native Rain address");
            ok &= Check(value == 0xa1b2c3d4, "both discarded loads and both saves preserve the accepted native Rain value");
            if (!load)
                ok &= Check(passed == (swap ? 0xd4c3b2a1 : 0xa1b2c3d4), "save data is read from the real native scalar with the native endian rule");
            ok &= Check(context.activeCalls == 0, "normal return retires the full synchronous borrow");
            ok &= Check(context.unqualifiedArchive == 0, "only the exact admitted archive targets receive local load buffers");
        }
    }
    table.back() = reinterpret_cast<std::uintptr_t>(&NestedSerialize);
    context.saveSerialize = context.loadSerialize = reinterpret_cast<std::uintptr_t>(&NestedSerialize);
    archive.flags[0] = 2;
    archive.flags[1] = 0;
    loading = false;
    Rain::Body(&context, &archive);
    ok &= Check(sawCount && context.activeCalls == 0, "recursive synchronous serialization counts both frames and retires each once");

    table.back() = reinterpret_cast<std::uintptr_t>(&Serialize);
    context.saveSerialize = context.loadSerialize = reinterpret_cast<std::uintptr_t>(&Serialize);
    raise = true;
    ok &= Check(Raised(&context, &archive), "the virtual archive exception propagates without retry");
    ok &= Check(context.activeCalls == 0, "compiler-emitted finally retires exceptional borrow before propagation");
    raise = false;

    ++context.loadSerialize;
    archive.flags[0] = 1;
    archive.flags[1] = 0x20;
    loading = true;
    Rain::Body(&context, &archive);
    ok &= Check(context.unqualifiedArchive == 1 && sawNative && value == 0x40302010,
                "a foreign virtual target preserves native load/byteswap behavior and latches refused ownership");
    ok &= Check(context.activeCalls == 0, "foreign fallback retires activity without claiming suppression");

    constexpr std::uintptr_t image = 0x140000000;
    context.nativeRain = image + Rain::NativeRainRva;
    context.saveVtable = image + Rain::SaveVtableRva;
    context.loadVtable = image + Rain::LoadVtableRva;
    context.saveSerialize = image + Rain::SaveSerializeRva;
    context.loadSerialize = image + Rain::LoadSerializeRva;
    const auto returnAddress = image + Rain::EntryRva + 5;
    const auto shortTarget = returnAddress + 2 + 0x5a;
    ok &= Check(shortTarget == image + Rain::ContinuationRva, "normal body return skips the entire original unbuffered Rain block");
    Rain::Plan plan{};
    for (const auto bridge : {returnAddress + 0x10000, returnAddress + static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()),
                              returnAddress - (static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()) + 1)})
    {
        ok &= Check(Rain::Build(image, bridge, &context, plan), "valid signed-relative boundary produces one atomic entry and a leaf tail bridge");
        std::int32_t displacement = 0;
        std::memcpy(&displacement, plan.entry.data() + 1, sizeof(displacement));
        ok &=
            Check(static_cast<std::int64_t>(returnAddress) + displacement == static_cast<std::int64_t>(bridge), "entry CALL reaches the exact retained bridge");
        std::uintptr_t descriptor = 0, body = 0;
        std::memcpy(&descriptor, plan.bridge.data() + 5, sizeof(descriptor));
        std::memcpy(&body, plan.bridge.data() + 15, sizeof(body));
        ok &= Check(descriptor == reinterpret_cast<std::uintptr_t>(&context) && body == reinterpret_cast<std::uintptr_t>(&Rain::Body),
                    "leaf bridge forwards the exact retained context and compiled target");
        ok &= Check(plan.entry[5] == 0xeb && plan.entry[6] == 0x5a && plan.entry[7] == 0x90 && plan.bridge[23] == 0xff && plan.bridge[24] == 0xe0,
                    "entry continuation and bridge tail jump contain no synthetic return or stack adjustment");
    }
    const auto accepted = plan;
    for (const auto bridge : {returnAddress + static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()) + 1,
                              returnAddress - (static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()) + 2), image + Rain::EntryRva})
    {
        ok &= Check(!Rain::Build(image, bridge, &context, plan) && plan.entry == accepted.entry && plan.bridge == accepted.bridge,
                    "out-of-range and overlapping bridges refuse without publishing partial output");
    }
    ++context.nativeRain;
    ok &= Check(!Rain::Build(image, returnAddress + 0x10000, &context, plan) && plan.entry == accepted.entry && plan.bridge == accepted.bridge,
                "foreign Rain identity refuses without changing caller output");
    std::printf("Rain bridge primitive: %d checks, %s\n", checks, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
