/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualify native world-property cheat bytes without executing cheats
 *****************************************************************************/
#include "NativeCheatProfile.h"
#include "NativeImageAccess.h"
#include <cstring>
#include <limits>
#include <windows.h>

namespace mta::game_de::NativeCheatProfile
{
    const std::array<Pin, 7>           Code = {{
        {0x10708d6, 29, {0x33, 0xdb, 0x48, 0x8d, 0x0d, 0xd1, 0xc7, 0x14, 0x04, 0x8b, 0xc3, 0x48, 0x83, 0xf8, 0x75,
                                   0x73, 0x6a, 0x88, 0x1c, 0x08, 0x48, 0xff, 0xc0, 0x48, 0x83, 0xf8, 0x75, 0x7c, 0xf2}},
        {0x10817b4, 50, {0x48, 0x8d, 0x1d, 0x45, 0xe8, 0xf7, 0xfe, 0x4a, 0x8b, 0x84, 0xe3, 0x70, 0x64, 0x30, 0x04, 0x4c, 0x8b,
                                   0xac, 0x24, 0xa8, 0x01, 0x00, 0x00, 0x48, 0x8b, 0xbc, 0x24, 0xa0, 0x01, 0x00, 0x00, 0x48, 0x8b, 0xb4,
                                   0x24, 0x98, 0x01, 0x00, 0x00, 0x48, 0x85, 0xc0, 0x0f, 0x84, 0xa6, 0x00, 0x00, 0x00, 0xff, 0xd0}},
        {0x108188a, 20, {0x41, 0x80, 0xbc, 0x1c, 0xb0, 0xd0, 0x1b, 0x05, 0x00, 0x0f, 0x94, 0xc0, 0x41, 0x88, 0x84, 0x1c, 0xb0, 0xd0, 0x1b, 0x05}},
        {0x13838a9, 9, {0x80, 0x3d, 0x2d, 0x98, 0xe3, 0x03, 0x00, 0x74, 0x51}},
        {0x137ef10, 34, {0x48, 0x83, 0xec, 0x38, 0x0f, 0xb6, 0x41, 0x6a, 0xc0, 0xe8, 0x03, 0x3c, 0x08, 0x77, 0x72, 0xba, 0x09,
                                   0x01, 0x00, 0x00, 0x0f, 0xa3, 0xc2, 0x73, 0x68, 0x80, 0x3d, 0xbc, 0xe1, 0xe3, 0x03, 0x00, 0x74, 0x5f}},
        {0x13a6cbb, 17, {0x80, 0x3d, 0x2b, 0x64, 0xe1, 0x03, 0x00, 0x74, 0x08, 0xf3, 0x0f, 0x59, 0x1d, 0x38, 0x28, 0x82, 0x03}},
        {0x1353636, 22, {0x80, 0x3d, 0xbb, 0x9a, 0xe6, 0x03, 0x00, 0x74, 0x0d, 0x83, 0xf8, 0x01, 0x77, 0x08, 0xf3, 0x0f, 0x59, 0x15, 0xec, 0x5f, 0x87, 0x03}},
    }};
    const std::array<Registration, 10> Registrations = {{
        {0x10752b8, 0x4306d50, "R4C32S34"},
        {0x1075321, 0x4306d78, "DRIVEONWATER"},
        {0x1076f68, 0x4306f80, "CHITTYCHITTYBANGBANG"},
        {0x1076fd1, 0x4307278, "UD131RL1L"},
        {0x107703a, 0x4307270, "RIPAZHA"},
        {0x1077171, 0x43068a8, "CJPHONEHOME"},
        {0x10771da, 0x4307290, "TSCCSCC12234"},
        {0x1077243, 0x4307288, "JHJOECW"},
        {0x107862c, 0x4307180, "UUTTUULRS44"},
        {0x1078695, 0x4307170, "KANGAROO"},
    }};
    // The fasterclock load pin ends before1147c13. The existing timing owner
    // validates the entire clock consumer against exact stock/owned-freeze
    // bytes before and after each read; no arbitrary code mask is accepted.
    const std::array<ReadMember, 14> ReadMembers = {
        {{MtaGame::CWorldCheatPolicy::Id::HoverCars, 45, 0, {0x13838a9, 9, {0x80, 0x3d, 0x2d, 0x98, 0xe3, 0x03, 0x00, 0x74, 0x51}}},
         {MtaGame::CWorldCheatPolicy::Id::AirCars, 60, 0, {0x137ef10, 34, {0x48, 0x83, 0xec, 0x38, 0x0f, 0xb6, 0x41, 0x6a, 0xc0, 0xe8, 0x03, 0x3c,
                                                                           0x08, 0x77, 0x72, 0xba, 0x09, 0x01, 0x00, 0x00, 0x0f, 0xa3, 0xc2, 0x73,
                                                                           0x68, 0x80, 0x3d, 0xbc, 0xe1, 0xe3, 0x03, 0x00, 0x74, 0x5f}}},
         {MtaGame::CWorldCheatPolicy::Id::ExtraBunny,
          61,
          0,
          {0x13a6cbb, 17, {0x80, 0x3d, 0x2b, 0x64, 0xe1, 0x03, 0x00, 0x74, 0x08, 0xf3, 0x0f, 0x59, 0x1d, 0x38, 0x28, 0x82, 0x03}}},
         {MtaGame::CWorldCheatPolicy::Id::ExtraJump, 72, 0, {0x1353636, 22, {0x80, 0x3d, 0xbb, 0x9a, 0xe6, 0x03, 0x00, 0x74, 0x0d, 0x83, 0xf8,
                                                                             0x01, 0x77, 0x08, 0xf3, 0x0f, 0x59, 0x15, 0xec, 0x5f, 0x87, 0x03}}},
         {MtaGame::CWorldCheatPolicy::Id::TankMode, 64, 0, {0x1316e6a, 56, {0x80, 0x3d, 0x7f, 0x62, 0xea, 0x03, 0x00, 0x74, 0x2f, 0x83, 0xbe, 0xe0,
                                                                            0x07, 0x00, 0x00, 0x01, 0x77, 0x26, 0x48, 0x8b, 0x43, 0x30, 0x48, 0x85,
                                                                            0xc0, 0x74, 0x1d, 0x81, 0x48, 0x78, 0x00, 0x00, 0x80, 0x00, 0x48, 0x8b,
                                                                            0x43, 0x30, 0x81, 0x48, 0x78, 0x00, 0x00, 0x04, 0x00, 0x48, 0x8b, 0x43,
                                                                            0x30, 0x80, 0xa0, 0x72, 0x06, 0x00, 0x00, 0xdf}}},
         {MtaGame::CWorldCheatPolicy::Id::NoReload, 84, 0, {0x13dd43e, 57, {0x44, 0x38, 0x2d, 0xbf, 0xfc, 0xdd, 0x03, 0x75, 0x30, 0x8b, 0x47, 0x08,
                                                                            0x85, 0xc0, 0x7e, 0x05, 0xff, 0xc8, 0x89, 0x47, 0x08, 0x8b, 0x47, 0x0c,
                                                                            0xff, 0xc8, 0x3d, 0xa6, 0x61, 0x00, 0x00, 0x77, 0x18, 0x41, 0x80, 0x7e,
                                                                            0x6a, 0x08, 0x73, 0x0e, 0xe8, 0x55, 0xe5, 0xd9, 0xff, 0x0f, 0x2f, 0x05,
                                                                            0x1a, 0xc5, 0x7e, 0x03, 0x73, 0x03, 0x89, 0x47, 0x0c}}},
         {MtaGame::CWorldCheatPolicy::Id::PerfectHandling, 39, 0, {0x138284c, 36, {0x0f, 0xb6, 0x1d, 0x84, 0xa8, 0xe3, 0x03, 0x74, 0x0f, 0x44, 0x0f, 0x2f,
                                                                                   0xaf, 0x0c, 0x0c, 0x00, 0x00, 0x76, 0x05, 0x41, 0xb2, 0x02, 0xeb, 0x0c,
                                                                                   0xf6, 0xc2, 0x01, 0x75, 0x04, 0x84, 0xdb, 0x74, 0x03, 0x41, 0xb2, 0x01}}},
         {MtaGame::CWorldCheatPolicy::Id::AllCarsHaveNitro, 65, 0, {0x13814de, 30, {0x44, 0x38, 0x2d, 0x0c, 0xbc, 0xe3, 0x03, 0x75, 0x04, 0xa8,
                                                                                    0x10, 0x74, 0x11, 0x81, 0x8f, 0xc0, 0x05, 0x00, 0x00, 0x00,
                                                                                    0x00, 0x08, 0x00, 0xc6, 0x87, 0xf6, 0x06, 0x00, 0x00, 0x65}}},
         {MtaGame::CWorldCheatPolicy::Id::AirBoats, 46, 0, {0x13a98a4, 40, {0x44, 0x38, 0x35, 0x33, 0x38, 0xe1, 0x03, 0x74, 0x1f, 0xba, 0x05, 0x00, 0x00, 0x00,
                                                                            0xf3, 0x0f, 0x11, 0x74, 0x24, 0x28, 0x0f, 0x28, 0xde, 0x0f, 0x28, 0xd6, 0xf3, 0x0f,
                                                                            0x11, 0x74, 0x24, 0x20, 0x48, 0x8b, 0xcb, 0xe8, 0xc4, 0x1c, 0x02, 0x00}}},
         {MtaGame::CWorldCheatPolicy::Id::InfiniteOxygen, 74, 0, {0x1258461, 71, {0x80, 0x3d, 0x92, 0x4c, 0xf6, 0x03, 0x00, 0x0f, 0x85, 0x8e, 0x00, 0x00,
                                                                                  0x00, 0x48, 0x8b, 0x81, 0xf0, 0x05, 0x00, 0x00, 0x0f, 0x57, 0xc9, 0xf3,
                                                                                  0x0f, 0x59, 0x15, 0x58, 0x1e, 0xfe, 0x03, 0xf3, 0x0f, 0x10, 0x40, 0x5c,
                                                                                  0x0f, 0x2f, 0xc1, 0x76, 0x1e, 0xf7, 0x81, 0x38, 0x06, 0x00, 0x00, 0x00,
                                                                                  0x00, 0x10, 0x00, 0x74, 0x12, 0xf3, 0x0f, 0x5c, 0xc2, 0xf3, 0x0f, 0x5f,
                                                                                  0xc1, 0xf3, 0x0f, 0x11, 0x40, 0x5c, 0xe9, 0xab, 0x00, 0x00, 0x00}}},
         {MtaGame::CWorldCheatPolicy::Id::FasterClock, 22, 0, {0x1147c0b, 8, {0x44, 0x0f, 0xb6, 0x0d, 0xb3, 0x54, 0x07, 0x04}}},
         {MtaGame::CWorldCheatPolicy::Id::AlwaysMidnight, 67, 0x107fce0, {0x107fce0, 43, {0x80, 0x3d, 0x0c, 0xd4, 0x13, 0x04, 0x00, 0x0f, 0x94, 0xc0, 0x88,
                                                                                          0x05, 0x03, 0xd4, 0x13, 0x04, 0x84, 0xc0, 0x74, 0x17, 0x33, 0xc0,
                                                                                          0xc6, 0x05, 0x1a, 0xcc, 0x19, 0x04, 0x00, 0x66, 0x89, 0x05, 0xec,
                                                                                          0xbf, 0x1a, 0x04, 0xc6, 0x05, 0x18, 0xcc, 0x19, 0x04, 0x00}}},
         {MtaGame::CWorldCheatPolicy::Id::InfiniteHealth,
          73,
          0,
          {0x119b53a, 19, {0x80, 0x3d, 0xb8, 0x1b, 0x02, 0x04, 0x00, 0x74, 0x0a, 0x83, 0x79, 0x10, 0x2f, 0x0f, 0x8c, 0x05, 0x02, 0x00, 0x00}}},
         {MtaGame::CWorldCheatPolicy::Id::NeverWanted, 77, 0x107ff50, {0x1187ae4, 24, {0x80, 0x3d, 0x12, 0x56, 0x03, 0x04, 0x00, 0x49,
                                                                                       0x8b, 0xf0, 0x8b, 0x69, 0x2c, 0x8b, 0xfa, 0x48,
                                                                                       0x8b, 0xd9, 0x0f, 0x85, 0x31, 0x01, 0x00, 0x00}}}}};
    const std::array<ReadRegistration, 30> ReadRegistrations = {{{MtaGame::CWorldCheatPolicy::Id::TankMode, {0x107771b, 0x4307200, "TOUCHMYCARYOUDIE"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::TankMode, {0x1077784, 0x4307228, "122UDDU344"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::TankMode, {0x10777ed, 0x4307220, "JCNRUAD"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::NoReload, {0x1079dc3, 0x43075f0, "FULLCLIP"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::NoReload, {0x1079e2c, 0x4307610, "13S3L43LSD11"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::NoReload, {0x1079e95, 0x4307608, "WANRLTW"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::PerfectHandling, {0x10746a5, 0x4306f18, "STICKLIKEGLUE"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::PerfectHandling, {0x107470e, 0x4306e58, "T33L3141"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::PerfectHandling, {0x1074777, 0x4306e50, "PGGOMOY"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::AllCarsHaveNitro, {0x107791b, 0x4307248, "SPEEDFREAK"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::AllCarsHaveNitro, {0x1077984, 0x4307238, "LT31USTDC211"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::AllCarsHaveNitro, {0x10779ed, 0x4307268, "COXEFGU"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::AirBoats, {0x1075458, 0x43068d0, "FLYINGFISH"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::AirBoats, {0x10754c1, 0x4306d68, "4CU1R3RUST"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::AirBoats, {0x107552a, 0x4306da0, "AFSNMSMW"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::InfiniteOxygen, {0x10789c3, 0x4307450, "MANFROMATLANTIS"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::InfiniteOxygen, {0x1078a2c, 0x4307470, "DL1DD4D2D"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::InfiniteOxygen, {0x1078a95, 0x4307468, "CVWKXAM"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::FasterClock, {0x10725d0, 0x4306a58, "TIMEJUSTFLIESBY"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::FasterClock, {0x1072634, 0x4306a78, "CC1S1SSS1TCT"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::FasterClock, {0x1072698, 0x4306a70, "YSOHNUL"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::AlwaysMidnight, {0x1077d1b, 0x43071b8, "NIGHTPROWLER"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::AlwaysMidnight, {0x1077d84, 0x43071a8, "4X11222T"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::AlwaysMidnight, {0x1077ded, 0x43071e0, "XJVSNAJ"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::InfiniteHealth, {0x10787c3, 0x4307440, "NOONECANHURTME"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::InfiniteHealth, {0x107882c, 0x4307430, "TXXCCS1423"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::InfiniteHealth, {0x1078895, 0x4307460, "BAGUVIX"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::NeverWanted, {0x1078fc3, 0x4307408, "IDOASIPLEASE"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::NeverWanted, {0x107902c, 0x43073f8, "CRCRLSTU"}},
                                                                 {MtaGame::CWorldCheatPolicy::Id::NeverWanted, {0x1079095, 0x4307428, "AEZAKMI"}}}};
    namespace
    {
        namespace Image = NativeImageAccess;
        bool Verify(std::uintptr_t image, bool strictFourValues = true, const ReadMember* selected = nullptr) noexcept
        {
            if (sizeof(void*) != 8 || !image || image > (std::numeric_limits<std::uintptr_t>::max)() - ImageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > ImageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            const auto         ntRva = static_cast<std::uintptr_t>(dos.e_lfanew);
            IMAGE_NT_HEADERS64 nt{};
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionRva > ImageSize || sectionBytes > ImageSize - sectionRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(image, image + sectionRva, sections.data(), sectionBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (Image::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(FlagsRva, NativeFlagCount, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !Image::ImagePages(image, image + FlagsRva, NativeFlagCount, strictFourValues))
                return false;
            for (const auto& pin : Code)
            {
                std::array<unsigned char, 160> actual{};
                if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            for (const auto& registration : Registrations)
            {
                std::array<unsigned char, 7> instruction{};
                std::array<char, 32>         name{};
                const auto                   size = std::strlen(registration.name) + 1;
                if (size > name.size() ||
                    !contains(registration.instructionRva, instruction.size(), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !contains(registration.nameRva, size, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + registration.instructionRva, instruction.data(), instruction.size(), false, true) ||
                    instruction[0] != 0x4c || instruction[1] != 0x8d || instruction[2] != 0x05 ||
                    !Image::ReadImage(image, image + registration.nameRva, name.data(), size) || std::memcmp(name.data(), registration.name, size))
                    return false;
                std::int32_t relative = 0;
                std::memcpy(&relative, instruction.data() + 3, sizeof(relative));
                if (static_cast<std::int64_t>(registration.instructionRva) + 7 + relative != static_cast<std::int64_t>(registration.nameRva))
                    return false;
            }
            for (const auto& member : Members)
            {
                if (member.nativeIndex >= NativeFlagCount)
                    return false;
                const auto    slotRva = DispatchTableRva + member.nativeIndex * sizeof(std::uint64_t);
                std::uint64_t function = 1;
                unsigned char value = 0;
                // The native dispatcher directly toggles these null-function
                // entries. Calling its broader action path would run unrelated
                // achievement/UI/gameplay side effects and is never required.
                if (!contains(slotRva, sizeof(function), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + slotRva, &function, sizeof(function)) || function != 0 ||
                    !Image::ReadImage(image, image + FlagsRva + member.nativeIndex, &value, sizeof(value), strictFourValues) || (strictFourValues && value > 1))
                    return false;
            }
            if (selected)
            {
                const auto                     slot = DispatchTableRva + selected->nativeIndex * sizeof(std::uint64_t);
                const auto&                    pin = selected->consumer;
                std::array<unsigned char, 160> actual{};
                std::uint64_t                  action = 0;
                if (selected->nativeIndex >= NativeFlagCount || !pin.size || pin.size > actual.size() ||
                    !contains(slot, sizeof(action), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + slot, &action, sizeof(action)) || action != (selected->actionRva ? image + selected->actionRva : 0) ||
                    !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes.data(), pin.size) ||
                    !Image::ImagePages(image, image + FlagsRva + selected->nativeIndex, 1))
                    return false;
                for (const auto& registered : ReadRegistrations)
                {
                    if (registered.id != selected->id)
                        continue;
                    const auto&                  registration = registered.native;
                    std::array<unsigned char, 7> instruction{};
                    std::array<char, 32>         name{};
                    const auto                   size = std::strlen(registration.name) + 1;
                    if (size > name.size() ||
                        !contains(registration.instructionRva, instruction.size(), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                        !contains(registration.nameRva, size, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE) ||
                        !Image::ReadImage(image, image + registration.instructionRva, instruction.data(), instruction.size(), false, true) ||
                        instruction[0] != 0x4c || instruction[1] != 0x8d || instruction[2] != 0x05 ||
                        !Image::ReadImage(image, image + registration.nameRva, name.data(), size) || std::memcmp(name.data(), registration.name, size))
                        return false;
                    std::int32_t relative = 0;
                    std::memcpy(&relative, instruction.data() + 3, sizeof(relative));
                    if (static_cast<std::int64_t>(registration.instructionRva) + 7 + relative != static_cast<std::int64_t>(registration.nameRva))
                        return false;
                }
            }
            return true;
        }
    }
    const ReadMember* FindRead(MtaGame::CWorldCheatPolicy::Id id) noexcept
    {
        if (!MtaGame::CWorldCheatPolicy::Get(id))
            return nullptr;
        for (const auto& member : ReadMembers)
            if (member.id == id)
                return &member;
        return nullptr;
    }
    bool RevalidateRead(const Binding& binding, MtaGame::CWorldCheatPolicy::Id id) noexcept
    {
        const auto* member = FindRead(id);
        const auto  image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        return member && image && binding.image == image && image <= (std::numeric_limits<std::uintptr_t>::max)() - ImageSize &&
               binding.flags == image + FlagsRva && Verify(image, false, member);
    }

    bool Find(const char* name, Flag& output) noexcept
    {
        if (!name)
            return false;
        for (std::size_t index = 0; index < Members.size(); ++index)
            if (!std::strcmp(name, Members[index].sdkName))
            {
                output = static_cast<Flag>(index);
                return true;
            }
        return false;
    }
    bool Qualify(Binding& output) noexcept
    {
        if (sizeof(void*) != 8 || !NativeImageAccess::CheckExecutableBuild())
            return false;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!Verify(image))
            return false;
        output = {image, image + FlagsRva};
        return true;
    }
    bool Revalidate(const Binding& binding) noexcept
    {
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        return image && binding.image == image && image <= (std::numeric_limits<std::uintptr_t>::max)() - ImageSize && binding.flags == image + FlagsRva &&
               Verify(image);
    }
}
