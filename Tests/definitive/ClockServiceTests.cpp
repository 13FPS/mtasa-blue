/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual MTA clock interface and checked service boundary
 *****************************************************************************/
#include "../../Client/game_de/CClockDE.h"
#include "../../Client/game_de/CGameTimingDE.h"
#include "../../Client/game_de/NativeGameTimingProfile.h"
#include "../../Client/game_de/NativeGameFPSProfile.h"
#include "../../Client/game_de/NativeServiceAdmission.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <intrin.h>
#include <limits>
#include <stdexcept>
#include <thread>
#include <tlhelp32.h>
#include <type_traits>
#include <vector>

namespace ClockTestFaults
{
    std::atomic<unsigned> partialDataWrites{0}, failFlushes{0}, protectionCountdown{0};
    std::atomic<bool>     failEnumeration{false}, failProtectionRestores{false};
    std::atomic<unsigned> failResumes{0};
    std::atomic<unsigned> clockDataWrites{0}, clockCodeExchanges{0};
    std::atomic<unsigned> partialTimingWrites{0}, failTimingWrites{0}, timingDataWrites{0};
    std::uintptr_t        timingWriteAddress = 0;
    std::atomic<DWORD>    resumeTarget{0};
    std::uintptr_t        patchAddress = 0, dataAddress = 0;
    std::uintptr_t        uiReadAddress = 0, uiRootChangeAddress = 0;
    std::atomic<bool>     failUiRead{false};

    BOOL WINAPI Read(HANDLE process, LPCVOID address, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == uiReadAddress && failUiRead.exchange(false))
        {
            if (copied)
                *copied = 0;
            return FALSE;
        }
        const auto result = ::ReadProcessMemory(process, address, output, bytes, copied);
        if (reinterpret_cast<std::uintptr_t>(address) == uiReadAddress && uiRootChangeAddress)
        {
            const std::uintptr_t absent = 0;
            SIZE_T               written = 0;
            ::WriteProcessMemory(process, reinterpret_cast<void*>(uiRootChangeAddress), &absent, sizeof(absent), &written);
            uiRootChangeAddress = 0;
        }
        return result;
    }

    BOOL WINAPI Write(HANDLE process, LPVOID address, LPCVOID bytes, SIZE_T size, SIZE_T* copied)
    {
        clockDataWrites.fetch_add(1);
        if (reinterpret_cast<std::uintptr_t>(address) == timingWriteAddress && (size == 4 || size == 1))
        {
            timingDataWrites.fetch_add(1);
            if (failTimingWrites.load())
            {
                failTimingWrites.fetch_sub(1);
                if (copied)
                    *copied = 0;
                SetLastError(ERROR_ACCESS_DENIED);
                return FALSE;
            }
            if (partialTimingWrites.load())
            {
                partialTimingWrites.fetch_sub(1);
                if (size == 1)
                {
                    if (copied)
                        *copied = 0;
                    SetLastError(ERROR_PARTIAL_COPY);
                    return FALSE;
                }
                return ::WriteProcessMemory(process, address, bytes, 2, copied);
            }
        }
        if (reinterpret_cast<std::uintptr_t>(address) == dataAddress && size == 4 && partialDataWrites.load())
        {
            partialDataWrites.fetch_sub(1);
            SIZE_T     actual = 0;
            const auto result = ::WriteProcessMemory(process, address, bytes, 2, &actual);
            if (copied)
                *copied = actual;
            SetLastError(ERROR_PARTIAL_COPY);
            return result;
        }
        return ::WriteProcessMemory(process, address, bytes, size, copied);
    }
    LONG64 WINAPI CompareExchange(volatile LONG64* address, LONG64 desired, LONG64 expected)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == patchAddress)
            clockCodeExchanges.fetch_add(1);
        return ::InterlockedCompareExchange64(address, desired, expected);
    }
    BOOL WINAPI Protect(LPVOID address, SIZE_T size, DWORD protection, PDWORD previous)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == patchAddress && size == 8)
        {
            if (protection != PAGE_EXECUTE_READWRITE && failProtectionRestores.load())
            {
                SetLastError(ERROR_ACCESS_DENIED);
                return FALSE;
            }
            auto remaining = protectionCountdown.load();
            if (remaining && protectionCountdown.fetch_sub(1) == 1)
            {
                SetLastError(ERROR_ACCESS_DENIED);
                return FALSE;
            }
        }
        return ::VirtualProtect(address, size, protection, previous);
    }
    BOOL WINAPI Flush(HANDLE process, LPCVOID address, SIZE_T size)
    {
        if (reinterpret_cast<std::uintptr_t>(address) == patchAddress && size == 8 && failFlushes.load())
        {
            failFlushes.fetch_sub(1);
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return ::FlushInstructionCache(process, address, size);
    }
    BOOL WINAPI Next(HANDLE snapshot, LPTHREADENTRY32 entry)
    {
        if (failEnumeration.exchange(false))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return ::Thread32Next(snapshot, entry);
    }
    DWORD WINAPI Resume(HANDLE thread)
    {
        if (GetThreadId(thread) == resumeTarget.load() && failResumes.load())
        {
            failResumes.fetch_sub(1);
            SetLastError(ERROR_ACCESS_DENIED);
            return static_cast<DWORD>(-1);
        }
        return ::ResumeThread(thread);
    }
}

// Fault injection exists only in this ordinary test translation unit. The
// shipping executable gate, actual native access and clock implementation are
// unchanged; each wrapper otherwise calls the real Win32 operation.
#define WriteProcessMemory    ClockTestFaults::Write
#define ReadProcessMemory     ClockTestFaults::Read
#define VirtualProtect        ClockTestFaults::Protect
#define FlushInstructionCache ClockTestFaults::Flush
#define Thread32Next          ClockTestFaults::Next
#define ResumeThread          ClockTestFaults::Resume
#ifdef InterlockedCompareExchange64
    #undef InterlockedCompareExchange64
#endif
#define InterlockedCompareExchange64 ClockTestFaults::CompareExchange
#include "../../Client/game_de/NativeClockService.cpp"
#undef InterlockedCompareExchange64
#undef ResumeThread
#undef Thread32Next
#undef FlushInstructionCache
#undef VirtualProtect
#undef WriteProcessMemory
#undef ReadProcessMemory

namespace ClockReaderTestHost
{
    std::uintptr_t        image = 0;
    std::atomic<unsigned> samples{0}, mismatchSample{0};
    std::atomic<unsigned> timingSamples{0}, mismatchTimingSample{0};
    std::atomic<unsigned> fpsSamples{0}, mismatchFPSSample{0}, failFPSSample{0};

    HMODULE WINAPI Module(LPCWSTR name)
    {
        return !name && image ? reinterpret_cast<HMODULE>(image) : ::GetModuleHandleW(name);
    }
}

namespace mta::game_de::NativeImageAccess
{
    bool ClockReaderFixtureExecutable() noexcept
    {
        // A positive reader fixture selects only its owned SEC_IMAGE mapping;
        // the ordinary wrong-host test below still executes the shipping hash
        // gate. This does not admit this process as a supported GTA build.
        return ClockReaderTestHost::image != 0 || CheckExecutableBuild();
    }

    bool TimingReaderFixtureReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t bytes, bool writable = false,
                                      bool executable = false) noexcept
    {
        if (!ReadImage(image, address, destination, bytes, writable, executable))
            return false;
        if (address == image + NativeGameFPSProfile::fpsRva && bytes == sizeof(float))
        {
            const auto sample = ClockReaderTestHost::fpsSamples.fetch_add(1) + 1;
            if (sample == ClockReaderTestHost::failFPSSample.load())
                return false;
            if (sample == ClockReaderTestHost::mismatchFPSSample.load())
            {
                float value = 0;
                std::memcpy(&value, destination, sizeof(value));
                value += 1;
                std::memcpy(destination, &value, sizeof(value));
            }
        }
        if (address == image + NativeGameTimingProfile::timeScaleRva && bytes == sizeof(float) &&
            ClockReaderTestHost::timingSamples.fetch_add(1) + 1 == ClockReaderTestHost::mismatchTimingSample.load())
        {
            float value = 0;
            std::memcpy(&value, destination, sizeof(value));
            value += 1;
            std::memcpy(destination, &value, sizeof(value));
        }
        return true;
    }
}

namespace mta::game_de::GameTimeReaderFixtureMemory
{
    bool Read(std::uintptr_t image, ClockValues& output) noexcept
    {
        if (!NativeClockMemory::Read(image, output))
            return false;
        if (ClockReaderTestHost::samples.fetch_add(1) + 1 == ClockReaderTestHost::mismatchSample.load())
            ++output.seconds;
        return true;
    }
    bool Equal(const ClockValues& first, const ClockValues& second) noexcept
    {
        return NativeClockMemory::Equal(first, second);
    }
}

// These test-TU-only wrappers select the same owned native image used by the
// clock fixture and introduce a deterministic mismatch between complete reads.
// Shipping code keeps the real exact-executable gate and raw shared primitive.
#define CheckExecutableBuild ClockReaderFixtureExecutable
#define GetModuleHandleW     ClockReaderTestHost::Module
#define NativeClockMemory    GameTimeReaderFixtureMemory
#include "../../Client/game_de/NativeGameTimeReader.cpp"
#undef NativeClockMemory
#undef GetModuleHandleW
#undef CheckExecutableBuild

// Select only the owned image and one deterministic sample mismatch for the
// timing prerequisite. All pin/page checks and real CGame method bodies remain
// production code; no complete CGame implementation is fabricated for tests.
#define GetModuleHandleW ClockReaderTestHost::Module
#define ReadImage        TimingReaderFixtureReadImage
#include "../../Client/game_de/NativeGameTimingReader.cpp"
#undef ReadImage
#undef GetModuleHandleW

using namespace mta::game_de;

namespace
{
    std::atomic<unsigned> checks{0};
    void                  Check(bool condition, const char* why)
    {
        const auto count = checks.fetch_add(1) + 1;
        if (!condition)
        {
            std::fprintf(stderr, "Clock service failed after %u checks: %s\n", count, why);
            std::exit(1);
        }
        std::printf("Check %u: %s\n", count, why);
    }

    class ClockImageFixture
    {
    public:
        static constexpr std::uint32_t TextRva = 0x1000000, TextSize = 0x300000, DataRva = 0x5200000, DataSize = 0x40000;
        std::uintptr_t                 image = 0;
        HANDLE                         file = INVALID_HANDLE_VALUE, mapping = nullptr;
        void (*invoke)() = nullptr;
        void (*invokeTimer)() = nullptr;
        void (*invokeAlternateTimer)() = nullptr;
        NativeApplicationFrameProfile::FrameFunction invokeFrame = nullptr, invokeLoadingFrame = nullptr;
        std::array<unsigned char, 0x300>             frameCaller{};
        std::array<std::uintptr_t, (NativeUiClassMapProfile::ownerOffset + NativeUiClassMapProfile::headerBytes) / sizeof(std::uintptr_t)> frameOwner{};
        std::array<unsigned char, NativeUiClassMapProfile::entryBytes * 2>                                                                 uiEntries{};
        std::array<char16_t, NativeUiClassMapProfile::keyLoadingCount>    uiLoading{u'L', u'o', u'a', u'd', u'i', u'n', u'g', 0};
        std::array<char16_t, NativeUiClassMapProfile::keySimpleFadeCount> uiSimpleFade{u'S', u'i', u'm', u'p', u'l', u'e', u'F', u'a', u'd', u'e', 0};
        std::uintptr_t                                                    frameReceiver = 0;

