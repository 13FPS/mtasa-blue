/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read genuine native startup states without lifecycle emulation
 *****************************************************************************/
#include "NativeGameStateReader.h"
#include "NativeGameStateProfile.h"
#include "NativeFrontendMenuProfile.h"
#include "NativeImageAccess.h"

#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace
    {
        namespace Profile = NativeGameStateProfile;
        namespace Image = NativeImageAccess;
        namespace MenuProfile = NativeFrontendMenuProfile;

        template <typename T>
        T Operand(const unsigned char* bytes) noexcept
        {
            T value{};
            std::memcpy(&value, bytes, sizeof(value));
            return value;
        }

        bool Binding(std::uintptr_t image) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - Profile::imageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > Profile::imageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::size_t>(dos.e_lfanew);
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != Profile::imageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionsBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionsRva > Profile::imageSize || sectionsBytes > Profile::imageSize - sectionsRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(image, image + sectionsRva, sections.data(), sectionsBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(Profile::stateRva, sizeof(std::uint32_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ImagePages(image, image + Profile::stateRva, sizeof(std::uint32_t), true))
                return false;

            // These bounded pins qualify both the numeric dispatch and the
            // independently identified semantic paths. A modified image cannot
            // silently reuse our mapping merely because its state stays in range.
            for (const auto& pin : Profile::pins)
            {
                std::array<unsigned char, 94> actual{};
                if (pin.size > actual.size() ||
                    !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | (pin.code ? IMAGE_SCN_MEM_EXECUTE : 0),
                              IMAGE_SCN_MEM_WRITE | (pin.code ? 0 : IMAGE_SCN_MEM_EXECUTE)) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) || std::memcmp(actual.data(), pin.bytes, pin.size) != 0)
                    return false;
                if (pin.rva == Profile::state_dispatchRva)
                {
                    const auto state = static_cast<std::int64_t>(pin.rva + 6) + Operand<std::int32_t>(actual.data() + 2);
                    const auto base = static_cast<std::int64_t>(pin.rva + 52) + Operand<std::int32_t>(actual.data() + 48);
                    if (state != static_cast<std::int64_t>(Profile::stateRva) || base != 0 || actual[28] != 9 ||
                        Operand<std::uint32_t>(actual.data() + 55) != Profile::state_tableRva)
                        return false;
                }
                if (pin.rva == Profile::state_tableRva)
                {
                    constexpr std::array<std::uint32_t, 4> targets = {0x120a01f, 0x120a12f, 0x120a234, 0x120a32c};
                    for (std::size_t index = 0; index < targets.size(); ++index)
                        if (Operand<std::uint32_t>(actual.data() + (index + 6) * sizeof(std::uint32_t)) != targets[index])
                            return false;
                }
            }
            return true;
        }

        bool MenuHeapRead(std::uintptr_t address, void* output, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + bytes;
            auto       at = address;
            while (at < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<const void*>(at), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Type != MEM_PRIVATE && info.Type != MEM_MAPPED) || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto protection = info.Protect & 0xff;
                if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)
                    return false;
                const auto region = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - region || region + info.RegionSize <= at)
                    return false;
                at = region + info.RegionSize < end ? region + info.RegionSize : end;
            }
            SIZE_T copied = 0;
            // The OS copy handles protection/lifetime changes after VirtualQuery.
            // This guards an observation; it cannot pin a UObject against GC.
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, bytes, &copied) && copied == bytes;
        }

        bool MenuBinding(std::uintptr_t image) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - MenuProfile::imageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > MenuProfile::imageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::size_t>(dos.e_lfanew);
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != MenuProfile::imageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionsRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionsBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionsRva > MenuProfile::imageSize || sectionsBytes > MenuProfile::imageSize - sectionsRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(image, image + sectionsRva, sections.data(), sectionsBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            const auto data = [&](std::uintptr_t rva, std::size_t bytes)
            {
                return contains(rva, bytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) &&
                       Image::ImagePages(image, image + rva, bytes, true);
            };
            if (!data(MenuProfile::engineRootRva, sizeof(std::uintptr_t)) || !data(MenuProfile::menuHeaderRva, 24))
                return false;
            for (const auto& pin : MenuProfile::pins)
            {
                std::array<unsigned char, MenuProfile::maximumPinBytes> actual{};
                if (pin.size > actual.size() ||
                    !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | (pin.code ? IMAGE_SCN_MEM_EXECUTE : 0),
                              IMAGE_SCN_MEM_WRITE | (pin.code ? 0 : IMAGE_SCN_MEM_EXECUTE)) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) || std::memcmp(actual.data(), pin.bytes, pin.size) != 0)
                    return false;
            }
            const auto pointer = [&](std::uintptr_t slot, std::uintptr_t target, bool code)
            {
                std::uintptr_t actual = 0;
                return contains(slot, sizeof(actual), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) &&
                       contains(target, 1, IMAGE_SCN_MEM_READ | (code ? IMAGE_SCN_MEM_EXECUTE : 0), IMAGE_SCN_MEM_WRITE | (code ? 0 : IMAGE_SCN_MEM_EXECUTE)) &&
                       Image::ReadImage(image, image + slot, &actual, sizeof(actual)) && Image::ImagePages(image, image + target, 1, false, code) &&
                       actual >= image && actual - image == target;
            };
            // Loaded vtable/registration pointers carry ASLR relocations. Pin
            // their image-relative targets, never raw preferred-base VA bytes.
            for (const auto& target : MenuProfile::vtableTargets)
                if (!pointer(target.slotRva, target.targetRva, true))
                    return false;
            for (const auto& target : MenuProfile::imagePointers)
                if (!pointer(target.slotRva, target.targetRva, target.code))
                    return false;
            return true;
        }

        struct MenuSnapshot
        {
            std::uintptr_t root = 0, vptr = 0, widget = 0, screens = 0, pending = 0;
            std::int32_t   count = 0, capacity = 0;

            bool operator==(const MenuSnapshot& other) const noexcept
            {
                return root == other.root && vptr == other.vptr && widget == other.widget && screens == other.screens && pending == other.pending &&
                       count == other.count && capacity == other.capacity;
            }
        };

        bool MenuSample(std::uintptr_t image, MenuSnapshot& output) noexcept
        {
            MenuSnapshot                  next;
            std::array<unsigned char, 24> header{};
            std::uintptr_t                finalRoot = 0, finalVptr = 0;
            if (!Image::ReadImage(image, image + MenuProfile::engineRootRva, &next.root, sizeof(next.root), true) ||
                next.root > (std::numeric_limits<std::uintptr_t>::max)() - MenuProfile::ownerSampleBytes ||
                !MenuHeapRead(next.root, &next.vptr, sizeof(next.vptr)) || next.vptr != image + MenuProfile::engineVtableRva ||
                !MenuHeapRead(next.root + MenuProfile::widgetOffset, &next.widget, sizeof(next.widget)) ||
                !Image::ReadImage(image, image + MenuProfile::menuHeaderRva, header.data(), header.size(), true) ||
                !MenuHeapRead(next.root, &finalVptr, sizeof(finalVptr)) ||
                !Image::ReadImage(image, image + MenuProfile::engineRootRva, &finalRoot, sizeof(finalRoot), true) || next.root != finalRoot ||
                next.vptr != finalVptr)
                return false;
            next.screens = Operand<std::uintptr_t>(header.data());
            next.count = Operand<std::int32_t>(header.data() + 8);
            next.capacity = Operand<std::int32_t>(header.data() + 12);
            next.pending = Operand<std::uintptr_t>(header.data() + 16);
            if (next.count < 0 || next.capacity < 0 || next.count > next.capacity || (next.count && !next.screens))
                return false;
            // Widget, screen data and pending screen are opaque presence bits.
            // Sampling them does not need a pointee dereference or native call.
            output = next;
            return true;
        }

        bool Translate(std::uint32_t native, SystemState::Enum& output) noexcept
        {
            // Early native states 0..5 have no established SDK semantics. Do
            // not infer them from matching integer values or from user pause.
            if (native > 9)
                return false;
            switch (native)
            {
                case 6:
                    output = SystemState::GS_INIT_FRONTEND;
                    return true;
                case 7:
                    output = SystemState::GS_FRONTEND;
                    return true;
                case 8:
                    output = SystemState::GS_INIT_PLAYING_GAME;
                    return true;
                case 9:
                    output = SystemState::GS_PLAYING_GAME;
                    return true;
                default:
                    return false;
            }
        }
    }

    Status NativeGameStateReader::Bind() noexcept
    {
        const auto thread = GetCurrentThreadId();
        if (ownerThread.load() && ownerThread.load() != thread)
            return Status::WrongThread;
        Reset();
        if (sizeof(void*) != 8 || !Image::CheckExecutableBuild())
            return Status::Unsupported;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!Binding(image))
            return Status::Unsupported;
        // Admission is independent of the current state. This caller owns only
        // subsequent reads, never a native callback or lifecycle-write lease.
        imageAddress.store(image);
        ownerThread.store(thread);
        return Status::Ok;
    }

    Status NativeGameStateReader::Read(SystemState::Enum& output) const noexcept
    {
        const auto owner = ownerThread.load();
        const auto image = imageAddress.load();
        if (!owner || !image)
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        std::uint32_t     first = 0, second = 0;
        SystemState::Enum next;
        if (!Binding(image) || !Image::ReadImage(image, image + Profile::stateRva, &first, sizeof(first), true) ||
            !Image::ReadImage(image, image + Profile::stateRva, &second, sizeof(second), true) || first != second || !Translate(second, next) ||
            !Binding(image))
            return Status::Unsupported;
        // Matching samples qualify the returned value without claiming an
        // engine lock. Failed reads and unproved states leave output untouched.
        output = next;
        return Status::Ok;
    }

    Status NativeGameStateReader::ReadMenu(bool& output) const noexcept
    {
        const auto owner = ownerThread.load();
        const auto image = imageAddress.load();
        if (!owner || !image)
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        MenuSnapshot first, second;
        if (!MenuBinding(image) || !MenuSample(image, first) || !MenuSample(image, second) || !(first == second) || !MenuBinding(image))
            return Status::Unsupported;
        // These are bounded matching observations, not GC pinning, an engine
        // lock, or permission to invoke lifecycle methods on this read thread.
        output = second.widget != 0 || second.count > 0 || second.pending != 0;
        return Status::Ok;
    }

    void NativeGameStateReader::Reset() noexcept
    {
        ownerThread.store(0);
        imageAddress.store(0);
    }

    bool NativeGameStateReader::IsBound() const noexcept
    {
        return ownerThread.load() != 0 && imageAddress.load() != 0;
    }
}
