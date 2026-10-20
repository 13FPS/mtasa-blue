/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client tests
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/
#include "NativeFrontendASIFixture.h"
#include "../../Client/definitive/NativeFrontendHost.h"
#include <array>

namespace
{
    HANDLE NamedObject(const wchar_t* variable, bool mapping)
    {
        std::array<wchar_t, 256> name{};
        const auto               length = GetEnvironmentVariableW(variable, name.data(), static_cast<DWORD>(name.size()));
        if (!length || length >= name.size())
            return nullptr;
        return mapping ? OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.data()) : OpenEventW(EVENT_MODIFY_STATE, FALSE, name.data());
    }
}

extern "C" __declspec(dllexport) int DE_NativeFrontendStartV1(const deonline::NativeFrontendHostProtocol::RequestV1* request)
{
    namespace F = NativeFrontendASIFixture;
    const auto mapping = NamedObject(F::MapVariable, true);
    const auto entered = NamedObject(F::EnteredVariable, false);
    const auto completed = NamedObject(F::CompletedVariable, false);
    auto*      words = mapping ? static_cast<volatile LONG*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, F::FieldCount * sizeof(LONG))) : nullptr;
    if (!mapping || !entered || !completed || !words)
    {
        if (words)
            UnmapViewOfFile(const_cast<LONG*>(words));
        for (const auto handle : {mapping, entered, completed})
            if (handle)
                CloseHandle(handle);
        return 0;
    }
    InterlockedIncrement(words + F::Calls);
    InterlockedExchange(words + F::CallerThread, static_cast<LONG>(GetCurrentThreadId()));
    if (request)
    {
        InterlockedExchange(words + F::RequestSize, static_cast<LONG>(request->size));
        InterlockedExchange(words + F::RequestVersion, static_cast<LONG>(request->version));
        InterlockedExchange(words + F::RequestFlags, static_cast<LONG>(request->flags));
        InterlockedExchange(words + F::RequestReserved, static_cast<LONG>(request->reserved));
    }
    else
        InterlockedExchange(words + F::InvalidArgument, 1);
    const auto started = GetTickCount64();
    SetEvent(entered);
    // A synchronous ASI entry would block for this deliberate delay. This is
    // controlled Bridge code only: it never creates Core, input, GUI or hooks.
    Sleep(1500);
    InterlockedExchange(words + F::ElapsedMs, static_cast<LONG>(GetTickCount64() - started));
    InterlockedExchange(words + F::Complete, 1);
    UnmapViewOfFile(const_cast<LONG*>(words));
    CloseHandle(mapping);
    CloseHandle(entered);
    SetEvent(completed);
    CloseHandle(completed);
    return 1;
}