        ~ClockImageFixture()
        {
            if (image)
                UnmapViewOfFile(reinterpret_cast<void*>(image));
            if (mapping)
                CloseHandle(mapping);
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
        void Create(const std::filesystem::path& root)
        {
            const auto path = root / L"owned-clock-image.bin";
            Check(!std::filesystem::exists(path), "owned native image evidence is created once without overwriting an earlier run");
            std::vector<unsigned char> bytes(0x1000 + TextSize + DataSize, 0);
            IMAGE_DOS_HEADER           dos{};
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            std::memcpy(bytes.data(), &dos, sizeof(dos));
            IMAGE_NT_HEADERS64 nt{};
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = 6;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.ImageBase = UINT64_C(0x180000000);
            nt.OptionalHeader.SectionAlignment = 0x1000;
            nt.OptionalHeader.FileAlignment = 0x200;
            nt.OptionalHeader.SizeOfHeaders = 0x1000;
            nt.OptionalHeader.SizeOfCode = TextSize;
            nt.OptionalHeader.SizeOfInitializedData = DataSize;
            nt.OptionalHeader.BaseOfCode = TextRva;
            nt.OptionalHeader.SizeOfImage = 0x5ca5000;
            nt.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
            nt.OptionalHeader.MajorOperatingSystemVersion = 6;
            nt.OptionalHeader.MajorSubsystemVersion = 6;
            nt.OptionalHeader.SizeOfStackReserve = nt.OptionalHeader.SizeOfHeapReserve = 0x100000;
            nt.OptionalHeader.SizeOfStackCommit = nt.OptionalHeader.SizeOfHeapCommit = 0x1000;
            nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            std::memcpy(bytes.data() + dos.e_lfanew, &nt, sizeof(nt));
            std::array<IMAGE_SECTION_HEADER, 6> sections{};
            // A Windows image section table must describe adjacent virtual
            // ranges. Sparse zero-filled sections preserve the actual native
            // RVAs without leaving loader-invalid gaps or a huge raw file.
            const auto zeroSection = [&](std::size_t index, const char* name, std::uint32_t rva, std::uint32_t size)
            {
                std::memcpy(sections[index].Name, name, std::strlen(name));
                sections[index].VirtualAddress = rva;
                sections[index].Misc.VirtualSize = size;
                sections[index].Characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            };
            zeroSection(0, ".head", 0x1000, TextRva - 0x1000);
            zeroSection(2, ".late", TextRva + TextSize, 0x1400000 - TextRva - TextSize);
            zeroSection(3, ".gap", 0x1400000, DataRva - 0x1400000);
            zeroSection(5, ".tail", DataRva + DataSize, nt.OptionalHeader.SizeOfImage - DataRva - DataSize);
            // Only bounded admitted pages gain executable/writable protection
            // below. These virtual-section flags also describe the earlier
            // application caller and later root/state fields to the real gate.
            sections[0].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
            sections[2].Characteristics |= IMAGE_SCN_MEM_EXECUTE;
            sections[5].Characteristics |= IMAGE_SCN_MEM_WRITE;
            std::memcpy(sections[1].Name, ".text", 5);
            sections[1].VirtualAddress = TextRva;
            sections[1].Misc.VirtualSize = sections[1].SizeOfRawData = TextSize;
            sections[1].PointerToRawData = 0x1000;
            sections[1].Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            std::memcpy(sections[4].Name, ".data", 5);
            sections[4].VirtualAddress = DataRva;
            sections[4].Misc.VirtualSize = sections[4].SizeOfRawData = DataSize;
            sections[4].PointerToRawData = 0x1000 + TextSize;
            sections[4].Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            std::memcpy(bytes.data() + dos.e_lfanew + sizeof(nt), sections.data(), sizeof(sections));
            const auto code = [&](std::uintptr_t rva, const auto& data) { std::memcpy(bytes.data() + 0x1000 + rva - TextRva, data.data(), data.size()); };
            code(NativeClockProfile::clockCodeRva, NativeClockProfile::clockCode);
            code(NativeClockProfile::hookEntryRva, NativeClockProfile::hookEntry);
            code(NativeClockProfile::hookTailRva, NativeClockProfile::hookTail);
            code(NativeClockProfile::clockCallRva, NativeClockProfile::clockCall);
            code(NativeClockProfile::frameCallRva, NativeClockProfile::frameCall);
            code(NativeClockProfile::getterCaseRva, NativeClockProfile::getterCase);
            code(NativeGameTimingProfile::initializeRva, NativeGameTimingProfile::initialize);
            code(NativeGameTimingProfile::elapsedMultiplierRva, NativeGameTimingProfile::elapsedMultiplier);
            code(NativeGameTimingProfile::stepUpdateRva, NativeGameTimingProfile::stepUpdate);
            code(NativeGameTimingProfile::frameGetterRva, NativeGameTimingProfile::frameGetter);
            code(NativeGameTimingProfile::stepConsumerRva, NativeGameTimingProfile::stepConsumer);
            code(NativeGameTimingProfile::scriptScaleRva, NativeGameTimingProfile::scriptScale);
            code(NativeGamePauseProfile::entryRva, NativeGamePauseProfile::entry);
            code(NativeGamePauseProfile::tailRva, NativeGamePauseProfile::tail);
            code(NativeGamePauseProfile::pauseReadsRva, NativeGamePauseProfile::pauseReads);
            code(NativeGamePauseProfile::pauseBranchRva, NativeGamePauseProfile::pauseBranch);
            code(NativeGamePauseProfile::frameRva, NativeGamePauseProfile::frame);
            code(NativeGamePauseProfile::returnRva, NativeGamePauseProfile::returnCode);
            code(NativeGamePauseProfile::normalCallRva, NativeGamePauseProfile::normalCall);
            code(NativeGamePauseProfile::alternateCallRva, NativeGamePauseProfile::alternateCall);
            code(NativeGamePauseProfile::gameplayTailRva, NativeGamePauseProfile::gameplayTail);
            // Preserve the independently pinned original prologue and supply
            // only an owned return body. Its native clock update is not executed.
            const std::array<unsigned char, 8> epilogue{0x48, 0x81, 0xc4, 0xd8, 0, 0, 0, 0xc3};
            code(NativeClockProfile::hookTailRva + NativeClockProfile::hookTail.size(), epilogue);
            const std::array<unsigned char, 5> callReturn{0x48, 0x83, 0xc4, 0x28, 0xc3};
            code(NativeClockProfile::clockCallRva + NativeClockProfile::clockCall.size(), callReturn);
            code(NativeGamePauseProfile::normalReturnRva, callReturn);
            code(0x129702a, callReturn);
            // Reproduce the independently admitted alternate tail-call origin
            // in this owned image. No native state dispatcher is executed.
            std::array<unsigned char, 5> timerTail{0xe9};
            const auto                   timerDisplacement = static_cast<std::int32_t>(NativeGamePauseProfile::entryRva - 0x1209e10 - timerTail.size());
            std::memcpy(timerTail.data() + 1, &timerDisplacement, sizeof(timerDisplacement));
            code(0x1209e10, timerTail);
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            Check(stream.good(), "save the owned synthetic PE used by actual Windows SEC_IMAGE page validation");
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "open only the task-owned synthetic native image");
            mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
            const auto mappingError = GetLastError();
            std::printf("Owned image SEC_IMAGE mapping GetLastError=%lu\n", mappingError);
            Check(mapping != nullptr, "create actual Windows image mapping without weakening shipping MEM_IMAGE checks");
            image = reinterpret_cast<std::uintptr_t>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
            Check(image != 0, "map the owned image with actual executable and writable native sections");
            // The production executable records a non-page-aligned SizeOfImage.
            // Keep the kernel mapping legal, then seed those exact observed
            // metadata bytes inside this independently owned image fixture.
            nt.OptionalHeader.SizeOfImage = 97141760;
            Patch(static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt));
            std::array<unsigned char, 16> thunk{0x48, 0x83, 0xec, 0x28, 0x48, 0xb8};
            const auto                    target = image + NativeClockProfile::clockCallRva;
            std::memcpy(thunk.data() + 6, &target, sizeof(target));
            thunk[14] = 0xff;
            thunk[15] = 0xe0;
            Patch(TextRva + 0x1000, thunk.data(), thunk.size());
            invoke = reinterpret_cast<void (*)()>(image + TextRva + 0x1000);
            const auto timerThunk = [&](std::uintptr_t rva, std::uintptr_t call)
            {
                auto       timerCode = thunk;
                const auto destination = image + call;
                std::memcpy(timerCode.data() + 6, &destination, sizeof(destination));
                Patch(rva, timerCode.data(), timerCode.size());
                return reinterpret_cast<void (*)()>(image + rva);
            };
            invokeTimer = timerThunk(TextRva + 0x1100, NativeGamePauseProfile::normalCallRva);
            invokeAlternateTimer = timerThunk(TextRva + 0x1200, NativeGamePauseProfile::alternateCallRva);
            ClockTestFaults::patchAddress = image + NativeClockProfile::patchRva - 3;
            ClockTestFaults::dataAddress = image + 0x522bcf8;
            AdmitFPSPins();
        }
        void AdmitFPSPins()
        {
            // FPS lives far from the compact clock sections. Populate only
            // this owned SEC_IMAGE's zero-filled virtual pages, then mark its
            // code RX, constants RO and data RW. This keeps MEM_IMAGE/page
            // checks real without copying GTA or growing a huge sparse raw file.
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t length, DWORD protection)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), length, PAGE_READWRITE, &previous) != FALSE,
                      "admit only the synthetic SEC_IMAGE FPS fixture page for initialization");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, length);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), length, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), length),
                      "set exact synthetic FPS code/data protections without executable writable pages");
            };
            const auto code = [&](std::uintptr_t rva, const auto& bytes) { admit(rva, bytes.data(), bytes.size(), PAGE_EXECUTE_READ); };
            code(NativeGameFPSProfile::frequencyScaleRva, NativeGameFPSProfile::frequencyScale);
            code(NativeGameFPSProfile::computationRva, NativeGameFPSProfile::computation);
            code(NativeGameFPSProfile::consumerRva, NativeGameFPSProfile::consumer);
            admit(NativeGameFPSProfile::labelRva, NativeGameFPSProfile::label.data(), NativeGameFPSProfile::label.size(), PAGE_READONLY);
            const auto constant = [&](std::uintptr_t rva, const auto& value) { admit(rva, &value, sizeof(value), PAGE_READONLY); };
            constant(NativeGameFPSProfile::previousWeightRva, NativeGameFPSProfile::previousWeight);
            constant(NativeGameFPSProfile::currentWeightRva, NativeGameFPSProfile::currentWeight);
            constant(NativeGameFPSProfile::fpsNumeratorRva, NativeGameFPSProfile::fpsNumerator);
            constant(NativeGameFPSProfile::millisecondsRva, NativeGameFPSProfile::milliseconds);
            constant(NativeGameFPSProfile::secondsOriginRva, NativeGameFPSProfile::secondsOrigin);
            constant(NativeGameFPSProfile::frequencyNumeratorRva, NativeGameFPSProfile::frequencyNumerator);
            const std::array<float, 2> values{16.0f, 62.5f};
            const double               previousSeconds = 16777216.0, secondsPerCycle = 0.000001;
            admit(NativeGameFPSProfile::averageMsRva, values.data(), sizeof(values), PAGE_READWRITE);
            admit(NativeGameFPSProfile::previousSecondsRva, &previousSeconds, sizeof(previousSeconds), PAGE_READWRITE);
            admit(NativeGameFPSProfile::secondsPerCycleRva, &secondsPerCycle, sizeof(secondsPerCycle), PAGE_READWRITE);
        }
        void AdmitApplicationPins()
        {
            namespace Frame = NativeApplicationFrameProfile;
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, PAGE_READWRITE, &previous) != FALSE,
                      "initialize only bounded owned SEC_IMAGE application fixture pages");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), size, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), size),
                      "restore synthetic application code RX, vtable RO and state/root RW protections");
            };
            for (const auto& pin : Frame::pins)
                admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto entry = image + Frame::entryRva;
            admit(Frame::slotRva, &entry, sizeof(entry), PAGE_READONLY);
            const std::uint32_t state = 7;
            admit(Frame::stateRva, &state, sizeof(state), PAGE_READWRITE);
            admit(Frame::rootRva, &frameReceiver, sizeof(frameReceiver), PAGE_READWRITE);

            // Retain every entry/tail pin and exercise the real Detours
            // trampoline, but return before any native engine work. Synthetic
            // callers deliberately have no invented exception-unwind metadata.
            const std::array<unsigned char, 10> originalReturn{0x48, 0x81, 0xc4, 0xb8, 0, 0, 0, 0x5d, 0x5b, 0xc3};
            Patch(Frame::tailRva + Frame::tail.size(), originalReturn.data(), originalReturn.size());
            const std::array<unsigned char, 3> benignReturn{0x33, 0xc0, 0xc3};
            constexpr std::uintptr_t           benignRva = TextRva + 0x1600;
            Patch(benignRva, benignReturn.data(), benignReturn.size());
            const auto benign = image + benignRva;
            admit(Frame::vtableRva + 0x300, &benign, sizeof(benign), PAGE_READONLY);
            admit(Frame::vtableRva + 0x388, &benign, sizeof(benign), PAGE_READONLY);
            // The pinned caller's suffix is kept intact. Its ordinary helper
            // targets become owned leaf returns, so the exact normal call can
            // run without pretending to create a UE world or GUI.
            for (const std::uintptr_t rva : {0xc227b0u, 0xb65960u, 0x3004080u})
                admit(rva, benignReturn.data(), benignReturn.size(), PAGE_EXECUTE_READ);
            std::vector<unsigned char> normal{0x53, 0x48, 0x83, 0xec, 0x50, 0x0f, 0x29, 0x74, 0x24, 0x40, 0x0f, 0x28, 0xf1, 0x48, 0xbb};
            const auto                 append = [](std::vector<unsigned char>& code, std::uintptr_t value)
            {
                const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
                code.insert(code.end(), bytes, bytes + sizeof(value));
            };
            append(normal, reinterpret_cast<std::uintptr_t>(frameCaller.data()));
            normal.insert(normal.end(), {0x48, 0x8b, 0x01, 0x49, 0xba});
            append(normal, image + Frame::normalReturnRva - 6);
            normal.insert(normal.end(), {0x41, 0xff, 0xe2});
            Patch(TextRva + 0x1400, normal.data(), normal.size());
            invokeFrame = reinterpret_cast<Frame::FrameFunction>(image + TextRva + 0x1400);
            std::vector<unsigned char> loading{0x48, 0x83, 0xec, 0x28, 0x48, 0x8b, 0x01, 0x49, 0xba};
            append(loading, image + Frame::excludedLoadingReturnRva - 6);
            loading.insert(loading.end(), {0x41, 0xff, 0xe2});
            Patch(TextRva + 0x1500, loading.data(), loading.size());
            const std::array<unsigned char, 11> loadingCall{0xff, 0x90, 0xb0, 0x03, 0, 0, 0x48, 0x83, 0xc4, 0x28, 0xc3};
            Patch(Frame::excludedLoadingReturnRva - 6, loadingCall.data(), loadingCall.size());
            invokeLoadingFrame = reinterpret_cast<Frame::FrameFunction>(image + TextRva + 0x1500);
        }
        void SetFrameReceiver(void* receiver)
        {
            frameReceiver = reinterpret_cast<std::uintptr_t>(receiver);
            Put(NativeApplicationFrameProfile::rootRva, frameReceiver);
            std::memcpy(frameCaller.data() + 0x2c0, &frameReceiver, sizeof(frameReceiver));
        }
        void AdmitUiClassMapPins()
        {
            for (const auto& pin : NativeUiClassMapProfile::pins)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + pin.rva), pin.size, PAGE_READWRITE, &previous) != FALSE,
                      "initialize only owned dictionary proof pages");
                std::memcpy(reinterpret_cast<void*>(image + pin.rva), pin.bytes, pin.size);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + pin.rva), pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + pin.rva), pin.size),
                      "restore dictionary proof code RX and immutable tables/literals RO");
            }
        }
        void SeedUiClassMap()
        {
            namespace Ui = NativeUiClassMapProfile;
            auto* map = reinterpret_cast<unsigned char*>(frameOwner.data()) + Ui::ownerOffset;
            std::memset(map, 0, Ui::headerBytes);
            uiEntries.fill(0);
            const auto put = [](unsigned char* target, std::size_t offset, const auto& value) { std::memcpy(target + offset, &value, sizeof(value)); };
            put(map, Ui::slotsOffset, reinterpret_cast<std::uintptr_t>(uiEntries.data()));
            put(map, Ui::slotCountOffset, std::int32_t{2});
            put(map, Ui::inlineHashOffset, std::int32_t{0});
            put(map, Ui::hashSizeOffset, std::int32_t{1});
            put(uiEntries.data(), Ui::keyDataOffset, reinterpret_cast<std::uintptr_t>(uiLoading.data()));
            put(uiEntries.data(), Ui::keyCountOffset, static_cast<std::int32_t>(uiLoading.size()));
            put(uiEntries.data(), Ui::valueOffset, std::uintptr_t{0x12345678});
            put(uiEntries.data(), Ui::nextOffset, std::int32_t{1});
            auto* fade = uiEntries.data() + Ui::entryBytes;
            put(fade, Ui::keyDataOffset, reinterpret_cast<std::uintptr_t>(uiSimpleFade.data()));
            put(fade, Ui::keyCountOffset, static_cast<std::int32_t>(uiSimpleFade.size()));
            put(fade, Ui::valueOffset, std::uintptr_t{0x87654321});
            put(fade, Ui::nextOffset, std::int32_t{-1});
        }
        void AdmitStartupPins()
        {
            const auto admit = [&](std::uintptr_t rva, const void* bytes, std::size_t count, DWORD protection)
            {
                DWORD previous = 0, ignored = 0;
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), count, PAGE_READWRITE, &previous) != FALSE,
                      "populate only the owned synthetic startup profile pages");
                std::memcpy(reinterpret_cast<void*>(image + rva), bytes, count);
                Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), count, protection, &ignored) != FALSE &&
                          ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), count),
                      "restore synthetic startup code RX and immutable pins/getter slot RO");
            };
            for (const auto& pin : NativeStartupOwnerProfile::pins)
                admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : NativeStartupWorldProfile::pins)
                admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            const auto getter = image + NativeStartupWorldProfile::getterRva;
            admit(NativeStartupWorldProfile::ownerGetWorldSlotRva, &getter, sizeof(getter), PAGE_READONLY);
            const std::uintptr_t absent = 0;
            admit(NativeStartupWorldProfile::objectChunksRva, &absent, sizeof(absent), PAGE_READWRITE);
        }
        void Patch(std::uintptr_t rva, const void* bytes, std::size_t length)
        {
            DWORD previous = 0, ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), length, PAGE_EXECUTE_READWRITE, &previous) != FALSE,
                  "temporarily admit writes to this owned image fixture range");
            std::memcpy(reinterpret_cast<void*>(image + rva), bytes, length);
            Check(::VirtualProtect(reinterpret_cast<void*>(image + rva), length, previous, &ignored) != FALSE &&
                      ::FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), length),
                  "restore actual fixture protection and instruction-cache coherence");
        }
        template <typename T>
        void Put(std::uintptr_t rva, const T& value)
        {
            SIZE_T written = 0;
            Check(::WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + rva), &value, sizeof(value), &written) && written == sizeof(value),
                  "write only a task-owned synthetic native data field");
        }
        void Seed()
        {
            Put(0x521c917, std::uint8_t{12});
            Put(0x521c923, std::uint8_t{34});
            Put(0x522bcf0, std::uint16_t{27});
            Put(0x523a458, std::uint32_t{10000});
            Put(0x522bcf8, std::uint32_t{9900});
            Put(0x5227bac, std::uint32_t{1000});
            Put(NativeGameTimingProfile::timeScaleRva, 1.0f);
            Put(NativeGamePauseProfile::userPauseRva, std::uint8_t{0});
        }
        template <typename T>
        T Get(std::uintptr_t rva) const
        {
            T      value{};
            SIZE_T copied = 0;
            Check(ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(image + rva), &value, sizeof(value), &copied) && copied == sizeof(value),
                  "read an actual owned native image field for independent verification");
            return value;
        }
    };

    // This receiver tests the real SDK implementation without pretending to be
    // a CGame or shipping native binding. Native memory/hook behavior belongs to
    // the separate production-runtime fixture below, not these receiver values.
    class AccessFixture final : public NativeClockAccess
    {
    public:
        std::atomic<DWORD>            owner{0};
        ClockValues                   values{12, 34, 56, 0x12345678, 0x12345670, 1000};
        ClockResult                   readResult = ClockResult::Ok, setResult = ClockResult::Ok;
        ClockResult                   freezeResult = ClockResult::Ok, frozenResult = ClockResult::Ok;
        bool                          frozen = false;
        std::atomic<unsigned>         reads{0}, sets{0}, freezes{0};
        mutable std::atomic<unsigned> frozenReads{0};
        std::uint8_t                  lastHour = 0, lastMinute = 0;

        ClockResult Owner() const noexcept
        {
            const auto thread = owner.load();
            return !thread ? ClockResult::NotInitialized : thread != GetCurrentThreadId() ? ClockResult::WrongThread : ClockResult::Ok;
        }
        ClockResult Read(ClockValues& output) noexcept override
        {
            ++reads;
            const auto result = Owner();
            if (result != ClockResult::Ok)
                return result;
            output = values;
            // Deliberately publish into the temporary even on a failed backend
            // read. The actual CClockDE must preserve its caller's two outputs.
            return readResult;
        }
        ClockResult Set(std::uint8_t hour, std::uint8_t minute) noexcept override
        {
            ++sets;
            const auto result = Owner();
            if (result != ClockResult::Ok)
                return result;
            if (setResult == ClockResult::Ok)
            {
                lastHour = hour;
                lastMinute = minute;
                values.hour = hour;
                values.minute = minute;
            }
            return setResult;
        }
        ClockResult Freeze(bool value) noexcept override
        {
            ++freezes;
            const auto result = Owner();
            if (result != ClockResult::Ok)
                return result;
            if (freezeResult == ClockResult::Ok)
                frozen = value;
            return freezeResult;
        }
        ClockResult Frozen(bool& output) const noexcept override
        {
            ++frozenReads;
            const auto result = Owner();
            if (result != ClockResult::Ok)
                return result;
            output = frozen;
            return frozenResult;
        }
    };

    void ClockInterfaceContract()
    {
        static_assert(std::is_base_of_v<CClock, CClockDE> && !std::is_abstract_v<CClockDE>);
        static_assert(!std::has_virtual_destructor_v<CClock>);
        AccessFixture access;
        CClockDE      clock(access);
        CClock&       sdk = clock;
        const CClock& constSdk = clock;
        BYTE          hour = 0xa5, minute = 0x5a;
        Check(clock.LastResult() == ClockResult::NotInitialized, "a new concrete MTA clock contains no fabricated native time");
        sdk.Get(&hour, &minute);
        Check(hour == 0xa5 && minute == 0x5a && clock.LastResult() == ClockResult::NotInitialized,
              "unbound SDK Get preserves both caller outputs and records the backend refusal");
        Check(!sdk.SetTimeFrozen(true) && clock.LastResult() == ClockResult::NotInitialized, "unbound SDK freeze never reports success");
        access.owner = GetCurrentThreadId();
        sdk.Get(&hour, &minute);
        Check(hour == 12 && minute == 34 && clock.LastResult() == ClockResult::Ok,
              "the existing CClock virtual interface reads a coherent successful native-access result");
        BYTE alias = 0;
        sdk.Get(&alias, &alias);
        Check(alias == 12, "aliased Get outputs retain the existing classic minute-first publication order");
        const auto readCalls = access.reads.load();
        Check(clock.TryGet(nullptr, &minute) == ClockResult::InvalidArgument && clock.TryGet(&hour, nullptr) == ClockResult::InvalidArgument &&
                  clock.TryGet(nullptr, nullptr) == ClockResult::InvalidArgument && hour == 12 && minute == 34 && access.reads.load() == readCalls,
              "null output arguments refuse before native access without changing a valid output");
        for (const auto value : {std::array<BYTE, 2>{0, 0}, std::array<BYTE, 2>{23, 59}, std::array<BYTE, 2>{23, 60}})
        {
            sdk.Set(value[0], value[1]);
            Check(clock.LastResult() == ClockResult::Ok && access.lastHour == value[0] && access.lastMinute == value[1],
                  "actual SDK Set delegates canonical times and classic minute 60 without normalization");
        }
        const auto setCalls = access.sets.load();
        for (const auto value : {std::array<BYTE, 2>{24, 0}, std::array<BYTE, 2>{255, 0}, std::array<BYTE, 2>{0, 61}, std::array<BYTE, 2>{0, 255}})
        {
            sdk.Set(value[0], value[1]);
            Check(clock.LastResult() == ClockResult::InvalidArgument && access.sets.load() == setCalls,
                  "out-of-range legacy SDK times cannot reach native writes");
        }
        access.readResult = ClockResult::ReadFailure;
        hour = 0xa5;
        minute = 0x5a;
        Check(clock.TryGet(&hour, &minute) == ClockResult::ReadFailure && hour == 0xa5 && minute == 0x5a,
              "a failed backend read cannot partially publish or return an earlier cached time");
        access.readResult = ClockResult::Ok;
        access.values.hour = 6;
        access.values.minute = 7;
        sdk.Get(&hour, &minute);
        Check(hour == 6 && minute == 7, "the next SDK Get reads the new backend values rather than a retained prior read");
        access.setResult = ClockResult::WriteFailure;
        sdk.Set(15, 16);
        Check(clock.LastResult() == ClockResult::WriteFailure && access.values.hour == 6 && access.values.minute == 7,
              "the void SDK Set retains a separately readable native write failure");
        access.setResult = ClockResult::Ok;
        Check(sdk.SetTimeFrozen(true) && constSdk.IsTimeFrozen() && clock.LastResult() == ClockResult::Ok,
              "existing SDK freeze and const query delegate to the real checked backend boundary");
        Check(sdk.SetTimeFrozen(true) && access.frozen, "repeated SDK freeze remains an idempotent backend request");
        bool observed = false;
        access.frozenResult = ClockResult::ReadFailure;
        Check(clock.TryIsTimeFrozen(observed) == ClockResult::ReadFailure && !observed, "failed frozen-state reads preserve the caller's existing false value");
        observed = true;
        Check(clock.TryIsTimeFrozen(observed) == ClockResult::ReadFailure && observed, "failed frozen-state reads also preserve an existing true value");
        Check(!constSdk.IsTimeFrozen() && clock.LastResult() == ClockResult::ReadFailure,
              "the legacy bool query records errors and never reuses a cached successful frozen value");
        access.frozenResult = ClockResult::Ok;
        access.freezeResult = ClockResult::PatchFailure;
        Check(!sdk.ResetTimeFrozen() && clock.LastResult() == ClockResult::PatchFailure && access.frozen,
              "a failed native unfreeze cannot falsely complete the SDK reset");
        access.freezeResult = ClockResult::Ok;
        Check(sdk.ResetTimeFrozen() && !constSdk.IsTimeFrozen() && sdk.ResetTimeFrozen(),
              "SDK ResetTimeFrozen means idempotent unfreeze rather than restoring an earlier cached state");
        bool        foreignRejected = false;
        std::thread foreign(
            [&]
            {
                BYTE otherHour = 0xab, otherMinute = 0xcd;
                bool otherFrozen = true;
                foreignRejected = clock.TryGet(&otherHour, &otherMinute) == ClockResult::WrongThread && otherHour == 0xab && otherMinute == 0xcd &&
                                  clock.TrySet(1, 2) == ClockResult::WrongThread && clock.TrySetTimeFrozen(true) == ClockResult::WrongThread &&
                                  clock.TryIsTimeFrozen(otherFrozen) == ClockResult::WrongThread && otherFrozen &&
                                  clock.TryResetTimeFrozen() == ClockResult::WrongThread;
            });
        foreign.join();
        Check(foreignRejected && !access.frozen && access.values.hour == 6 && access.values.minute == 7,
              "every checked SDK operation preserves the backend's adopted engine-thread policy");
        Check(clock.LastResult() == ClockResult::WrongThread, "cross-thread diagnostic observation is atomic without sharing cached native fields");
        Check(clock.TrySet(8, 9) == ClockResult::Ok && clock.TryGet(&hour, &minute) == ClockResult::Ok && hour == 8 && minute == 9,
              "an unauthorized call does not revoke a later valid engine-owner operation");
    }
    ClockServiceStateV1 State(NativeClockService& service)
    {
        ClockServiceStateV1 state;
        Check(service.Read(state), "read the production service's coherent published state");
        return state;
    }

    void BeginOwned(NativeClockService& service, ClockImageFixture& fixture, bool adoptNow = true, bool attachNow = true, bool timingMode = false,
                    GameTimingHandler handler = nullptr, void* context = nullptr)
    {
        ClockServiceStateV1 rejected;
        const bool          shippingAdmitted = timingMode ? service.BeginGameTiming({}, rejected, handler, context) : service.Begin({}, rejected);
        Check(!shippingAdmitted && rejected.failure == ClockResult::UnsupportedBuild && !rejected.bound && !rejected.hooked && !rejected.modulePinned &&
                  rejected.generation && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "shipping Begin rejects this actual test executable before binding, hooks or module pinning");
        fixture.Seed();
        NativeServiceAdmission::Token claim;
        Check(NativeServiceAdmission::TryAcquire(NativeServiceAdmission::Category::Clock, claim),
              "the owned native image fixture holds the actual shared clock admission throughout setup and cleanup");
        runtime.admissionSerial.store(claim.serial);
        runtime.admissionGeneration.store(runtime.generation.load());
        Check(!ReleaseAdmission(runtime.generation.load() - 1) && NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "a stale native cleanup generation cannot release the current fixture's real process-wide clock claim");
        // Whitebox admission replaces only the test-owned runtime's image after
        // proving that public shipping admission refused it. All subsequent
        // data/code validation, Detours and callback ownership remain real.
        runtime.image = fixture.image;
        runtime.failure.store(ClockResult::Ok);
        runtime.nativeTouched.store(false);
        runtime.stopping.store(false);
        runtime.bound.store(true);
        runtime.active.store(true);
        runtime.started = GetTickCount64();
        runtime.refreshed.store(runtime.started);
        runtime.originalContinuation = reinterpret_cast<ContinuationFunction>(fixture.image + NativeClockProfile::hookEntryRva);
        runtime.originalTimer = timingMode ? reinterpret_cast<ContinuationFunction>(fixture.image + NativeGamePauseProfile::entryRva) : nullptr;
        runtime.timingHandler = handler;
        runtime.timingContext = context;
        Check(CheckBinding(true), "exact owned MEM_IMAGE code, page and section pins validate before any native hook transaction");
        if (!attachNow)
            return;
        Check(ChangeHooks(true), "actual Detours attaches after validated native ownership");
        runtime.hooked.store(true);
        if (!adoptNow)
            return;
        fixture.invoke();
        const auto state = State(service);
        Check(state.engineThread == GetCurrentThreadId() && state.producerThread == GetCurrentThreadId() && state.ready && state.active && !state.restored &&
                  state.qualifiedCallbacks == 1 && state.callbacks == 1 && state.hour == 12 && state.minute == 34 && state.seconds == 27,
              "the actual qualified native return address adopts the owner and captures fresh engine clock fields");
    }

    ClockCommandV1 Command(const ClockServiceStateV1& state, std::uint64_t sequence, ClockOperation operation = ClockOperation::Read)
    {
        ClockCommandV1 command;
        command.generation = state.generation;
        command.sequence = sequence;
        command.operation = operation;
        return command;
    }

    ClockServiceStateV1 Execute(NativeClockService& service, ClockImageFixture& fixture, ClockOperation operation, std::uint64_t sequence = 1)
    {
        const auto before = State(service);
        Check(service.Submit(Command(before, sequence, operation)), "the producer queues one generation-bound production clock command");
        fixture.invoke();
        const auto after = State(service);
        Check(after.lastCompletedSequence == sequence && !after.pendingSequence && !after.inFlight,
              "actual callback completes the queued sequence and releases callback lifetime accounting");
        return after;
    }

    void FinishOwned(NativeClockService& service, ClockImageFixture& fixture)
    {
        NativeServiceAdmission::Token refused;
        Check(NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) &&
                  !NativeServiceAdmission::TryAcquire(NativeServiceAdmission::Category::Input, refused) &&
                  !NativeServiceAdmission::TryAcquire(NativeServiceAdmission::Category::Render, refused),
              "native setup or cleanup debt keeps the actual shared clock claim exclusive against input and renderer startup");
        if (!runtime.stopping.load())
            Check(!service.End() && service.IsRetained(), "End retains the real hook until native-owner restoration runs");
        fixture.invoke();
        Check(service.End() && !service.IsRetained(), "native restoration acknowledges before real Detours detach and owner release");
        const auto state = State(service);
        Check(state.restored && !state.ready && !state.bound && !state.hooked && !state.patchInstalled && !state.patchRecoveryPending &&
                  !state.dataRecoveryPending && !runtime.timingDirty.load() && !runtime.timingRecovery.load() && !runtime.timingCaptured.load() &&
                  !runtime.timingGeneration.load() && !state.retainedPeerThreads && !state.protectionRestorePending && !state.inFlight,
              "completed cleanup has no native data, branch, protection, peer-handle or callback debt");
        Check(CheckBinding(true), "exact original clock and continuation bytes survive complete cleanup");
        Check(!NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && !runtime.admissionSerial.load() &&
                  !runtime.admissionGeneration.load(),
              "only confirmed native cleanup releases its exact shared service admission generation");
    }

    void NativeContractValidation()
    {
        ClockServiceRequestV1 request;
        Check(NativeClockService::ValidateRequest(request), "default bounded native clock request is valid");
        for (unsigned field = 0; field != 6; ++field)
        {
            auto invalid = request;
            if (field == 0)
                --invalid.size;
            if (field == 1)
                ++invalid.version;
            if (field == 2)
                invalid.maximumDurationMs = 0;
            if (field == 3)
                invalid.maximumDurationMs = ClockServiceMaximumDurationMsV1 + 1;
            if (field == 4)
                invalid.reserved[0] = 1;
            if (field == 5)
                invalid.reserved[2] = 1;
            Check(!NativeClockService::ValidateRequest(invalid), "invalid request shape, lease duration or reserved data refuses admission");
        }
        ClockCommandV1 command;
        command.generation = command.sequence = 1;
        Check(NativeClockService::ValidateCommand(command), "canonical native read command is valid");
        for (unsigned field = 0; field != 9; ++field)
        {
            auto invalid = command;
            if (field == 0)
                --invalid.size;
            if (field == 1)
                ++invalid.version;
            if (field == 2)
                invalid.generation = 0;
            if (field == 3)
                invalid.sequence = 0;
            if (field == 4)
                invalid.reserved = 1;
            if (field == 5)
                invalid.operation = static_cast<ClockOperation>(5);
            if (field == 6)
                invalid.hour = 1;
            if (field == 7)
            {
                invalid.operation = ClockOperation::Set;
                invalid.hour = 24;
            }
            if (field == 8)
            {
                invalid.operation = ClockOperation::Set;
                invalid.minute = 61;
            }
            Check(!NativeClockService::ValidateCommand(invalid), "invalid command identity, operation or time cannot enter the native mailbox");
        }
    }

    void NativeServiceAdmissionContract()
    {
        namespace Admission = NativeServiceAdmission;
        using Admission::Category;
        using Admission::Token;
        Token input, render, clock, refused;
        Check(!Admission::IsClaimed(Category::Input) && !Admission::IsClaimed(Category::Render) && !Admission::IsClaimed(Category::Clock),
              "real shared native service admission starts without a retained service owner");
        refused = {Category::Render, 987};
        Check(!Admission::TryAcquire(Category::None, refused) && refused.category == Category::Render && refused.serial == 987 && !Admission::Release(refused),
              "invalid admission preserves output and an unowned token cannot release any service category");
        Check(Admission::TryAcquire(Category::Input, input) && Admission::TryAcquire(Category::Render, render) && input.serial != render.serial,
              "the real shared helper admits frontend input and rendering together with independent ownership serials");
        refused = {};
        Check(!Admission::TryAcquire(Category::Clock, clock) && !Admission::TryAcquire(Category::Input, refused) && !refused && !Admission::Release(refused) &&
                  Admission::IsClaimed(Category::Input) && Admission::IsClaimed(Category::Render),
              "clock and duplicate input startup refuse without granting a token capable of releasing the active frontend owners");
        Check(!Admission::Release({Category::Render, input.serial}) && Admission::IsClaimed(Category::Render),
              "a token from one category cannot release another category's independently owned admission");
        Check(Admission::Release(render) && !Admission::TryAcquire(Category::Clock, clock),
              "releasing rendering alone cannot clear input's clock exclusion during retained input cleanup");
        Check(Admission::Release(input) && Admission::TryAcquire(Category::Clock, clock),
              "clock startup becomes available only after every conflicting frontend claim is explicitly released");
        Check(!Admission::Release(input) && !Admission::Release(render) && Admission::IsClaimed(Category::Clock) &&
                  !Admission::TryAcquire(Category::Input, refused) && !Admission::TryAcquire(Category::Render, refused),
              "stale frontend cleanup cannot release a newer exclusive clock owner or admit another service");
        // No success/failure guess releases a claim: callers keep the token
        // throughout their explicit cleanup debt and release it only on proof.
        Check(Admission::IsClaimed(Category::Clock) && !Admission::TryAcquire(Category::Clock, refused),
              "a retained clock claim survives failed cleanup and duplicate startup until its exact owner explicitly releases it");
        const auto oldClock = clock;
        Check(Admission::Release(clock) && Admission::TryAcquire(Category::Clock, clock) && clock.serial != oldClock.serial && !Admission::Release(oldClock) &&
                  Admission::IsClaimed(Category::Clock),
              "a stale same-category token cannot release a later clock generation after admission reuse");
        Check(Admission::Release(clock), "the exact current clock token releases its own admission once cleanup is confirmed");
        Check(Admission::TryAcquire(Category::Input, input), "hold the real input service category before an actual native clock Begin");
        NativeClockService  blockedClock;
        ClockServiceStateV1 blockedState;
        Check(!blockedClock.Begin({}, blockedState) && !runtime.hooked.load() && !Admission::IsClaimed(Category::Clock) &&
                  Admission::IsClaimed(Category::Input) && blockedClock.End() && Admission::IsClaimed(Category::Input),
              "actual native clock admission refuses an existing input owner and its failed-owner End cannot release that foreign claim");
        Check(Admission::Release(input), "the actual input token remains releasable only by its original service owner");
        for (unsigned round = 0; round != 4; ++round)
        {
            std::atomic<unsigned> ready{0}, completed{0};
            std::atomic<bool>     start{false}, release{false}, inputWon{false}, clockWon{false}, inputReleased{false}, clockReleased{false};
            const auto            compete = [&](Category category, std::atomic<bool>& won, std::atomic<bool>& released)
            {
                Token token;
                ready.fetch_add(1);
                while (!start.load())
                    SwitchToThread();
                const bool acquired = Admission::TryAcquire(category, token);
                won.store(acquired);
                completed.fetch_add(1);
                while (!release.load())
                    SwitchToThread();
                released.store(acquired ? Admission::Release(token) : !Admission::Release(token));
            };
            std::thread inputThread([&] { compete(Category::Input, inputWon, inputReleased); });
            std::thread clockThread([&] { compete(Category::Clock, clockWon, clockReleased); });
            while (ready.load() != 2)
                SwitchToThread();
            start.store(true);
            while (completed.load() != 2)
                SwitchToThread();
            Check(inputWon.load() != clockWon.load() && Admission::IsClaimed(Category::Input) == inputWon.load() &&
                      Admission::IsClaimed(Category::Clock) == clockWon.load(),
                  "synchronized competing input and clock startup admits exactly one owner while its token is retained");
            release.store(true);
            inputThread.join();
            clockThread.join();
            Check(inputReleased.load() && clockReleased.load() && !Admission::IsClaimed(Category::Input) && !Admission::IsClaimed(Category::Clock),
                  "only the racing winner releases its actual claim while a failed contender cannot affect ownership");
        }
    }

    void NativeReadSetAndRestoration(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture);
        auto state = State(service);
        BYTE hour = 0xaa, minute = 0xbb;
        Check(runtime.clock.TryGet(&hour, &minute) == ClockResult::WrongThread && hour == 0xaa && minute == 0xbb &&
                  runtime.clock.TrySet(1, 2) == ClockResult::WrongThread && ChangeFreezePatch(false) == ClockResult::WrongThread,
              "even the producer on the adopted OS thread cannot access native SDK values outside its callback scope");
        ClockServiceStateV1 unchanged;
        unchanged.hour = 99;
        unchanged.lastCompletedSequence = 123;
        unchanged.lastCommandResult = ClockResult::OwnerConflict;
        runtime.publishedClock.version.fetch_add(1);
        const bool busyRead = service.Read(unchanged);
        runtime.publishedClock.version.fetch_add(1);
        Check(!busyRead && unchanged.hour == 99 && unchanged.lastCompletedSequence == 123 && unchanged.lastCommandResult == ClockResult::OwnerConflict,
              "an in-progress combined clock/command publication refuses boundedly without changing the caller's values or acknowledgement");
        auto command = Command(state, 1);
        Check(service.Submit(command) && !service.Submit(Command(state, 2)), "an occupied production mailbox cannot silently overwrite a pending command");
        fixture.invoke();
        state = State(service);
        Check(state.lastCommandResult == ClockResult::Ok && state.reads == 1 && state.lastCompletedSequence == 1 && !service.Submit(command),
              "actual SDK Read completes once and rejects replayed sequence numbers");
        command = Command(state, 2, ClockOperation::Set);
        command.hour = 23;
        command.minute = 60;
        std::vector<unsigned char> before(ClockImageFixture::DataSize);
        std::memcpy(before.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), before.size());
        Check(service.Submit(command), "classic-compatible minute 60 is accepted without DE script-setter normalization");
        fixture.invoke();
        state = State(service);
        Check(state.lastCommandResult == ClockResult::Ok && state.sets == 1 && state.hour == 23 && state.minute == 60 && state.seconds == 27 &&
                  state.nativeTimer == 10000 && state.lastMinuteChange == 10000,
              "the actual SDK Set changes displayed time and minute timestamp while preserving seconds and native timer");
        bool onlyClockFields = true;
        for (std::size_t index = 0; index != before.size(); ++index)
        {
            const auto rva = ClockImageFixture::DataRva + index;
            const bool owned = rva == hoursRva || rva == minutesRva || (rva >= lastChangeRva && rva < lastChangeRva + 4);
            if (!owned && before[index] != *reinterpret_cast<const unsigned char*>(fixture.image + rva))
                onlyClockFields = false;
        }
        Check(onlyClockFields, "independent whole-data-section comparison finds no timer, calendar, store-slot or unrelated writes");
        state = Execute(service, fixture, ClockOperation::ResetTimeFrozen, 3);
        Check(state.lastCommandResult == ClockResult::Ok && state.resets == 1 && !state.frozen && !state.patchInstalled,
              "already-unfrozen SDK reset succeeds even when branch writes have not been independently admitted");
        auto stale = Command(state, 4);
        --stale.generation;
        Check(!service.Submit(stale) && !service.Renew(state.generation + 1) && service.Renew(state.generation),
              "stale generations cannot queue or renew another owner's native clock lifetime");
        bool        foreignRefused = false;
        std::thread foreign(
            [&]
            {
                ClockServiceStateV1 unchanged;
                unchanged.hour = 99;
                foreignRefused = !service.Submit(Command(state, 4)) && !service.Renew(state.generation) && !service.Read(unchanged) && unchanged.hour == 99 &&
                                 !service.End();
            });
        foreign.join();
        Check(foreignRefused, "foreign producer calls cannot publish output, enqueue, renew or end the adopted owner's service");
        fixture.Put(nativeTimerRva, std::uint32_t{15000});
        FinishOwned(service, fixture);
        Check(fixture.Get<std::uint8_t>(hoursRva) == 12 && fixture.Get<std::uint8_t>(minutesRva) == 34 && fixture.Get<std::uint16_t>(secondsRva) == 27 &&
                  fixture.Get<std::uint32_t>(nativeTimerRva) == 15000 && fixture.Get<std::uint32_t>(lastChangeRva) == 14900,
              "normal owner release restores captured display and fractional phase without rewinding the native game timer");
    }

    void NativeCoherentCommandPublication(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture);
        auto state = State(service);
        auto command = Command(state, 1, ClockOperation::Set);
        command.hour = 18;
        command.minute = 19;
        // This owned fixture already adopted the real SDK/native owner. Split
        // its command and callback publication phases deliberately, so the
        // regression observes the exact gap without a shipping test seam or
        // timing a racing OS thread. Other cases exercise the real mailbox.
        ++callbackDepth;
        CompleteCommand(command);
        --callbackDepth;
        Check(runtime.lastCompletedSequence.load() == 1 && runtime.lastCommandResult.load() == ClockResult::Ok && fixture.Get<std::uint8_t>(hoursRva) == 18 &&
                  fixture.Get<std::uint8_t>(minutesRva) == 19,
              "the actual SDK Set completes and changes owned native memory before callback readback is published");
        state = State(service);
        Check(state.lastCompletedSequence == 0 && state.lastCommandResult == ClockResult::NotInitialized && state.hour == 12 && state.minute == 34 &&
                  state.seconds == 27 && state.nativeTimer == 10000 && state.lastMinuteChange == 9900,
              "delayed native readback cannot expose a new Set acknowledgement paired with the previous clock values");
        fixture.invoke();
        state = State(service);
        Check(state.lastCompletedSequence == 1 && state.lastCommandResult == ClockResult::Ok && state.hour == 18 && state.minute == 19 && state.seconds == 27 &&
                  state.nativeTimer == 10000 && state.lastMinuteChange == 10000,
              "the qualified callback publishes the completed Set and its actual native readback in one epoch");

        fixture.Put(minutesRva, std::uint8_t{20});
        fixture.Put(nativeTimerRva, std::uint32_t{10500});
        fixture.Put(lastChangeRva, std::uint32_t{10400});
        fixture.invoke();
        state = State(service);
        Check(state.lastCompletedSequence == 1 && state.lastCommandResult == ClockResult::Ok && state.hour == 18 && state.minute == 20 &&
                  state.nativeTimer == 10500 && state.lastMinuteChange == 10400,
              "ordinary captures carry the latest completed command forward while publishing natural clock advancement");

        command = Command(state, 2, ClockOperation::Set);
        command.hour = 20;
        command.minute = 21;
        ClockTestFaults::partialDataWrites.store(1);
        ++callbackDepth;
        CompleteCommand(command);
        --callbackDepth;
        Check(runtime.lastCompletedSequence.load() == 2 && runtime.lastCommandResult.load() == ClockResult::WriteFailure && runtime.dataRecovery.load(),
              "an actual partial Set records its failed completion and retains native restoration before publication");
        state = State(service);
        Check(state.lastCompletedSequence == 1 && state.lastCommandResult == ClockResult::Ok && state.hour == 18 && state.minute == 20 &&
                  state.nativeTimer == 10500 && state.lastMinuteChange == 10400,
              "an unpublished failed command cannot mix its result with the preceding successful clock publication");
        fixture.invoke();
        state = State(service);
        Check(state.lastCompletedSequence == 2 && state.lastCommandResult == ClockResult::WriteFailure && state.restored && state.hour == 12 &&
                  state.minute == 34 && state.seconds == 27 && state.nativeTimer == 10500 && state.lastMinuteChange == 10400,
              "the recovery callback pairs the latest failed completion with its verified restored clock values");
        FinishOwned(service, fixture);
        state = State(service);
        Check(state.lastCompletedSequence == 2 && state.lastCommandResult == ClockResult::WriteFailure && state.restored && state.hour == 12 &&
                  state.minute == 34 && state.seconds == 27 && state.nativeTimer == 10500 && state.lastMinuteChange == 10400,
              "End retains the final coherent completion and restoration values after native detachment");
    }

    void NativeFailureAndLease(ClockImageFixture& fixture)
    {
        for (bool maximum : {false, true})
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            const auto state = State(service);
            if (maximum)
                runtime.started = GetTickCount64() - ClockServiceMaximumDurationMsV1 - 1;
            else
                runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
            Check(!service.Renew(state.generation) && !runtime.active.load() && runtime.failure.load() == ClockResult::Expired,
                  "elapsed renewable lease and maximum duration each stop rather than reviving an expired owner");
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            Execute(service, fixture, ClockOperation::Set);
            auto modified = NativeClockProfile::getterCase;
            modified[0] ^= 1;
            fixture.Patch(NativeClockProfile::getterCaseRva, modified.data(), modified.size());
            fixture.invoke();
            const auto state = State(service);
            Check(state.failure == ClockResult::BindingChanged && !state.active && !state.restored && state.restoreFailures && !service.End() &&
                      service.IsRetained(),
                  "changed native byte pins retain the hook and snapshot instead of claiming unverified restoration");
            fixture.Patch(NativeClockProfile::getterCaseRva, NativeClockProfile::getterCase.data(), NativeClockProfile::getterCase.size());
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            auto command = Command(State(service), 1, ClockOperation::Set);
            command.hour = 20;
            command.minute = 45;
            Check(service.Submit(command), "queue the actual native write before injecting a later clock-field page failure");
            DWORD protection = 0, ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + lastChangeRva), 4, PAGE_READONLY, &protection) != FALSE,
                  "remove write permission only from the owned timestamp/seconds image page");
            fixture.invoke();
            auto state = State(service);
            Check(state.failure == ClockResult::WriteFailure && state.dataRecoveryPending && !state.restored && state.restoreFailures &&
                      fixture.Get<std::uint8_t>(hoursRva) == 12 && !service.End() && service.IsRetained(),
                  "partial native Set and failed seconds restoration preserve explicit data recovery ownership");
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + lastChangeRva), 4, protection, &ignored) != FALSE,
                  "restore the owned data page so the actual native owner can retry its saved snapshot");
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            ClockTestFaults::partialDataWrites.store(1);
            const auto state = Execute(service, fixture, ClockOperation::Set);
            Check(state.lastCommandResult == ClockResult::WriteFailure && state.failure == ClockResult::WriteFailure && state.restored &&
                      !state.dataRecoveryPending && fixture.Get<std::uint32_t>(lastChangeRva) == 9900,
                  "an actual short WriteProcessMemory data copy cannot report success and is restored by the saved native snapshot");
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            ClockTestFaults::failEnumeration.store(true);
            const auto state = Execute(service, fixture, ClockOperation::Set);
            Check(state.lastCommandResult == ClockResult::Busy && state.failure == ClockResult::Busy && !state.sets && !state.retainedPeerThreads,
                  "Thread32Next failure is rejected as incomplete enumeration instead of admitting an unparked native write");
            FinishOwned(service, fixture);
        }
    }

    void NativeCaptureAndWraparound(ClockImageFixture& fixture)
    {
        for (unsigned invalid = 0; invalid != 3; ++invalid)
        {
            NativeClockService service;
            BeginOwned(service, fixture, false);
            if (invalid == 0)
                fixture.Put(minutesRva, std::uint8_t{60});
            if (invalid == 1)
                fixture.Put(secondsRva, std::uint16_t{61});
            if (invalid == 2)
                fixture.Put(lastChangeRva, std::uint32_t{8000});
            fixture.invoke();
            const auto state = State(service);
            Check(state.failure == ClockResult::BindingChanged && !state.ready && state.restored && !state.sets && !state.dataRecoveryPending,
                  "noncanonical initial minute, seconds or fractional phase refuses ownership before any native data writes");
            FinishOwned(service, fixture);
        }
        NativeClockService service;
        BeginOwned(service, fixture, false);
        fixture.Put(nativeTimerRva, std::uint32_t{50});
        fixture.Put(lastChangeRva, std::uint32_t{0xffffffce});
        fixture.invoke();
        Check(State(service).ready, "native DWORD timer wraparound retains a valid one-hundred-millisecond captured phase");
        Execute(service, fixture, ClockOperation::Set);
        fixture.Put(nativeTimerRva, std::uint32_t{125});
        FinishOwned(service, fixture);
        Check(fixture.Get<std::uint32_t>(nativeTimerRva) == 125 && fixture.Get<std::uint32_t>(lastChangeRva) == 25,
              "restore uses modular native timer arithmetic instead of rejecting or rewinding a wrapped timer");
    }

    void NativeReadOnlyRetirement(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture);
        ClockTestFaults::clockDataWrites.store(0);
        ClockTestFaults::clockCodeExchanges.store(0);
        const auto state = Execute(service, fixture, ClockOperation::Read);
        Check(state.lastCommandResult == ClockResult::Ok && state.reads == 1 && !runtime.nativeTouched.load(),
              "actual SDK Read leaves the native clock mutation obligation untouched");
        // Model natural engine advancement after capture. Read-only cleanup
        // must retire ownership without replaying or rebasing the old snapshot.
        fixture.Put(hoursRva, std::uint8_t{17});
        fixture.Put(minutesRva, std::uint8_t{18});
        fixture.Put(secondsRva, std::uint16_t{19});
        fixture.Put(nativeTimerRva, std::uint32_t{25000});
        fixture.Put(lastChangeRva, std::uint32_t{24750});
        std::vector<unsigned char> natural(ClockImageFixture::DataSize);
        std::memcpy(natural.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), natural.size());
        FinishOwned(service, fixture);
        Check(!ClockTestFaults::clockDataWrites.load() && !ClockTestFaults::clockCodeExchanges.load() && !runtime.nativeTouched.load(),
              "actual read-only retirement makes zero native clock WriteProcessMemory calls or atomic code exchanges");
        Check(std::memcmp(natural.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), natural.size()) == 0 &&
                  fixture.Get<std::uint8_t>(hoursRva) == 17 && fixture.Get<std::uint8_t>(minutesRva) == 18 && fixture.Get<std::uint16_t>(secondsRva) == 19 &&
                  fixture.Get<std::uint32_t>(nativeTimerRva) == 25000 && fixture.Get<std::uint32_t>(lastChangeRva) == 24750,
              "read-only cleanup preserves the complete naturally advanced data image instead of restoring old display or fractional-minute fields");
    }

    void NativePeerResumeRecovery(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture);
        const auto stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(stop != nullptr, "create an owned disposable peer lifetime for real suspend/resume recovery");
        std::atomic<DWORD> peerId{0};
        std::thread        peer(
            [&]
            {
                peerId.store(GetCurrentThreadId());
                WaitForSingleObject(stop, INFINITE);
            });
        while (!peerId.load())
            SwitchToThread();
        const auto handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, peerId.load());
        Check(handle != nullptr, "observe only the disposable fixture peer's suspension count");
        ClockTestFaults::resumeTarget.store(peerId.load());
        ClockTestFaults::failResumes.store(64);
        const auto before = State(service);
        Check(service.Submit(Command(before, 1, ClockOperation::Set)), "queue the actual SDK Set before holding its peer-resume recovery debt");
        fixture.invoke();
        const auto state = State(service);
        // A retained peer prevents the callback from reaching coherent native
        // readback/publication. The internal SDK completion is not yet a
        // producer acknowledgement; Execute's ordinary completion assertion
        // would incorrectly require exposing it with the old clock values.
        Check(runtime.lastCompletedSequence.load() == 1 && runtime.lastCommandResult.load() == ClockResult::RestoreFailure &&
                  state.lastCompletedSequence == before.lastCompletedSequence && state.lastCommandResult == before.lastCommandResult &&
                  state.hour == before.hour && state.minute == before.minute && state.seconds == before.seconds && !state.pendingSequence && !state.inFlight,
              "retained peer recovery withholds a newly completed SDK result until coherent callback readback can be published");
        Check(state.failure == ClockResult::RestoreFailure && state.retainedPeerThreads == 1 && state.dataRecoveryPending && !state.restored &&
                  !service.End() && service.IsRetained(),
              "failed ResumeThread keeps the actual suspended handle and native snapshot owned across callback and End");
        const auto count = ::SuspendThread(handle);
        Check(count == 1 && ::ResumeThread(handle) == 2, "independent suspend-count probe proves the retained peer was not falsely acknowledged as running");
        ClockTestFaults::failResumes.store(0);
        fixture.invoke();
        const auto recovered = State(service);
        Check(!recovered.retainedPeerThreads && runtime.recoveryPeers.empty() && recovered.restored && recovered.lastCompletedSequence == 1 &&
                  recovered.lastCommandResult == ClockResult::RestoreFailure && recovered.hour == before.hour && recovered.minute == before.minute &&
                  recovered.seconds == before.seconds,
              "the next native callback releases the exact retained peer and publishes its failed SDK result with verified restored clock values");
        SetEvent(stop);
        peer.join();
        CloseHandle(handle);
        CloseHandle(stop);
        FinishOwned(service, fixture);
    }

    void NativeEngineThreadHandoff(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture, false);
        const auto run = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        const auto done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Check(run && done, "create bounded handoff events for a distinct native engine fixture thread");
        std::atomic<bool>  stop{false};
        std::atomic<DWORD> engine{0};
        std::thread        worker(
            [&]
            {
                engine.store(GetCurrentThreadId());
                while (WaitForSingleObject(run, 5000) == WAIT_OBJECT_0)
                {
                    if (stop.load())
                        break;
                    fixture.invoke();
                    SetEvent(done);
                }
            });
        const auto tick = [&]
        {
            Check(service.Renew(State(service).generation) && SetEvent(run) && WaitForSingleObject(done, 5000) == WAIT_OBJECT_0,
                  "producer hands one real qualified callback to a separate engine thread within a bounded wait");
        };
        tick();
        auto state = State(service);
        Check(state.ready && state.engineThread == engine.load() && state.engineThread != state.producerThread,
              "the first native callback adopts its actual thread rather than the producer constructor thread");
        auto command = Command(state, 1, ClockOperation::Set);
        command.hour = 18;
        command.minute = 19;
        Check(service.Submit(command), "the producer queues an actual SDK write for the distinct engine owner");
        tick();
        state = State(service);
        Check(state.lastCompletedSequence == 1 && state.lastCommandResult == ClockResult::Ok && state.hour == 18 && state.minute == 19,
              "the separate native owner consumes the mailbox and performs actual SDK data writes");
        fixture.invoke();
        state = State(service);
        Check(state.failure == ClockResult::WrongThread && state.foreignCallbacks == 1 && !state.active && !state.restored,
              "a later qualified callback on another thread stops safely without taking over the adopted native owner");
        Check(!service.End() && service.IsRetained(), "producer End waits for separate engine-owner restoration");
        Check(SetEvent(run) && WaitForSingleObject(done, 5000) == WAIT_OBJECT_0, "stopped owners still receive their final native restoration callback");
        Check(service.End() && !service.IsRetained() && State(service).restored,
              "producer detaches only after the separate native engine owner acknowledges restoration");
        Check(!NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "separate-thread cleanup releases its process-wide claim after native restoration and detachment");
        stop.store(true);
        SetEvent(run);
        worker.join();
        CloseHandle(run);
        CloseHandle(done);
        Check(CheckBinding(true), "separate-thread handoff leaves original native code bytes restored");
    }

    void NativeCommittedHookResumeRecovery(ClockImageFixture& fixture)
    {
        NativeClockService service;
        BeginOwned(service, fixture, false, false);
        const auto stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(stop != nullptr, "create a disposable parked peer for committed hook recovery");
        std::atomic<DWORD> peerId{0};
        std::thread        peer(
            [&]
            {
                peerId.store(GetCurrentThreadId());
                WaitForSingleObject(stop, INFINITE);
            });
        while (!peerId.load())
            SwitchToThread();
        ClockTestFaults::resumeTarget.store(peerId.load());
        ClockTestFaults::failResumes.store(64);
        const bool changed = ChangeHooks(true);
        const auto state = State(service);
        Check(!changed && state.hooked && state.retainedPeerThreads == 1 && service.IsRetained() &&
                  NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) &&
                  !CheckCode(NativeClockProfile::hookEntryRva, NativeClockProfile::hookEntry),
              "successful Detours commit followed by resume failure keeps the actual installed hook recorded and its suspended peer retained");
        Check(!service.End() && service.IsRetained(), "End cannot acknowledge a committed hook while a real suspended peer remains owned");
        ClockTestFaults::failResumes.store(0);
        Check(service.End() && !service.IsRetained() && !State(service).hooked && CheckBinding(true) &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "retained peer recovery then detaches the truly committed hook instead of losing it behind a failed transaction result");
        SetEvent(stop);
        peer.join();
        CloseHandle(stop);
    }

    void OriginalRaises()
    {
        RaiseException(0xe001c10c, 0, 0, nullptr);
    }

    bool InvokeOriginalException()
    {
        // Keep the SEH boundary in a normal compiler-owned frame: the synthetic
        // native code deliberately has no invented unwind metadata.
        __try
        {
            HookClockContinuation();
        }
        __except (GetExceptionCode() == 0xe001c10c ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }

    void NativeCallbackLifetime(ClockImageFixture& fixture)
    {
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            ++callbackDepth;
            fixture.invoke();
            --callbackDepth;
            const auto state = State(service);
            Check(state.failure == ClockResult::Busy && state.nestedCallbacks == 1 && !state.inFlight && !callbackDepth && !state.restored,
                  "nested actual hook callbacks stop without consuming SDK work or losing outer lifetime accounting");
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            const auto original = runtime.originalContinuation;
            runtime.originalContinuation = &OriginalRaises;
            const bool caught = InvokeOriginalException();
            runtime.originalContinuation = original;
            const auto state = State(service);
            Check(caught && state.failure == ClockResult::OriginalException && !state.inFlight && !callbackDepth && !state.restored,
                  "an original-continuation exception propagates while production finally releases callback debt and retains native restoration");
            FinishOwned(service, fixture);
        }
    }

    void NativeReadOnlyGameTimeReader(ClockImageFixture& fixture)
    {
        NativeGameTimeReader reader;
        GameTimeStateV1      state;
        std::memset(&state, 0x5a, sizeof(state));
        const auto untouched = state;
        const auto unchanged = [&] { return std::memcmp(&state, &untouched, sizeof(state)) == 0; };
        Check(reader.Read(state) == Status::NotInitialized && unchanged() && !reader.IsBound(),
              "an unbound read-only game-time service preserves every caller output byte");
        Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "the shipping game-time executable hash refuses this ordinary host");
        fixture.Seed();
        ClockReaderTestHost::image = fixture.image;
        Check(reader.Bind() == Status::Ok && reader.IsBound(), "the production reader admits the exact code, metadata and pages of its owned image fixture");
        const auto writesBefore = ClockTestFaults::clockDataWrites.load(), patchesBefore = ClockTestFaults::clockCodeExchanges.load();
        Check(reader.Read(state) == Status::Ok && state.size == sizeof(GameTimeStateV1) && state.version == GameTimeServiceVersionV1 &&
                  state.producerThread == GetCurrentThreadId() && state.hour == 12 && state.minute == 34 && state.seconds == 27 && state.nativeTimer == 10000 &&
                  state.lastMinuteChange == 9900 && state.millisecondsPerGameMinute == 1000 && !state.reserved[0] && !state.reserved[1] && !state.reserved[2],
              "two matching complete native reads publish the real timer, clock and minute duration without synthesized time");
        std::thread foreign(
            [&]
            {
                GameTimeStateV1 output = untouched;
                Check(
                    reader.Bind() == Status::WrongThread && reader.Read(output) == Status::WrongThread && std::memcmp(&output, &untouched, sizeof(output)) == 0,
                    "a foreign caller cannot steal reader ownership or publish a native sample");
            });
        foreign.join();
        state = untouched;
        ClockReaderTestHost::samples.store(0);
        ClockReaderTestHost::mismatchSample.store(2);
        Check(reader.Read(state) == Status::Unsupported && unchanged() && ClockReaderTestHost::samples.load() == 2,
              "a mismatch between complete native samples refuses publication without retries or a cached success");
        ClockReaderTestHost::mismatchSample.store(0);
        for (unsigned field = 0; field != 4; ++field)
        {
            fixture.Seed();
            if (field == 0)
                fixture.Put(NativeClockProfile::hoursRva, std::uint8_t{24});
            if (field == 1)
                fixture.Put(NativeClockProfile::minutesRva, std::uint8_t{61});
            if (field == 2)
                fixture.Put(NativeClockProfile::minuteDurationRva, std::uint32_t{0});
            if (field == 3)
                fixture.Put(NativeClockProfile::minuteDurationRva, std::uint32_t{3600001});
            state = untouched;
            Check(reader.Read(state) == Status::Unsupported && unchanged(), "invalid native clock fields cannot escape as a successful game-time sample");
        }
        fixture.Seed();
        fixture.Put(NativeClockProfile::minutesRva, std::uint8_t{60});
        fixture.Put(NativeClockProfile::secondsRva, std::uint16_t{65535});
        Check(reader.Read(state) == Status::Ok && state.minute == 60 && state.seconds == 65535,
              "raw native seconds and the existing SDK minute-60 boundary are preserved instead of inventing normalized time");
        fixture.Seed();
        auto changed = NativeClockProfile::getterCase;
        changed[0] ^= 1;
        fixture.Patch(NativeClockProfile::getterCaseRva, changed.data(), changed.size());
        state = untouched;
        Check(reader.Read(state) == Status::Unsupported && unchanged(), "changed live getter instructions invalidate an already bound reader");
        fixture.Patch(NativeClockProfile::getterCaseRva, NativeClockProfile::getterCase.data(), NativeClockProfile::getterCase.size());
        DWORD originalProtection = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeClockProfile::nativeTimerRva), sizeof(std::uint32_t), PAGE_NOACCESS,
                               &originalProtection) != FALSE,
              "deny native timer pages only inside the independently owned image");
        state = untouched;
        Check(reader.Read(state) == Status::Unsupported && unchanged(), "lost native page access refuses a fresh read while preserving output");
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeClockProfile::nativeTimerRva), sizeof(std::uint32_t), originalProtection,
                               &ignored) != FALSE,
              "restore the owned native timer page after its read-only refusal test");
        Check(reader.Read(state) == Status::Ok && ClockTestFaults::clockDataWrites.load() == writesBefore &&
                  ClockTestFaults::clockCodeExchanges.load() == patchesBefore,
              "successful and failed game-time reads perform no clock writes, branch patches or hook ownership changes");

        NativeClockService service;
        BeginOwned(service, fixture);
        Check(reader.Read(state) == Status::Ok,
              "read-only game time coexists with the qualified clock continuation hook while its observed calculation span remains original");
        if (NativeClockProfile::clockBoundaryReviewed)
        {
            const auto frozen = Execute(service, fixture, ClockOperation::Freeze);
            Check(frozen.lastCommandResult == ClockResult::Ok && frozen.patchInstalled, "install only the existing clock owner's reviewed freeze branch");
            state = untouched;
            Check(reader.Read(state) == Status::Unsupported && unchanged(),
                  "read-only time refuses an altered calculation span without claiming or changing the clock writer's ownership");
        }
        FinishOwned(service, fixture);
        Check(reader.Read(state) == Status::Ok, "clock-owner retirement restores the original reader binding without a new time trial");
        reader.Reset();
        state = untouched;
        Check(!reader.IsBound() && reader.Read(state) == Status::NotInitialized && unchanged(),
              "reader reset discards only its local binding and does not return the earlier native sample");
        ClockReaderTestHost::image = 0;
        std::puts("Production read-only game-time reader fixture completed");
    }

    struct TimingInvocation
    {
        bool                 minute = false;
        float                scale = 0;
        std::uint32_t        duration = 0;
        unsigned             refusal = 0;
        ClockResult          result = ClockResult::NotInitialized;
        ContinuationFunction original = nullptr;
    } timingInvocation;

    void TimingContinuation()
    {
        // The real qualified hook has already adopted its owner. This test
        // replaces only the owned fixture's original continuation, just as the
        // existing exception fixture does; production gets no test command path.
        const auto priorGeneration = timingCallbackGeneration;
        const auto priorAdmission = runtime.admissionGeneration.load();
        if (timingInvocation.refusal == 1)
            timingCallbackGeneration = priorGeneration + 1;
        if (timingInvocation.refusal == 2)
            runtime.admissionGeneration.store(priorAdmission + 1);
        if (timingInvocation.refusal == 3)
            ++callbackDepth;
        NativeGameTimingAccess& access = runtime.access;
        timingInvocation.result = timingInvocation.minute ? access.SetMinuteDuration(timingInvocation.duration) : access.SetTimeScale(timingInvocation.scale);
        if (timingInvocation.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(priorAdmission);
        timingCallbackGeneration = priorGeneration;
        timingInvocation.original();
    }

    ClockResult InvokeTiming(ClockImageFixture& fixture, bool minute, float scale, std::uint32_t duration, unsigned refusal = 0)
    {
        timingInvocation = {minute, scale, duration, refusal, ClockResult::NotInitialized, runtime.originalContinuation};
        runtime.originalContinuation = &TimingContinuation;
        fixture.invoke();
        runtime.originalContinuation = timingInvocation.original;
        Check(!runtime.inFlight.load() && !callbackDepth && !timingCallbackGeneration,
              "the actual hook finally retires the qualified timing generation and callback scope");
        return timingInvocation.result;
    }

    void NativeTimingWritesAndRestoration(ClockImageFixture& fixture)
    {
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            NativeGameTimingAccess& access = runtime.access;
            const auto              beforeWrites = ClockTestFaults::clockDataWrites.load();
            Check(access.SetTimeScale(2.0f) == ClockResult::WrongThread && access.SetMinuteDuration(2000) == ClockResult::WrongThread &&
                      ClockTestFaults::clockDataWrites.load() == beforeWrites && !runtime.timingCaptured.load(),
                  "matching the adopted OS thread outside its natural callback does not grant timing write authority");
            std::thread foreign(
                [&]
                {
                    Check(access.SetTimeScale(2.0f) == ClockResult::WrongThread && access.SetMinuteDuration(2000) == ClockResult::WrongThread,
                          "a foreign producer cannot invoke the actual timing writer");
                });
            foreign.join();
            for (unsigned refusal = 1; refusal <= 3; ++refusal)
                Check(InvokeTiming(fixture, false, 2.0f, 0, refusal) == (refusal == 3 ? ClockResult::WrongThread : ClockResult::OwnerConflict) &&
                          !runtime.timingCaptured.load() && !runtime.timingDirty.load() && ClockTestFaults::clockDataWrites.load() == beforeWrites,
                      "stale callback generations, mismatched admission tokens and nested scopes cannot capture or write timing values");
            Check(runtime.active.load(), "unauthorized timing calls leave the actual current engine owner active");
            for (float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
                Check(InvokeTiming(fixture, false, invalid, 0) == ClockResult::InvalidArgument && !runtime.timingCaptured.load(),
                      "nonfinite scale values refuse without acquiring a native restoration snapshot");
            for (std::uint32_t invalid : {0u, 3600001u})
                Check(InvokeTiming(fixture, true, 0, invalid) == ClockResult::InvalidArgument && !runtime.timingCaptured.load(),
                      "minute-duration writes preserve the existing checked native reader domain");
            ClockTestFaults::failEnumeration.store(true);
            Check(InvokeTiming(fixture, false, 2.0f, 0) == ClockResult::Busy && !runtime.timingDirty.load() &&
                      ClockTestFaults::clockDataWrites.load() == beforeWrites,
                  "a refused peer transaction performs no timing write or dirty capture");
            Check(InvokeTiming(fixture, false, -0.5f, 0) == ClockResult::Ok && fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == -0.5f &&
                      runtime.originalTimeScale == 1.0f && runtime.timingDirty.load() == timingScaleBit && !runtime.timingRecovery.load() &&
                      !runtime.nativeTouched.load(),
                  "the real native writer preserves finite signed scale semantics and keeps successful ownership distinct from failed-write recovery");
            Check(InvokeTiming(fixture, false, 0.0f, 0) == ClockResult::Ok && fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 0 &&
                      runtime.originalTimeScale == 1.0f,
                  "repeated native scale writes retain the first original instead of recapturing an already mutated scalar");
            Check(InvokeTiming(fixture, true, 0, 2500) == ClockResult::Ok && fixture.Get<std::uint32_t>(NativeClockProfile::minuteDurationRva) == 2500 &&
                      runtime.originalMinuteDuration == 1000 && runtime.timingDirty.load() == (timingScaleBit | timingMinuteBit) &&
                      !runtime.timingRecovery.load(),
                  "minute duration uses the same real native owner while retaining its own original value");
            const auto mutated = State(service);
            Check(mutated.active && mutated.ready && !mutated.restored && mutated.dataRecoveryPending && !runtime.dataRecovery.load(),
                  "unchanged V1 diagnostics aggregate successful timing restoration debt without pretending the displayed clock was mutated");

            fixture.Put(NativeClockProfile::hoursRva, std::uint8_t{13});
            fixture.Put(NativeClockProfile::minutesRva, std::uint8_t{40});
            fixture.Put(NativeClockProfile::secondsRva, std::uint16_t{31});
            fixture.Put(NativeClockProfile::nativeTimerRva, std::uint32_t{10800});
            fixture.Put(NativeClockProfile::lastChangeRva, std::uint32_t{10500});
            std::vector<unsigned char> expected(ClockImageFixture::DataSize);
            std::memcpy(expected.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), expected.size());
            const float         originalScale = 1;
            const std::uint32_t originalDuration = 1000;
            std::memcpy(expected.data() + NativeGameTimingProfile::timeScaleRva - ClockImageFixture::DataRva, &originalScale, sizeof(originalScale));
            std::memcpy(expected.data() + NativeClockProfile::minuteDurationRva - ClockImageFixture::DataRva, &originalDuration, sizeof(originalDuration));
            const auto endWrites = ClockTestFaults::clockDataWrites.load();
            const auto endPatches = ClockTestFaults::clockCodeExchanges.load();
            Check(!service.End() && service.IsRetained() && State(service).dataRecoveryPending,
                  "producer End retains timing originals and cannot restore them from the caller's thread");
            FinishOwned(service, fixture);
            Check(std::memcmp(expected.data(), reinterpret_cast<const void*>(fixture.image + ClockImageFixture::DataRva), expected.size()) == 0 &&
                      ClockTestFaults::clockDataWrites.load() == endWrites + 2 && ClockTestFaults::clockCodeExchanges.load() == endPatches,
                  "timing-only native retirement restores exactly scale and duration while preserving progressed H/M/S, last-minute change and global timer");
        }
        for (bool minute : {false, true})
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            const auto target = minute ? NativeClockProfile::minuteDurationRva : NativeGameTimingProfile::timeScaleRva;
            const auto bit = minute ? timingMinuteBit : timingScaleBit;
            ClockTestFaults::timingWriteAddress = fixture.image + target;
            ClockTestFaults::partialTimingWrites.store(1);
            Check(InvokeTiming(fixture, minute, 1.2345f, 4321) == ClockResult::WriteFailure && runtime.timingDirty.load() == bit &&
                      runtime.timingRecovery.load() == bit && (runtime.timingCaptured.load() & bit) && !runtime.active.load() && !runtime.nativeTouched.load(),
                  "a partial real timing write retains its original before reporting failure and does not claim a CClock time mutation");
            const auto failed = State(service);
            Check(failed.failure == ClockResult::WriteFailure && failed.dataRecoveryPending && !failed.ready && !failed.restored && !service.End() &&
                      service.IsRetained(),
                  "V1 cleanup/readiness and producer End cannot hide an unconfirmed native timing write");
            ClockTestFaults::failTimingWrites.store(100);
            fixture.invoke();
            const auto pending = State(service);
            Check(pending.restoreFailures && pending.dataRecoveryPending && !pending.restored && runtime.timingRecovery.load() == bit && !service.End() &&
                      service.IsRetained(),
                  "a real native timing rollback failure keeps the same hook, snapshot and admission owner for retry");
            ClockTestFaults::failTimingWrites.store(0);
            FinishOwned(service, fixture);
            Check(minute ? fixture.Get<std::uint32_t>(target) == 1000 : fixture.Get<float>(target) == 1.0f,
                  "a later qualified callback restores the actual first timing value before native ownership retires");
            ClockTestFaults::timingWriteAddress = 0;
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            Check(InvokeTiming(fixture, false, 1.5f, 0) == ClockResult::Ok && InvokeTiming(fixture, true, 0, 2000) == ClockResult::Ok,
                  "one existing native owner can own both timing scalars before a real SDK clock command");
            auto command = Command(State(service), 1, ClockOperation::Set);
            command.hour = 18;
            command.minute = 25;
            Check(service.Submit(command), "mixed timing ownership keeps the existing CClock mailbox usable without extending its protocol");
            fixture.invoke();
            Check(State(service).lastCommandResult == ClockResult::Ok && runtime.nativeTouched.load() && runtime.timingDirty.load(),
                  "a real SDK CClock mutation keeps separate timing and displayed-clock restoration obligations");
            fixture.Put(NativeClockProfile::nativeTimerRva, std::uint32_t{10800});
            FinishOwned(service, fixture);
            Check(fixture.Get<std::uint8_t>(NativeClockProfile::hoursRva) == 12 && fixture.Get<std::uint8_t>(NativeClockProfile::minutesRva) == 34 &&
                      fixture.Get<std::uint16_t>(NativeClockProfile::secondsRva) == 27 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::lastChangeRva) == 10700 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::nativeTimerRva) == 10800 &&
                      fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1.0f &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::minuteDurationRva) == 1000,
                  "mixed retirement restores original timing scalars and preserves the real CClock fractional-minute rebase without rewinding global time");
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            auto changed = NativeGameTimingProfile::elapsedMultiplier;
            changed[0] ^= 1;
            fixture.Patch(NativeGameTimingProfile::elapsedMultiplierRva, changed.data(), changed.size());
            const auto writes = ClockTestFaults::clockDataWrites.load();
            Check(InvokeTiming(fixture, false, 2.0f, 0) == ClockResult::BindingChanged && !runtime.timingCaptured.load() && !runtime.timingDirty.load() &&
                      ClockTestFaults::clockDataWrites.load() == writes,
                  "changed native timing dataflow refuses mutation before capturing a value or writing memory");
            fixture.Patch(NativeGameTimingProfile::elapsedMultiplierRva, NativeGameTimingProfile::elapsedMultiplier.data(),
                          NativeGameTimingProfile::elapsedMultiplier.size());
            FinishOwned(service, fixture);
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            fixture.Put(NativeGameTimingProfile::timeScaleRva, -0.0f);
            Check(InvokeTiming(fixture, false, 1.5f, 0) == ClockResult::Ok,
                  "the native writer captures the actual original scalar immediately before the first owned write");
            auto changed = NativeGameTimingProfile::scriptScale;
            changed[0] ^= 1;
            fixture.Patch(NativeGameTimingProfile::scriptScaleRva, changed.data(), changed.size());
            Check(!service.End(), "changed timing pins cannot retire a dirty writer from the producer thread");
            fixture.invoke();
            Check(!State(service).restored && State(service).dataRecoveryPending && !service.End() && service.IsRetained(),
                  "a changed corroborating timing binding withholds native rollback and retains the same generation owner");
            fixture.Patch(NativeGameTimingProfile::scriptScaleRva, NativeGameTimingProfile::scriptScale.data(), NativeGameTimingProfile::scriptScale.size());
            FinishOwned(service, fixture);
            const auto restored = fixture.Get<float>(NativeGameTimingProfile::timeScaleRva);
            Check(restored == 0 && std::signbit(restored), "confirmed native restoration preserves original signed-zero bits");
        }
        std::puts("Production native CGame timing writer fixture completed");
    }

    struct PauseHandlerContext
    {
        unsigned    calls = 0, operation = 0;
        bool        paused = false;
        ClockResult result = ClockResult::NotInitialized;
    };

    void PauseHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto& context = *static_cast<PauseHandlerContext*>(opaque);
        ++context.calls;
        Check(callbackDepth == 1 && timingTimerScope && timingCallbackGeneration == runtime.generation.load() && runtime.timerReady.load() &&
                  runtime.engineThread.load() == GetCurrentThreadId(),
              "the real post-original timer handler receives only a qualified same-generation native owner");
        if (context.operation == 1)
            context.result = access.Pause(context.paused);
        else if (context.operation == 2)
            context.result = access.SetTimeScale(1.5f);
        else if (context.operation == 3)
            context.result = access.SetMinuteDuration(2000);
    }

    ContinuationFunction countedTimer = nullptr;
    unsigned             originalTimerInvocations = 0;
    void                 CountOriginalTimer()
    {
        ++originalTimerInvocations;
        countedTimer();
    }

    bool InvokeTimerOriginalException()
    {
        // The deliberate native exception crosses compiler-owned frames only;
        // no fabricated unwind metadata is assigned to synthetic game callers.
        __try
        {
            HookTimerUpdate();
        }
        __except (GetExceptionCode() == 0xe001c10c ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }

    void FinishTimingOwned(NativeClockService& service, ClockImageFixture& fixture)
    {
        const auto handler = runtime.timingHandler;
        const auto context = runtime.timingContext;
        Check(!service.End() && service.IsRetained() && runtime.timerHooked.load() && runtime.timingHandler == handler && runtime.timingContext == context,
              "End retains both actual hooks, callback context and pause originals while native cleanup is pending");
        fixture.invokeTimer();
        Check(service.End() && !service.IsRetained() && !runtime.timerHooked.load() && !runtime.hooked.load() && !runtime.timerReady.load() &&
                  !runtime.timingHandler && !runtime.timingContext && !runtime.inFlight.load() && !runtime.timingDirty.load() &&
                  !runtime.timingRecovery.load() && !runtime.timingCaptured.load() && !runtime.timingGeneration.load() && !timingCallbackGeneration &&
                  !timingTimerScope && !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && CheckBinding(true),
              "the pause-safe native callback restores all debt before both real Detours hooks and handler context retire");
    }

    void NativePauseSafeTimingOwner(ClockImageFixture& fixture)
    {
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            Check(runtime.access.Pause(true) == ClockResult::NotInitialized && !runtime.timingDirty.load() && !runtime.timerHooked.load(),
                  "legacy V1 clock-only admission cannot acquire pause debt without its recovery callback");
            FinishOwned(service, fixture);
        }
        for (bool initialPause : {false, true})
        {
            NativeClockService  service;
            PauseHandlerContext context;
            context.operation = 1;
            context.paused = !initialPause;
            BeginOwned(service, fixture, false, true, true, &PauseHandler, &context);
            fixture.Put(NativeGamePauseProfile::userPauseRva, static_cast<std::uint8_t>(initialPause));
            countedTimer = runtime.originalTimer;
            runtime.originalTimer = &CountOriginalTimer;
            originalTimerInvocations = 0;
            HookTimerUpdate();
            Check(originalTimerInvocations == 1 && !context.calls && !runtime.engineThread.load() && !runtime.timerReady.load(),
                  "unknown-origin timer invocations still execute their original update once without adopting or dispatching client work");
            fixture.invokeTimer();
            Check(originalTimerInvocations == 2 && context.calls == 1 && context.result == ClockResult::Ok && runtime.timerReady.load() &&
                      !runtime.captured.load() && !State(service).ready && runtime.originalUserPause == static_cast<std::uint8_t>(initialPause) &&
                      fixture.Get<std::uint8_t>(NativeGamePauseProfile::userPauseRva) == static_cast<std::uint8_t>(!initialPause),
                  "a real normal-origin timer callback can change either initially paused or unpaused state without inventing clock readiness");
            Check(!service.Submit(Command(State(service), 1)) && !runtime.pendingSequence.load(),
                  "timer ownership alone does not admit CClock mailbox commands before its real postclock capture");
            context.operation = 0;
            fixture.invokeAlternateTimer();
            Check(originalTimerInvocations == 3 && context.calls == 2 && !runtime.captured.load(),
                  "the independently proved alternate gameplay tail return dispatches the same owner without losing its original update");
            context.operation = 1;
            context.paused = initialPause;
            fixture.invokeTimer();
            Check(
                context.result == ClockResult::Ok && fixture.Get<std::uint8_t>(NativeGamePauseProfile::userPauseRva) == static_cast<std::uint8_t>(initialPause),
                "the actual native handler can reverse pause without waiting for the skipped weather continuation");
            context.paused = !initialPause;
            fixture.invokeTimer();
            context.operation = 2;
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok, "paused timer scope shares the actual time-scale writer without clock-ready masquerading");
            context.operation = 3;
            fixture.invokeTimer();
            Check(context.result == ClockResult::Ok && runtime.timingDirty.load() == (timingPauseBit | timingScaleBit | timingMinuteBit) &&
                      !runtime.timingRecovery.load() && !runtime.nativeTouched.load() && !runtime.captured.load(),
                  "pause, scale and duration share one restoration owner while genuine CClock state remains uncaptured");
            runtime.originalTimer = countedTimer;
            fixture.Put(NativeClockProfile::hoursRva, std::uint8_t{14});
            fixture.Put(NativeClockProfile::minutesRva, std::uint8_t{45});
            fixture.Put(NativeClockProfile::secondsRva, std::uint16_t{10});
            fixture.Put(NativeClockProfile::nativeTimerRva, std::uint32_t{20000});
            fixture.Put(NativeClockProfile::lastChangeRva, std::uint32_t{19950});
            const auto calls = context.calls;
            FinishTimingOwned(service, fixture);
            Check(context.calls == calls && fixture.Get<std::uint8_t>(NativeGamePauseProfile::userPauseRva) == static_cast<std::uint8_t>(initialPause) &&
                      fixture.Get<float>(NativeGameTimingProfile::timeScaleRva) == 1 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::minuteDurationRva) == 1000 &&
                      fixture.Get<std::uint8_t>(NativeClockProfile::hoursRva) == 14 && fixture.Get<std::uint8_t>(NativeClockProfile::minutesRva) == 45 &&
                      fixture.Get<std::uint16_t>(NativeClockProfile::secondsRva) == 10 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::nativeTimerRva) == 20000 &&
                      fixture.Get<std::uint32_t>(NativeClockProfile::lastChangeRva) == 19950,
                  "stopped handlers never run again and timer-only cleanup restores original pause/timing without rewinding naturally progressed clock fields");
        }
        for (unsigned failure = 0; failure != 4; ++failure)
        {
            NativeClockService  service;
            PauseHandlerContext context;
            context.operation = 1;
            context.paused = true;
            BeginOwned(service, fixture, false, true, true, &PauseHandler, &context);
            fixture.invokeTimer();
            const auto calls = context.calls;
            if (failure == 0)
            {
                ClockTestFaults::timingWriteAddress = fixture.image + NativeGamePauseProfile::userPauseRva;
                ClockTestFaults::failTimingWrites.store(100);
                Check(!service.End(), "pause rollback first waits for the same actual native owner");
                fixture.invokeTimer();
                Check(!State(service).restored && State(service).dataRecoveryPending && runtime.timingRecovery.load() && service.IsRetained(),
                      "a real failed pause-byte rollback retains both hooks and context rather than reporting a restored runtime");
                ClockTestFaults::failTimingWrites.store(0);
                ClockTestFaults::timingWriteAddress = 0;
            }
            if (failure == 1)
            {
                runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
                fixture.invokeTimer();
                Check(State(service).failure == ClockResult::Expired && !runtime.timingDirty.load() && context.calls == calls,
                      "an expired paused owner restores from the live timer callback without dispatching further client work");
            }
            if (failure == 2)
            {
                std::thread foreign([&] { fixture.invokeAlternateTimer(); });
                foreign.join();
                Check(State(service).failure == ClockResult::WrongThread && context.calls == calls && runtime.timingDirty.load(),
                      "a foreign natural timer origin cannot take over the existing engine owner or discard its pause snapshot");
            }
            if (failure == 3)
            {
                const auto original = runtime.originalTimer;
                runtime.originalTimer = &OriginalRaises;
                const bool caught = InvokeTimerOriginalException();
                runtime.originalTimer = original;
                Check(caught && State(service).failure == ClockResult::OriginalException && !runtime.inFlight.load() && !callbackDepth &&
                          !timingCallbackGeneration && !timingTimerScope && runtime.timingDirty.load() && context.calls == calls,
                      "native timer exceptions propagate while actual hook finally releases callback scope and retains pause recovery");
            }
            if (!runtime.timingDirty.load())
            {
                Check(service.End() && !service.IsRetained() && !runtime.timingHandler && !runtime.timingContext,
                      "a lease-stop callback that already restored pause can retire both hooks without another mutation");
            }
            else
                FinishTimingOwned(service, fixture);
            Check(fixture.Get<std::uint8_t>(NativeGamePauseProfile::userPauseRva) == 0 && context.calls == calls,
                  "all failed or stopped pause owners preserve the original byte and never resume handler dispatch");
        }
        std::puts("Production pause-safe native timing owner fixture completed");
    }

    struct ApplicationFrameContext
    {
        unsigned               calls = 0, originalCountAtLastHandler = 0;
        NativeApplicationFrame last;
        NativeClockService*    service = nullptr;
        bool                   endInHandler = false, endResult = true, throwInHandler = false;
        unsigned               uiMode = 0;
        ClockImageFixture*     fixture = nullptr;
    };

    ApplicationFrameFunction countedFrame = nullptr;
    unsigned                 originalFrameInvocations = 0;
    void*                    lastFrameReceiver = nullptr;
    float                    lastFrameDelta = 0;
    HANDLE                   frameOriginalEntered = nullptr, frameOriginalRelease = nullptr;
    ClockImageFixture*       mutateFrameRoot = nullptr;
    NativeClockService*      originalUiService = nullptr;
    unsigned                 originalUiRefusals = 0;

    void CountOriginalFrame(void* receiver, float delta)
    {
        ++originalFrameInvocations;
        lastFrameReceiver = receiver;
        lastFrameDelta = delta;
        if (originalUiService)
        {
            NativeUiClassMapAccess::Value value{0xa5a5, true};
            Check(!frameCallbackGeneration && !frameCallbackReceiver &&
                      !originalUiService->ReadStartupUiClassMap(runtime.generation.load(), NativeUiClassMapAccess::Key::Loading, value) &&
                      value.classAddress == 0xa5a5 && value.keyPresent,
                  "even the engine owner cannot read the UI map during the original frame or an unqualified origin");
            ++originalUiRefusals;
        }
        if (frameOriginalEntered)
        {
            SetEvent(frameOriginalEntered);
            Check(WaitForSingleObject(frameOriginalRelease, 5000) == WAIT_OBJECT_0,
                  "release the owned original application callback within a bounded fixture wait");
        }
        countedFrame(receiver, delta);
        if (mutateFrameRoot)
            mutateFrameRoot->Put(NativeApplicationFrameProfile::rootRva, std::uintptr_t{0});
    }

    void ObserveApplicationFrame(const NativeApplicationFrame& frame, void* opaque)
    {
        auto& context = *static_cast<ApplicationFrameContext*>(opaque);
        ++context.calls;
        context.originalCountAtLastHandler = originalFrameInvocations;
        context.last = frame;
        Check(callbackDepth == 1 && runtime.inFlight.load() == 1 && runtime.engineThread.load() == GetCurrentThreadId() &&
                  frame.generation == runtime.generation.load() && !timingCallbackGeneration && !timingTimerScope,
              "only an outermost real application callback observes raw state after original execution without timing-writer scope");
        BYTE       hour = 0xa5, minute = 0x5a;
        const auto beforeWrites = ClockTestFaults::clockDataWrites.load();
        Check(runtime.clock.TryGet(&hour, &minute) == ClockResult::NotInitialized && hour == 0xa5 && minute == 0x5a &&
                  runtime.clock.TrySet(1, 2) == ClockResult::NotInitialized && runtime.access.SetTimeScale(2) == ClockResult::NotInitialized &&
                  runtime.access.SetMinuteDuration(2000) == ClockResult::NotInitialized && runtime.access.Pause(true) == ClockResult::NotInitialized &&
                  ClockTestFaults::clockDataWrites.load() == beforeWrites && !runtime.captured.load() && !runtime.timingCaptured.load() &&
                  !runtime.timingDirty.load() && !runtime.patchInstalled.load(),
              "observer readiness cannot capture CClock, enqueue its writes or borrow any native timing/pause capability");
        if (context.uiMode)
        {
            namespace Ui = NativeUiClassMapProfile;
            using Key = NativeUiClassMapAccess::Key;
            if (context.uiMode == 10)
            {
                // Each object and registry item is owned synthetic memory. The
                // test exercises real callback/page/profile guards without a
                // GTA process, UObject constructor, GC claim or native call.
                namespace Owner = NativeStartupOwnerProfile;
                namespace World = NativeStartupWorldProfile;
                alignas(8) std::array<unsigned char, 0x40>        uiBase{}, gameBase{}, levelBase{}, uiClass{}, gameClass{}, levelClass{}, world{};
                alignas(8) std::array<unsigned char, 0x2c8>       game{};
                alignas(8) std::array<unsigned char, 0xd8>        level{};
                std::array<std::uintptr_t, 3>                     uiChain{}, gameChain{}, levelChain{};
                std::array<unsigned char, World::objectItemBytes> item{};
                const auto                                        address = [](const auto& value) { return reinterpret_cast<std::uintptr_t>(value.data()); };
                const auto put = [](auto& target, std::size_t offset, const auto& value) { std::memcpy(target.data() + offset, &value, sizeof(value)); };
                const auto type = [&](auto& base, auto& candidate, auto& chain)
                {
                    put(base, Ui::classChainIndexOffset, std::int32_t{1});
                    chain[1] = address(base) + Ui::classChainOffset;
                    put(candidate, Ui::classChainOffset, address(chain));
                    put(candidate, Ui::classChainIndexOffset, std::int32_t{2});
                };
                type(uiBase, uiClass, uiChain);
                type(gameBase, gameClass, gameChain);
                type(levelBase, levelClass, levelChain);
                const auto root = reinterpret_cast<std::uintptr_t>(context.fixture->frameOwner.data());
                const auto gameAddress = address(game), levelAddress = address(level);
                std::memcpy(reinterpret_cast<unsigned char*>(root) + Owner::objectOuterOffset, &gameAddress, sizeof(gameAddress));
                put(game, 0, context.fixture->image + Owner::gameModeVtableRva);
                put(game, Owner::objectClassOffset, address(gameClass));
                put(game, Owner::objectOuterOffset, levelAddress);
                put(game, Owner::gameModeInterfaceOffset, root);
                put(level, World::objectClassOffset, address(levelClass));
                put(level, World::levelWorldOffset, address(world));
                put(item, World::itemObjectOffset, levelAddress);
                std::array<std::uintptr_t, 1> chunks{address(item)};
                context.fixture->Put(Ui::uiTransitionClassCacheRva, address(uiBase));
                context.fixture->Put(Owner::gameModeClassCacheRva, address(gameBase));
                context.fixture->Put(World::levelClassCacheRva, address(levelBase));
                context.fixture->Put(World::objectChunksRva, address(chunks));
                context.fixture->Put(World::objectCountRva, std::int32_t{1});
                put(context.fixture->uiEntries, Ui::valueOffset, address(uiClass));
                put(context.fixture->uiEntries, Ui::entryBytes + Ui::valueOffset, address(uiClass));
                std::uint32_t facts = 0xa5, refused = 0x5a;
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 63 && !refused,
                      "the actual native callback observes typed UI classes, enclosing owner and conservative world link without invoking providers");
                uiChain[1] = address(uiBase);
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 53 && !refused,
                      "stable unrelated UI classes remain observed negatives rather than read refusals");
                uiChain[1] = address(uiBase) + Ui::classChainOffset;
                put(level, World::levelWorldOffset, std::uintptr_t{0});
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 31 && refused == StartupWorldLinkRefused,
                      "a missing world preserves independent UI and owner observations while refusing only the world prerequisite");
                put(level, World::levelWorldOffset, address(world));
                put(game, Owner::gameModeInterfaceOffset, std::uintptr_t{0});
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 15 && refused == 12,
                      "a broken enclosing-owner correlation refuses both owner-dependent probes without hiding independent UI facts");
                put(game, Owner::gameModeInterfaceOffset, root);
                context.fixture->Put(Ui::uiTransitionClassCacheRva, std::uintptr_t{0});
                Check(context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 48 && refused == 3,
                      "a missing UI class cache is a refusal and cannot run native StaticClass initialization");
                context.fixture->Put(Ui::uiTransitionClassCacheRva, address(uiBase));
                facts = 0xa5;
                refused = 0x5a;
                Check(!context.service->ReadStartupPrerequisites(frame.generation + 1, facts, refused) && facts == 0xa5 && refused == 0x5a,
                      "a different generation cannot borrow startup observations or modify either caller output");
                bool        foreign = true;
                std::thread other([&] { foreign = context.service->ReadStartupPrerequisites(frame.generation, facts, refused); });
                other.join();
                Check(!foreign && facts == 0xa5 && refused == 0x5a, "a foreign thread cannot borrow the native startup callback scope");
                const auto& pin = Owner::pins[0];
                const auto  changed = static_cast<unsigned char>(pin.bytes[0] ^ 1);
                context.fixture->Patch(pin.rva, &changed, sizeof(changed));
                Check(!context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 0xa5 && refused == 0x5a,
                      "changed owner proof refuses the whole sample before publishing partial facts");
                context.fixture->Patch(pin.rva, pin.bytes, pin.size);
                ClockTestFaults::uiReadAddress = reinterpret_cast<std::uintptr_t>(context.fixture->frameOwner.data()) + Ui::ownerOffset;
                ClockTestFaults::uiRootChangeAddress = context.fixture->image + NativeApplicationFrameProfile::rootRva;
                Check(!context.service->ReadStartupPrerequisites(frame.generation, facts, refused) && facts == 0xa5 && refused == 0x5a,
                      "root movement during an independent child probe cannot publish a mixed startup sample");
                ClockTestFaults::uiReadAddress = ClockTestFaults::uiRootChangeAddress = 0;
                context.fixture->SetFrameReceiver(context.fixture->frameOwner.data());
                context.fixture->Put(Ui::uiTransitionClassCacheRva, std::uintptr_t{0});
                context.fixture->Put(Owner::gameModeClassCacheRva, std::uintptr_t{0});
                context.fixture->Put(World::levelClassCacheRva, std::uintptr_t{0});
                // Do not leave the fixture root pointing at expired local records.
                const std::uintptr_t absent = 0;
                std::memcpy(reinterpret_cast<unsigned char*>(root) + Owner::objectOuterOffset, &absent, sizeof(absent));
                return;
            }
            if (context.uiMode == 9)
            {
                // The image and these class records belong only to this host.
                // Exercise the production callback gate as well as native
                // embedded-chain tokens without constructing any UObject.
                alignas(8) std::array<unsigned char, 0x40> base{}, candidate{};
                std::array<std::uintptr_t, 3>              chain{};
                const auto put = [](unsigned char* target, std::size_t offset, const auto& field) { std::memcpy(target + offset, &field, sizeof(field)); };
                const auto baseAddress = reinterpret_cast<std::uintptr_t>(base.data());
                const auto candidateAddress = reinterpret_cast<std::uintptr_t>(candidate.data());
                chain[1] = baseAddress + Ui::classChainOffset;
                put(base.data(), Ui::classChainIndexOffset, std::int32_t{1});
                put(candidate.data(), Ui::classChainOffset, reinterpret_cast<std::uintptr_t>(chain.data()));
                put(candidate.data(), Ui::classChainIndexOffset, std::int32_t{2});
                put(context.fixture->uiEntries.data(), Ui::valueOffset, candidateAddress);
                put(context.fixture->uiEntries.data() + Ui::entryBytes, Ui::valueOffset, candidateAddress);
                bool compatible = true;
                context.fixture->Put(Ui::uiTransitionClassCacheRva, std::uintptr_t{0});
                Check(!context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible) && compatible,
                      "an uninitialized class cache cannot call StaticClass or invent a negative type result");
                context.fixture->Put(Ui::uiTransitionClassCacheRva, baseAddress);
                Check(context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible) && compatible &&
                          context.service->ReadStartupUiTransitionClass(frame.generation, Key::SimpleFade, compatible) && compatible,
                      "both configured classes use the actual native embedded base-chain relation on the qualified callback");
                Check(!context.service->ReadStartupUiTransitionClass(frame.generation + 1, Key::Loading, compatible) && compatible,
                      "type observations cannot borrow a different generation");
                bool        foreign = true;
                std::thread other([&] { foreign = context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible); });
                other.join();
                Check(!foreign && compatible, "type observations cannot borrow the frame scope from another thread");
                chain[1] = baseAddress;
                Check(context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible) && !compatible,
                      "a stable unrelated class is distinct from a failed observation and never matches the class pointer itself");
                compatible = true;
                put(context.fixture->uiEntries.data(), Ui::valueOffset, std::uintptr_t{0});
                Check(!context.service->ReadStartupUiTransitionClass(frame.generation, Key::Loading, compatible) && compatible,
                      "a present dictionary key with no class preserves type output instead of becoming a negative relation");
                context.fixture->Put(Ui::uiTransitionClassCacheRva, std::uintptr_t{0});
                return;
            }
            NativeUiClassMapAccess::Value value{0xa5a5, true};
            Check(frameCallbackGeneration == frame.generation && frameCallbackReceiver == context.fixture->frameOwner.data(),
                  "only the qualified post-original handler holds this receiver and generation for dictionary observations");
            const bool observed = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value);
            if (context.uiMode == 1 || context.uiMode == 7 || context.uiMode == 8)
            {
                Check(observed && value.keyPresent && value.classAddress == 0x12345678,
                      "the natural frame handler observes Loading as an opaque configured class without dereferencing it");
                Check(context.service->ReadStartupUiClassMap(frame.generation, Key::SimpleFade, value) && value.keyPresent && value.classAddress == 0x87654321,
                      "the qualified owner follows the same bounded dictionary chain for SimpleFade");
                value = {0xa5a5, true};
                Check(!context.service->ReadStartupUiClassMap(frame.generation + 1, Key::Loading, value) && value.classAddress == 0xa5a5 && value.keyPresent,
                      "a stale requested generation cannot borrow the current frame scope or change output");
                ++callbackDepth;
                const bool nested = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value);
                --callbackDepth;
                runtime.stopping.store(true);
                const bool stopping = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value);
                runtime.stopping.store(false);
                Check(!nested && !stopping && value.classAddress == 0xa5a5 && value.keyPresent,
                      "nested or stopping callbacks preserve the caller value instead of admitting dictionary access");
                const auto refreshed = runtime.refreshed.load();
                runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
                const bool expired = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value);
                runtime.refreshed.store(refreshed);
                Check(!expired && value.classAddress == 0xa5a5 && value.keyPresent,
                      "an expired lease cannot borrow an already-entered handler's dictionary scope");
                bool        foreign = true;
                std::thread other([&] { foreign = context.service->ReadStartupUiClassMap(frame.generation, Key::Loading, value); });
                other.join();
                Check(!foreign && value.classAddress == 0xa5a5 && value.keyPresent, "another thread cannot borrow a live handler's receiver/generation scope");
                NativeClockService unrelated;
                Check(!unrelated.ReadStartupUiClassMap(frame.generation, Key::Loading, value) && value.classAddress == 0xa5a5 && value.keyPresent,
                      "a separate service object cannot borrow the current owner's dictionary generation");
                if (context.uiMode == 7)
                    OriginalRaises();
            }
            else if (context.uiMode == 2)
                Check(observed && !value.keyPresent && !value.classAddress, "a valid absent UI dictionary key is reported separately from read refusal");
            else if (context.uiMode == 3)
                Check(observed && value.keyPresent && !value.classAddress, "a matching key with null class identity remains distinct from absent membership");
            else
                Check(!observed && value.classAddress == 0xa5a5 && value.keyPresent,
                      "failed OS reads, root movement or changed dictionary pins preserve the complete caller value");
        }
        if (context.endInHandler)
            context.endResult = context.service->End();
        if (context.throwInHandler)
            throw std::runtime_error("owned observer failure");
    }

    void BeginApplicationOwned(NativeClockService& service, ClockImageFixture& fixture, ApplicationFrameContext& context, bool attachNow = true)
    {
        ClockServiceStateV1 rejected;
        Check(!service.BeginApplicationFrames({}, rejected, &ObserveApplicationFrame, &context) && rejected.failure == ClockResult::UnsupportedBuild &&
                  !rejected.bound && !rejected.hooked && !rejected.modulePinned && rejected.generation &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
              "internal application Begin preserves the shipping wrong-executable refusal before any native hook or module pin");
        NativeServiceAdmission::Token claim;
        Check(NativeServiceAdmission::TryAcquire(NativeServiceAdmission::Category::Clock, claim),
              "application fixture owns the same exclusive setup/cleanup admission as other native transactions");
        runtime.admissionSerial.store(claim.serial);
        runtime.admissionGeneration.store(runtime.generation.load());
        runtime.image = fixture.image;
        runtime.failure.store(ClockResult::Ok);
        runtime.stopping.store(false);
        runtime.bound.store(true);
        runtime.active.store(true);
        runtime.started = GetTickCount64();
        runtime.refreshed.store(runtime.started);
        runtime.frameHandler = &ObserveApplicationFrame;
        runtime.frameContext = &context;
        runtime.originalFrame = reinterpret_cast<ApplicationFrameFunction>(fixture.image + NativeApplicationFrameProfile::entryRva);
        fixture.frameOwner[0] = fixture.image + NativeApplicationFrameProfile::vtableRva;
        fixture.SetFrameReceiver(fixture.frameOwner.data());
        fixture.Put(NativeApplicationFrameProfile::stateRva, std::uint32_t{7});
        context.service = &service;
        originalUiService = context.uiMode ? &service : nullptr;
        originalFrameInvocations = 0;
        lastFrameReceiver = nullptr;
        lastFrameDelta = 0;
        Check(runtime.frameMode && !runtime.timingMode && !runtime.timerHooked.load() && !runtime.hooked.load() && !runtime.ready.load() &&
                  !runtime.captured.load() && CheckBinding(true),
              "application-only admission validates its own exact pins without installing clock/timer hooks or capturing CClock");
        if (!attachNow)
            return;
        Check(ChangeHooks(true) && runtime.frameHooked.load() && !runtime.hooked.load() && !runtime.timerHooked.load(),
              "real Detours installs only the native application entry for the observer mode");
        countedFrame = runtime.originalFrame;
        runtime.originalFrame = &CountOriginalFrame;
    }

    void FinishApplicationOwned(NativeClockService& service)
    {
        runtime.originalFrame = countedFrame;
        Check(service.End() && !service.IsRetained() && !State(service).hooked && !runtime.frameHooked.load() && !runtime.hooked.load() &&
                  !runtime.timerHooked.load() && !runtime.frameHandler && !runtime.frameContext && !runtime.inFlight.load() && !runtime.captured.load() &&
                  !runtime.timingCaptured.load() && !runtime.timingDirty.load() && !runtime.timingRecovery.load() && !timingCallbackGeneration &&
                  !timingTimerScope && !frameCallbackGeneration && !frameCallbackReceiver &&
                  !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock) && CheckBinding(true),
              "an observer retires without a future clock tick after actual hook detach, callback completion and admission release");
        originalUiService = nullptr;
    }

    void FrameOriginalRaises(void* receiver, float delta)
    {
        ++originalFrameInvocations;
        lastFrameReceiver = receiver;
        lastFrameDelta = delta;
        OriginalRaises();
    }

    bool InvokeFrameOriginalException(void* receiver)
    {
        // Deliberate exceptions cross compiler-owned frames only, as the
        // synthetic native caller intentionally has no unwind registration.
        __try
        {
            HookApplicationFrame(receiver, 0.125f);
        }
        __except (GetExceptionCode() == 0xe001c10c ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }

    bool InvokeQualifiedHandlerException(void* receiver)
    {
        // Exercise the production handler's SEH cleanup using compiler-owned
        // frames; the synthetic caller intentionally has no invented unwind data.
        __try
        {
            FrameBody(receiver, 0.125f, reinterpret_cast<void*>(runtime.image + NativeApplicationFrameProfile::normalReturnRva), true);
        }
        __except (GetExceptionCode() == 0xe001c10c ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }

    void NativeStartupUiClassMapOwner(ClockImageFixture& fixture)
    {
        namespace Ui = NativeUiClassMapProfile;
        using Key = NativeUiClassMapAccess::Key;
        fixture.AdmitUiClassMapPins();
        fixture.AdmitStartupPins();
        for (unsigned mode = 1; mode != 11; ++mode)
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            context.uiMode = mode;
            context.fixture = &fixture;
            context.throwInHandler = mode == 8;
            fixture.SeedUiClassMap();
            BeginApplicationOwned(service, fixture, context);
            NativeUiClassMapAccess::Value outside{0xa5a5, true};
            Check(!service.ReadStartupUiClassMap(runtime.generation.load(), Key::Loading, outside) && outside.classAddress == 0xa5a5 && outside.keyPresent,
                  "producer-side lookup before a frame is refused even with a bound observer and the current generation");
            bool typeOutside = true;
            Check(!service.ReadStartupUiTransitionClass(runtime.generation.load(), Key::Loading, typeOutside) && typeOutside,
                  "producer-side class typing cannot acquire an engine callback scope");
            std::uint32_t startupFacts = 0xa5, startupRefusals = 0x5a;
            Check(
                !service.ReadStartupPrerequisites(runtime.generation.load(), startupFacts, startupRefusals) && startupFacts == 0xa5 && startupRefusals == 0x5a,
                "producer-side startup reads preserve both outputs outside the natural frame callback");
            if (mode == 1)
            {
                const auto before = originalUiRefusals;
                HookApplicationFrame(fixture.frameOwner.data(), 0.125f);
                fixture.invokeLoadingFrame(fixture.frameOwner.data(), 0.125f);
                Check(originalUiRefusals == before + 2 && !context.calls,
                      "unknown/loading origins forward once but cannot create a UI dictionary handler scope");
            }
            auto* map = reinterpret_cast<unsigned char*>(fixture.frameOwner.data()) + Ui::ownerOffset;
            if (mode == 2)
            {
                const std::int32_t missing = -1;
                std::memcpy(map + Ui::inlineHashOffset, &missing, sizeof(missing));
            }
            if (mode == 3)
            {
                const std::uintptr_t absent = 0;
                std::memcpy(fixture.uiEntries.data() + Ui::valueOffset, &absent, sizeof(absent));
            }
            ClockTestFaults::uiReadAddress = reinterpret_cast<std::uintptr_t>(map);
            if (mode == 4)
                ClockTestFaults::failUiRead.store(true);
            if (mode == 5)
                ClockTestFaults::uiRootChangeAddress = fixture.image + NativeApplicationFrameProfile::rootRva;
            const auto& proof = Ui::pins[0];
            const auto  changed = static_cast<unsigned char>(proof.bytes[0] ^ 1);
            if (mode == 6)
                fixture.Patch(proof.rva, &changed, sizeof(changed));
            if (mode == 7)
            {
                ++callbackDepth;
                runtime.inFlight.fetch_add(1);
                const bool caught = InvokeQualifiedHandlerException(fixture.frameOwner.data());
                runtime.inFlight.fetch_sub(1);
                --callbackDepth;
                Check(caught && !frameCallbackGeneration && !frameCallbackReceiver && State(service).failure == ClockResult::OriginalException,
                      "handler SEH retires both UI scope fields before propagating through compiler-owned frames");
            }
            else
                fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
            if (mode == 8)
                Check(State(service).failure == ClockResult::OriginalException && !runtime.active.load(),
                      "C++ handler failures stop the owner after retiring the generation and receiver scope");
            ClockTestFaults::uiReadAddress = ClockTestFaults::uiRootChangeAddress = 0;
            ClockTestFaults::failUiRead.store(false);
            if (mode == 6)
                fixture.Patch(proof.rva, proof.bytes, proof.size);
            fixture.SetFrameReceiver(fixture.frameOwner.data());
            Check(context.calls == 1 && !frameCallbackGeneration && !frameCallbackReceiver &&
                      !service.ReadStartupUiClassMap(runtime.generation.load(), Key::Loading, outside) && outside.classAddress == 0xa5a5 && outside.keyPresent,
                  "normal/refused/exceptional handler completion cannot leak UI scope back to its producer");
            FinishApplicationOwned(service);
        }
        std::puts("Production qualified startup UI dictionary fixture completed");
    }

    void NativeApplicationFrameOwner(ClockImageFixture& fixture)
    {
        namespace Frame = NativeApplicationFrameProfile;
        fixture.AdmitApplicationPins();
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            const auto              unrelatedClockByte = static_cast<unsigned char>(NativeClockProfile::clockCode[0] ^ 1);
            fixture.Patch(NativeClockProfile::clockCodeRva, &unrelatedClockByte, sizeof(unrelatedClockByte));
            BeginApplicationOwned(service, fixture, context);
            HookApplicationFrame(fixture.frameOwner.data(), 0.125f);
            fixture.invokeLoadingFrame(fixture.frameOwner.data(), 0.25f);
            Check(originalFrameInvocations == 2 && !context.calls && !runtime.engineThread.load() && !runtime.ready.load(),
                  "unknown and actual excluded loading origins preserve original ABI calls exactly once without adopting the observer owner");
            const auto writes = ClockTestFaults::clockDataWrites.load();
            const auto exchanges = ClockTestFaults::clockCodeExchanges.load();
            for (const std::uint32_t state : {7u, 9u})
            {
                fixture.Put(Frame::stateRva, state);
                const float delta = state == 7 ? 0.03125f : -0.0f;
                const auto  before = originalFrameInvocations;
                fixture.invokeFrame(fixture.frameOwner.data(), delta);
                Check(originalFrameInvocations == before + 1 && context.originalCountAtLastHandler == before + 1 &&
                          lastFrameReceiver == fixture.frameOwner.data() && std::memcmp(&lastFrameDelta, &delta, sizeof(delta)) == 0 &&
                          std::memcmp(&context.last.deltaSeconds, &delta, sizeof(delta)) == 0 && context.last.nativeState == state && State(service).ready &&
                          runtime.restored.load() && !runtime.captured.load() && !runtime.timerReady.load(),
                      "the exact pinned normal call preserves this and XMM1 bitwise and observes frontend/gameplay state only after its original once");
            }
            Check(context.calls == 2 && !service.Submit(Command(State(service), 1)) && !runtime.pendingSequence.load() && !runtime.hasPending &&
                      ClockTestFaults::clockDataWrites.load() == writes && ClockTestFaults::clockCodeExchanges.load() == exchanges,
                  "application observer dispatch never admits the public clock mailbox or changes native clock/timing data");
            fixture.frameOwner[0] += sizeof(std::uintptr_t);
            Check(!FrameReceiver(fixture.frameOwner.data()), "typed receiver qualification rejects another vtable without dereferencing any UObject child");
            fixture.frameOwner[0] -= sizeof(std::uintptr_t);
            FinishApplicationOwned(service);
            fixture.Patch(NativeClockProfile::clockCodeRva, NativeClockProfile::clockCode.data(), NativeClockProfile::clockCode.size());
        }
        for (unsigned refusal = 0; refusal != 7; ++refusal)
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context);
            std::array<std::uintptr_t, 2> differentReceiver{fixture.frameOwner[0], 0};
            void*                         receiver = fixture.frameOwner.data();
            float                         delta = 0.0625f;
            if (refusal == 0)
                receiver = differentReceiver.data();
            if (refusal == 1)
                mutateFrameRoot = &fixture;
            if (refusal == 2)
                delta = (std::numeric_limits<float>::quiet_NaN)();
            if (refusal == 3)
                fixture.Put(Frame::stateRva, std::uint32_t{10});
            if (refusal == 4)
                runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
            if (refusal == 5)
                ++callbackDepth;
            const auto changed = static_cast<unsigned char>(Frame::rootPublication[0] ^ 1);
            if (refusal == 6)
                fixture.Patch(Frame::rootPublicationRva, &changed, sizeof(changed));
            fixture.invokeFrame(receiver, delta);
            if (refusal == 5)
                --callbackDepth;
            if (refusal == 6)
                fixture.Patch(Frame::rootPublicationRva, Frame::rootPublication.data(), Frame::rootPublication.size());
            mutateFrameRoot = nullptr;
            fixture.SetFrameReceiver(fixture.frameOwner.data());
            const auto failure = refusal == 4 ? ClockResult::Expired : refusal == 5 ? ClockResult::Busy : ClockResult::BindingChanged;
            Check(originalFrameInvocations == 1 && !context.calls && !runtime.ready.load() && State(service).failure == failure && !runtime.inFlight.load() &&
                      !callbackDepth && !timingCallbackGeneration && !timingTimerScope && !runtime.captured.load(),
                  "receiver mismatch/bookend, nonfinite delta, invalid raw state, expired lease, nesting or changed exact pins refuse after original once");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
            std::thread foreign([&] { fixture.invokeFrame(fixture.frameOwner.data(), 0.25f); });
            foreign.join();
            Check(originalFrameInvocations == 2 && context.calls == 1 && State(service).failure == ClockResult::WrongThread &&
                      State(service).foreignCallbacks == 1 && !runtime.inFlight.load(),
                  "a foreign normal-origin callback preserves its original but cannot take over the established engine thread");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            context.endInHandler = true;
            BeginApplicationOwned(service, fixture, context);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
            Check(context.calls == 1 && !context.endResult && service.IsRetained() && runtime.frameHooked.load() && !runtime.inFlight.load(),
                  "End inside an observer cannot discard its own live trampoline or handler context");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            context.throwInHandler = true;
            BeginApplicationOwned(service, fixture, context);
            fixture.invokeFrame(fixture.frameOwner.data(), 0.125f);
            Check(context.calls == 1 && originalFrameInvocations == 1 && State(service).failure == ClockResult::OriginalException && !runtime.active.load() &&
                      !runtime.inFlight.load() && !callbackDepth,
                  "client observer exceptions stop dispatch without unwinding into the native caller or leaking callback accounting");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context);
            runtime.originalFrame = &FrameOriginalRaises;
            const bool caught = InvokeFrameOriginalException(fixture.frameOwner.data());
            runtime.originalFrame = &CountOriginalFrame;
            Check(caught && originalFrameInvocations == 1 && !context.calls && State(service).failure == ClockResult::OriginalException &&
                      !runtime.inFlight.load() && !callbackDepth && !timingCallbackGeneration && !timingTimerScope && service.IsRetained(),
                  "original native exceptions propagate through compiler-owned SEH while finally retains hook ownership and releases scopes");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context);
            frameOriginalEntered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            frameOriginalRelease = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            Check(frameOriginalEntered && frameOriginalRelease, "create bounded application in-flight lifetime events");
            std::thread engine([&] { fixture.invokeFrame(fixture.frameOwner.data(), 0.125f); });
            Check(WaitForSingleObject(frameOriginalEntered, 5000) == WAIT_OBJECT_0 && runtime.inFlight.load() == 1 && !service.End() && service.IsRetained() &&
                      runtime.frameHooked.load() && runtime.frameHandler == &ObserveApplicationFrame,
                  "producer End retains the actual trampoline/context while another thread is inside the original native application call");
            SetEvent(frameOriginalRelease);
            engine.join();
            CloseHandle(frameOriginalEntered);
            CloseHandle(frameOriginalRelease);
            frameOriginalEntered = frameOriginalRelease = nullptr;
            Check(!context.calls && !runtime.inFlight.load(), "a stopped in-flight original finishes once without dispatching late client observation");
            FinishApplicationOwned(service);
        }
        {
            NativeClockService      service;
            ApplicationFrameContext context;
            BeginApplicationOwned(service, fixture, context, false);
            const auto stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            Check(stop != nullptr, "create the owned observer-transaction peer lifetime");
            std::atomic<DWORD> peerId{0};
            std::thread        peer(
                [&]
                {
                    peerId.store(GetCurrentThreadId());
                    WaitForSingleObject(stop, INFINITE);
                });
            while (!peerId.load())
                SwitchToThread();
            ClockTestFaults::resumeTarget.store(peerId.load());
            ClockTestFaults::failResumes.store(64);
            Check(!ChangeHooks(true) && runtime.frameHooked.load() && State(service).retainedPeerThreads == 1 && service.IsRetained() && !service.End() &&
                      NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "committed application hook plus failed peer resume retains its actual hook and exclusive cleanup admission");
            ClockTestFaults::failResumes.store(0);
            countedFrame = runtime.originalFrame;
            FinishApplicationOwned(service);
            SetEvent(stop);
            peer.join();
            CloseHandle(stop);
        }
        NativeStartupUiClassMapOwner(fixture);
        std::puts("Production natural application-frame observer fixture completed");
    }

    class TimingBaseProbe : public CGameTimingDE
    {
    public:
        using CGameTimingDE::RequireFPS;
        using CGameTimingDE::RequireTiming;
    };
    static_assert(std::is_abstract_v<TimingBaseProbe> && std::is_base_of_v<CGame, TimingBaseProbe>);

    void NativeGameTimingPrerequisite(ClockImageFixture& fixture)
    {
        NativeGameTimingReader reader;
        NativeGameTimingValues values;
        std::memset(&values, 0x5a, sizeof(values));
        const auto untouched = values;
        const auto unchanged = [&] { return std::memcmp(&values, &untouched, sizeof(values)) == 0; };
        const auto refusesGetter = [&]
        {
            try
            {
                TimingBaseProbe::RequireTiming(reader);
                return false;
            }
            catch (const std::runtime_error& error)
            {
                return std::strstr(error.what(), "DE CGame timing unavailable") != nullptr;
            }
        };
        Check(reader.Read(values) == Status::NotInitialized && unchanged() && !reader.IsBound() && refusesGetter(),
              "the real CGame getter read boundary throws for unavailable timing instead of returning synthetic zero values");
        Check(reader.Bind() == Status::Unsupported && !reader.IsBound(), "native CGame timing preserves the shipping wrong-executable refusal");
        fixture.Seed();
        fixture.Put(NativeGameTimingProfile::frameCounterRva, std::uint32_t{0xfffffffe});
        fixture.Put(NativeGameTimingProfile::timeStepRva, 0.75f);
        fixture.Put(NativeGameTimingProfile::oldTimeStepRva, 0.625f);
        fixture.Put(NativeGameTimingProfile::timeScaleRva, 1.25f);
        ClockReaderTestHost::image = fixture.image;
        Check(reader.Bind() == Status::Ok && reader.IsBound(), "native timing binds genuine timer update/getter pins over the same owned native clock image");
        const auto writes = ClockTestFaults::clockDataWrites.load(), patches = ClockTestFaults::clockCodeExchanges.load();
        Check(reader.Read(values) == Status::Ok && values.clock.producerThread == GetCurrentThreadId() && values.clock.nativeTimer == 10000 &&
                  values.clock.millisecondsPerGameMinute == 1000 && values.frameCounter == 0xfffffffe && values.timeStep == 0.75f &&
                  values.oldTimeStep == 0.625f && values.timeScale == 1.25f,
              "matching complete samples preserve native counter bits and all independently corroborated timing values");
        fixture.Put(NativeGameTimingProfile::frameCounterRva, std::uint32_t{2});
        fixture.Put(NativeGameTimingProfile::timeStepRva, 0.5f);
        const auto fresh = TimingBaseProbe::RequireTiming(reader);
        Check(fresh.frameCounter == 2 && fresh.timeStep == 0.5f,
              "the actual CGame timing overrides' shared read path requests current native values instead of retaining a successful snapshot");
        std::thread foreign(
            [&]
            {
                NativeGameTimingValues output = untouched;
                Check(reader.Bind() == Status::WrongThread && reader.Read(output) == Status::WrongThread &&
                          std::memcmp(&output, &untouched, sizeof(output)) == 0 && refusesGetter(),
                      "foreign-thread native timing calls preserve output and the CGame getter boundary refuses access");
            });
        foreign.join();
        values = untouched;
        ClockReaderTestHost::timingSamples.store(0);
        ClockReaderTestHost::mismatchTimingSample.store(2);
        Check(reader.Read(values) == Status::Unsupported && unchanged() && ClockReaderTestHost::timingSamples.load() == 2,
              "a changed timing scalar between complete samples refuses the entire 64-byte native timing result");
        ClockReaderTestHost::mismatchTimingSample.store(0);
        for (const auto field : {NativeGameTimingProfile::timeStepRva, NativeGameTimingProfile::oldTimeStepRva, NativeGameTimingProfile::timeScaleRva})
        {
            fixture.Put(field, std::numeric_limits<float>::quiet_NaN());
            values = untouched;
            Check(reader.Read(values) == Status::Unsupported && unchanged() && refusesGetter(),
                  "each nonfinite native timing scalar refuses publication and cannot be hidden by the SDK getter boundary");
            fixture.Put(field, 1.0f);
        }
        fixture.Put(NativeGameTimingProfile::timeStepRva, 99.0f);
        fixture.Put(NativeGameTimingProfile::oldTimeStepRva, -1.0f);
        fixture.Put(NativeGameTimingProfile::timeScaleRva, -0.5f);
        Check(reader.Read(values) == Status::Ok && values.timeStep == 99.0f && values.oldTimeStep == -1.0f && values.timeScale == -0.5f,
              "finite native values are observed without inventing script-independent clamp or time-scale policies");
        auto changed = NativeGameTimingProfile::frameGetter;
        changed[0] ^= 1;
        fixture.Patch(NativeGameTimingProfile::frameGetterRva, changed.data(), changed.size());
        values = untouched;
        Check(reader.Read(values) == Status::Unsupported && unchanged() && refusesGetter(),
              "changed corroborating getter instructions revoke fresh timing reads after an earlier success");
        fixture.Patch(NativeGameTimingProfile::frameGetterRva, NativeGameTimingProfile::frameGetter.data(), NativeGameTimingProfile::frameGetter.size());
        DWORD previous = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameTimingProfile::oldTimeStepRva), sizeof(float), PAGE_NOACCESS, &previous) !=
                  FALSE,
              "deny native timing data only in the independently owned fixture image");
        values = untouched;
        Check(reader.Read(values) == Status::Unsupported && unchanged() && refusesGetter(),
              "a native timing page loss preserves output and never fabricates readable CGame state");
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameTimingProfile::oldTimeStepRva), sizeof(float), previous, &ignored) != FALSE,
              "restore the fixture timing page after verified refusal");
        Check(reader.Read(values) == Status::Ok && ClockTestFaults::clockDataWrites.load() == writes && ClockTestFaults::clockCodeExchanges.load() == patches,
              "native CGame timing reads and failure recovery neither write time nor install or renew engine hooks");
        reader.Reset();
        values = untouched;
        Check(!reader.IsBound() && reader.Read(values) == Status::NotInitialized && unchanged() && refusesGetter(),
              "timing retirement clears the reader rather than keeping earlier values available to CGame getters");
        ClockReaderTestHost::image = 0;
        std::puts("Production CGame native timing prerequisite fixture completed");
    }

    void NativeStoredFPSReader(ClockImageFixture& fixture)
    {
        NativeGameTimingReader reader;
        float                  value = 123.25f;
        const auto             refusesGetter = [&]
        {
            try
            {
                TimingBaseProbe::RequireFPS(reader);
                return false;
            }
            catch (const std::runtime_error& error)
            {
                return std::strstr(error.what(), "DE CGame FPS unavailable (status ") != nullptr;
            }
        };
        const auto refused = [&](Status expected)
        {
            value = 123.25f;
            return reader.ReadFPS(value) == expected && value == 123.25f;
        };
        Check(refused(Status::NotInitialized) && refusesGetter(), "unbound native FPS preserves output and the CGame getter explicitly refuses");
        Check(reader.Bind() == Status::Unsupported && refused(Status::NotInitialized), "native FPS cannot bypass the shipping executable gate");
        fixture.Seed();
        ClockReaderTestHost::image = fixture.image;
        Check(reader.Bind() == Status::Ok && reader.IsBound(), "FPS reuses the existing exact-image native timing binding and owner");
        const auto writes = ClockTestFaults::clockDataWrites.load(), patches = ClockTestFaults::clockCodeExchanges.load();
        ClockReaderTestHost::fpsSamples.store(0);
        Check(reader.ReadFPS(value) == Status::Ok && value == 62.5f && ClockReaderTestHost::fpsSamples.load() == 2 &&
                  TimingBaseProbe::RequireFPS(reader) == 62.5f,
              "the real CGame FPS getter boundary reads matching native FPS floats without constructing a partial CGame");
        fixture.Put(NativeGameFPSProfile::fpsRva, 37.25f);
        fixture.Put(NativeGameTimingProfile::timeStepRva, 99.0f);
        fixture.Put(NativeGameTimingProfile::timeScaleRva, -0.5f);
        Check(reader.ReadFPS(value) == Status::Ok && value == 37.25f,
              "FPS returns the fresh native field and cannot be replaced by a timestep or time-scale estimate");
        fixture.Put(NativeGameFPSProfile::fpsRva, 0.0f);
        Check(reader.ReadFPS(value) == Status::Ok && value == 0.0f, "a stored finite zero FPS remains a native value rather than an invented minimum");
        fixture.Put(NativeGameFPSProfile::fpsRva, 62.5f);
        std::thread foreign(
            [&] { Check(refused(Status::WrongThread) && refusesGetter(), "foreign FPS reads preserve output and the actual getter refusal policy"); });
        foreign.join();
        ClockReaderTestHost::fpsSamples.store(0);
        ClockReaderTestHost::mismatchFPSSample.store(2);
        Check(refused(Status::Unsupported) && ClockReaderTestHost::fpsSamples.load() == 2,
              "two unequal native FPS samples refuse publication rather than publishing a torn value");
        ClockReaderTestHost::mismatchFPSSample.store(0);
        for (unsigned failed = 1; failed <= 2; ++failed)
        {
            ClockReaderTestHost::fpsSamples.store(0);
            ClockReaderTestHost::failFPSSample.store(failed);
            Check(refused(Status::Unsupported), "failure of either native FPS read preserves the caller's output");
        }
        ClockReaderTestHost::failFPSSample.store(0);
        for (const auto invalid :
             {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -1.0f})
        {
            fixture.Put(NativeGameFPSProfile::fpsRva, invalid);
            Check(refused(Status::Unsupported) && refusesGetter(), "nonfinite or negative stored FPS is refused by both reader and CGame getter boundary");
        }
        fixture.Put(NativeGameFPSProfile::fpsRva, 62.5f);
        const auto changedPin = [&](std::uintptr_t rva, const auto& original)
        {
            auto changed = original;
            changed[0] ^= 1;
            fixture.Patch(rva, changed.data(), changed.size());
            Check(refused(Status::Unsupported) && refusesGetter(), "a changed FPS computation, scale or named consumer pin revokes an earlier successful read");
            fixture.Patch(rva, original.data(), original.size());
        };
        changedPin(NativeGameFPSProfile::frequencyScaleRva, NativeGameFPSProfile::frequencyScale);
        changedPin(NativeGameFPSProfile::computationRva, NativeGameFPSProfile::computation);
        changedPin(NativeGameFPSProfile::consumerRva, NativeGameFPSProfile::consumer);
        changedPin(NativeGameFPSProfile::labelRva, NativeGameFPSProfile::label);
        const auto changedConstant = [&](std::uintptr_t rva, const auto& original)
        {
            auto changed = original + 1;
            fixture.Patch(rva, &changed, sizeof(changed));
            Check(refused(Status::Unsupported), "changing a native FPS arithmetic constant refuses the stored value's meaning");
            fixture.Patch(rva, &original, sizeof(original));
        };
        changedConstant(NativeGameFPSProfile::previousWeightRva, NativeGameFPSProfile::previousWeight);
        changedConstant(NativeGameFPSProfile::currentWeightRva, NativeGameFPSProfile::currentWeight);
        changedConstant(NativeGameFPSProfile::fpsNumeratorRva, NativeGameFPSProfile::fpsNumerator);
        changedConstant(NativeGameFPSProfile::millisecondsRva, NativeGameFPSProfile::milliseconds);
        changedConstant(NativeGameFPSProfile::secondsOriginRva, NativeGameFPSProfile::secondsOrigin);
        changedConstant(NativeGameFPSProfile::frequencyNumeratorRva, NativeGameFPSProfile::frequencyNumerator);
        for (const auto protection : {PAGE_NOACCESS, PAGE_EXECUTE_READWRITE})
        {
            DWORD previous = 0, ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameFPSProfile::fpsRva), sizeof(float), protection, &previous) != FALSE,
                  "change only the owned fixture FPS data page's access policy");
            Check(refused(Status::Unsupported) && refusesGetter(),
                  "unreadable or executable native FPS data cannot satisfy writable non-executable qualification");
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameFPSProfile::fpsRva), sizeof(float), previous, &ignored) != FALSE,
                  "restore the owned FPS data page after checked refusal");
        }
        DWORD previous = 0, ignored = 0;
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameFPSProfile::computationRva), NativeGameFPSProfile::computation.size(),
                               PAGE_READWRITE, &previous) != FALSE,
              "remove execute access only from the synthetic FPS computation page");
        Check(refused(Status::Unsupported), "a readable matching computation pin still requires an executable native image page");
        Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + NativeGameFPSProfile::computationRva), NativeGameFPSProfile::computation.size(),
                               previous, &ignored) != FALSE,
              "restore synthetic FPS code page execute access");
        Check(reader.ReadFPS(value) == Status::Ok && value == 62.5f && ClockTestFaults::clockDataWrites.load() == writes &&
                  ClockTestFaults::clockCodeExchanges.load() == patches,
              "FPS failure recovery performs no engine writes, patch exchanges or lease operations");
        reader.Reset();
        Check(refused(Status::NotInitialized) && refusesGetter() && !reader.IsBound(),
              "retired native FPS access does not retain an earlier successful sample");
        ClockReaderTestHost::image = 0;
        std::puts("Production native stored FPS reader fixture completed");
    }

    void NativePatchTransactions(ClockImageFixture& fixture)
    {
        if (!NativeClockProfile::clockBoundaryReviewed)
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            const auto state = Execute(service, fixture, ClockOperation::Freeze);
            Check(state.lastCommandResult == ClockResult::UnsupportedBuild && !state.patchInstalled && !state.patchRecoveryPending && state.restored,
                  "an unreviewed branch boundary refuses actual freeze writes but still completes safe original-state cleanup");
            FinishOwned(service, fixture);
            std::puts("Native freeze patch capability: withheld by production review gate");
            return;
        }
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            DWORD protection = 0, ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(ClockTestFaults::patchAddress), 8, PAGE_EXECUTE_READWRITE, &protection) != FALSE,
                  "admit a direct atomic comparison test only on the owned code image");
            const auto original = fixture.Get<LONG64>(NativeClockProfile::patchRva - 3);
            Check(!ExchangeCode(original ^ 1, original ^ 2) && fixture.Get<LONG64>(NativeClockProfile::patchRva - 3) == original,
                  "the real aligned atomic code store refuses a changed expected instruction without writing any bytes");
            Check(::VirtualProtect(reinterpret_cast<void*>(ClockTestFaults::patchAddress), 8, protection, &ignored) != FALSE,
                  "restore fixture code protection after comparison-refusal verification");
            MEMORY_BASIC_INFORMATION before{}, after{};
            VirtualQuery(reinterpret_cast<const void*>(ClockTestFaults::patchAddress), &before, sizeof(before));
            auto state = Execute(service, fixture, ClockOperation::Freeze);
            Check(state.lastCommandResult == ClockResult::Ok && state.frozen && state.patchInstalled && state.freezes == 1,
                  "SDK freeze installs the reviewed branch through real quiescent protection/write/cache operations");
            const auto frozen = fixture.Get<LONG64>(NativeClockProfile::patchRva - 3);
            Check((frozen & 0xffffff) == (original & 0xffffff) && CheckBinding(),
                  "independent native readback preserves the three neighboring instruction bytes and changes only the reviewed branch");
            state = Execute(service, fixture, ClockOperation::Freeze, 2);
            Check(state.lastCommandResult == ClockResult::Ok && state.frozen, "repeated actual SDK freeze is idempotent");
            state = Execute(service, fixture, ClockOperation::Unfreeze, 3);
            Check(state.lastCommandResult == ClockResult::Ok && !state.frozen && state.unfreezes == 1,
                  "actual SDK unfreeze restores the original native branch");
            VirtualQuery(reinterpret_cast<const void*>(ClockTestFaults::patchAddress), &after, sizeof(after));
            Check(before.Protect == after.Protect, "successful branch transactions restore the actual original executable page protection");
            FinishOwned(service, fixture);
        }
        for (unsigned fault = 0; fault != 3; ++fault)
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            if (fault == 0)
                ClockTestFaults::failFlushes.store(1);
            if (fault == 1)
                ClockTestFaults::protectionCountdown.store(1);
            if (fault == 2)
                ClockTestFaults::protectionCountdown.store(2);
            const auto state = Execute(service, fixture, ClockOperation::Freeze);
            Check(state.lastCommandResult == ClockResult::PatchFailure && state.failure == ClockResult::PatchFailure && !state.patchInstalled &&
                      !state.patchRecoveryPending && !state.protectionRestorePending && state.restored,
                  "cache failure and protection acquisition/restoration failures roll back before releasing the native owner");
            FinishOwned(service, fixture);
        }
        for (bool protectionFailure : {false, true})
        {
            NativeClockService service;
            BeginOwned(service, fixture);
            if (protectionFailure)
                ClockTestFaults::failProtectionRestores.store(true);
            else
                ClockTestFaults::failFlushes.store(100);
            const auto state = Execute(service, fixture, ClockOperation::Freeze);
            Check(state.lastCommandResult == ClockResult::PatchFailure && state.patchRecoveryPending && state.dataRecoveryPending && !state.restored &&
                      state.restoreFailures && !service.End() && service.IsRetained() && (!protectionFailure || state.protectionRestorePending),
                  "persistent cache or executable-protection restoration failure retains explicit branch, data and protection recovery ownership");
            ClockTestFaults::failFlushes.store(0);
            ClockTestFaults::failProtectionRestores.store(false);
            FinishOwned(service, fixture);
        }
        std::puts("Native freeze patch capability: verified with production review gate");
    }
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute() && std::filesystem::equivalent(std::filesystem::path(argv[0]).parent_path(), argv[1]),
          "durable fixture evidence is restricted to the explicit owned executable output directory");
    ClockInterfaceContract();
    NativeContractValidation();
    NativeServiceAdmissionContract();
    ClockImageFixture fixture;
    fixture.Create(argv[1]);
    NativeReadOnlyGameTimeReader(fixture);
    NativeGameTimingPrerequisite(fixture);
    NativeStoredFPSReader(fixture);
    NativeTimingWritesAndRestoration(fixture);
    NativePauseSafeTimingOwner(fixture);
    NativeReadSetAndRestoration(fixture);
    NativeCoherentCommandPublication(fixture);
    NativeReadOnlyRetirement(fixture);
    NativeFailureAndLease(fixture);
    NativeCaptureAndWraparound(fixture);
    NativePeerResumeRecovery(fixture);
    NativeCommittedHookResumeRecovery(fixture);
    NativeEngineThreadHandoff(fixture);
    NativeCallbackLifetime(fixture);
    NativePatchTransactions(fixture);
    NativeApplicationFrameOwner(fixture);
    std::puts("Production native clock runtime fixture completed");
    std::printf("Passed %u production clock service checks\n", checks.load());
    return 0;
}
