/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <windows.h>

namespace deonline::NativeFrontendHostProtocol
{
    inline constexpr std::uint32_t ApiVersion = 1;
    inline constexpr std::uint32_t BindingPersistence = 1, LocalInput = 2, SupportedFlags = BindingPersistence | LocalInput;
    struct RequestV1
    {
        std::uint32_t size = sizeof(RequestV1), version = ApiVersion, flags = 0, reserved = 0;
    };
    static_assert(sizeof(RequestV1) == 16);
    enum class Phase : std::uint32_t
    {
        Off,
        Starting,
        Running,
        Stopping,
        Stopped,
        Failed
    };
    enum class Failure : std::uint32_t
    {
        None,
        ModulePin,
        ThreadCreation,
        RuntimeBegin,
        RuntimeRead,
        CommandExhausted,
        UnexpectedWorkerExit,
        Exception
    };
    enum Field : std::uint32_t
    {
        Version,
        HostPhase,
        OwnerThread,
        Flags,
        RuntimeGeneration,
        LastCommand,
        CommandSequence,
        KeyMask,
        Focused,
        WorkerPhase,
        WorkerFailure,
        CoreRetained,
        CleanupPending,
        CleanupComplete,
        CoreStoppedVerified,
        CoreReleasedVerified,
        CoreUnloadedVerified,
        ViewAttempts,
        ViewsRetired,
        ConfigurationReadOnly,
        BindingManagerCreations,
        BindingLoads,
        ConfigurationLoads,
        LastFailure,
        ControllerPinned,
        WorkerCaptured,
        StopRequested,
        Reserved27,
        NetworkStarted,
        GameplayReady,
        FullMtaReady,
        Reserved31,
        FieldCount
    };
    static_assert(FieldCount == 32);
    using Snapshot = std::array<std::uint32_t, FieldCount>;
    using Start = int (*)(const RequestV1*);
    using Stop = int (*)();
    using Read = int (*)(std::uint32_t*, std::uint32_t);
}

namespace deonline
{
    class CoreStartupReference;
    class NativeFrontendHost
    {
    public:
        NativeFrontendHost(CoreStartupReference& runtime, std::atomic<DWORD>& controllerThread) noexcept;
        bool Start(const void* moduleAddress, const NativeFrontendHostProtocol::RequestV1& request) noexcept;
        bool Stop() noexcept;
        bool Read(std::uint32_t* output, std::uint32_t count) const noexcept;

    private:
        static unsigned __stdcall                                                      ThreadMain(void* context) noexcept;
        void                                                                           Run() noexcept;
        void                                                                           Publish() noexcept;
        void                                                                           Log() noexcept;
        void                                                                           Fail(NativeFrontendHostProtocol::Failure failure) noexcept;
        CoreStartupReference&                                                          runtime;
        std::atomic<DWORD>&                                                            controllerThread;
        std::atomic<bool>                                                              started{false}, stopRequested{false};
        const void*                                                                    moduleAddress = nullptr;
        NativeFrontendHostProtocol::Snapshot                                           report{}, logged{};
        std::array<std::atomic<std::uint32_t>, NativeFrontendHostProtocol::FieldCount> published{};
        std::atomic<std::uint32_t>                                                     publication{0};
        HANDLE                                                                         log = INVALID_HANDLE_VALUE;
    };
}
