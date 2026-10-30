/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify one genuine Restore and its engine-owned reference surgery
 ****************************************************************************/
#include "NativeCameraRestore.h"
#include "NativeCameraRestoreProfile.h"
#include "NativePedReferenceMemory.h"
#include "NativePedReferenceProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeImageAccess.h"
#include "NativeStartupProvidersAccess.h"
#include <intrin.h>
#include <array>
#include <cstring>
#include <initializer_list>
#include <limits>
namespace mta::game_de
{
    namespace CameraRestoreDetail
    {
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraRestoreProfile;
        namespace R = NativePedReferenceProfile;
        namespace Pool = NativePedPoolProfile;
        using CheckOwner = NativeCameraRestoreCalls::CheckOwner;
        struct Node
        {
            std::uintptr_t next = 0, cell = 0;
        };
        static_assert(sizeof(Node) == R::ReferenceBytes);
        struct Snapshot
        {
            D::Value                                  world;
            std::array<unsigned char, P::CameraBytes> camera{};
            std::array<Node, R::ReferenceCount>       bank{};
            NativePedPoolReader::Value                player;
            std::uintptr_t                            head = 0, freeHead = 0;
            std::uint32_t                             flags = 0, mode = 0;
            bool                                      noEffect = false;
        };
        bool Profile(std::uintptr_t image) noexcept
        {
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - Pool::ImageSize ||
                !NativeImageAccess::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > Pool::ImageSize - sizeof(nt) ||
                !NativeImageAccess::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
                nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != Pool::ImageSize ||
                nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (at > Pool::ImageSize || bytes > Pool::ImageSize - at || !NativeImageAccess::ReadImage(image, image + at, sections.data(), bytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                    if (NativeImageAccess::SectionContains(sections[i], rva, size, required, forbidden))
                        return true;
                return false;
            };
            std::array<unsigned char, P::Restore.size()> actual{};
            return contains(P::EntryRva, actual.size(), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) &&
                   contains(P::CameraRva, P::CameraBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) &&
                   NativeImageAccess::ReadImage(image, image + P::EntryRva, actual.data(), actual.size(), false, true) && actual == P::Restore &&
                   NativeImageAccess::ImagePages(image, image + P::CameraRva, P::CameraBytes, true) && D::Profile(image);
        }
        template <class T>
        T Get(const Snapshot& value, std::size_t offset) noexcept
        {
            T result{};
            std::memcpy(&result, value.camera.data() + offset, sizeof(result));
            return result;
        }
        template <class T>
        void Put(Snapshot& value, std::size_t offset, T next) noexcept
        {
            std::memcpy(value.camera.data() + offset, &next, sizeof(next));
        }
        bool Index(std::uintptr_t image, std::uintptr_t node, std::size_t& index) noexcept
        {
            const auto begin = image + R::ReferencesRva;
            if (node < begin || node - begin >= R::ReferenceCount * R::ReferenceBytes || (node - begin) % R::ReferenceBytes)
                return false;
            index = static_cast<std::size_t>((node - begin) / R::ReferenceBytes);
            return true;
        }
        bool Graph(std::uintptr_t image, const Snapshot& value) noexcept
        {
            std::array<bool, R::ReferenceCount> free{}, used{};
            for (const auto& node : value.bank)
            {
                std::size_t ignored = 0;
                if (node.next && !Index(image, node.next, ignored))
                    return false;
            }
            auto cursor = value.freeHead;
            for (std::size_t count = 0; cursor; ++count)
            {
                std::size_t index = 0;
                if (count >= R::ReferenceCount || !Index(image, cursor, index) || free[index] || value.bank[index].cell)
                    return false;
                free[index] = true;
                cursor = value.bank[index].next;
            }
            cursor = value.head;
            for (std::size_t count = 0; cursor; ++count)
            {
                std::size_t index = 0;
                if (count >= R::ReferenceCount || !Index(image, cursor, index) || free[index] || used[index] || !value.bank[index].cell)
                    return false;
                used[index] = true;
                cursor = value.bank[index].next;
            }
            for (const auto offset : {std::size_t{0x7f8}, std::size_t{0x800}})
            {
                const auto target = Get<std::uintptr_t>(value, offset);
                if (target && target != value.player.nativePed)
                    return false;
                const auto  cell = image + P::CameraRva + offset;
                std::size_t matches = 0;
                for (std::size_t index = 0; index != value.bank.size(); ++index)
                    if (value.bank[index].cell == cell)
                    {
                        if (!used[index] || free[index])
                            return false;
                        ++matches;
                    }
                if (matches != (target ? 1u : 0u))
                    return false;
            }
            return true;
        }
        bool SamePlayer(const NativePedPoolReader::Value& a, const NativePedPoolReader::Value& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        bool Same(const Snapshot& a, const Snapshot& b) noexcept
        {
            return D::Same(a.world, b.world, true) && a.camera == b.camera && a.noEffect == b.noEffect && SamePlayer(a.player, b.player) && a.head == b.head &&
                   a.freeHead == b.freeHead && a.flags == b.flags && a.mode == b.mode && !std::memcmp(a.bank.data(), b.bank.data(), sizeof(a.bank));
        }
        ClockResult Player(std::uintptr_t image, const NativePlayerInfoMemory& players, CheckOwner owner, NativePedPoolReader::Value& output) noexcept
        {
            NativePlayerInfoMemory::Identity identity;
            auto                             result = players.ReadIdentity(identity, owner);
            if (result != ClockResult::Ok)
                return result;
            if (identity.image != image || !identity.ped)
                return ClockResult::BindingChanged;
            std::uintptr_t pool = 0, objects = 0, flags = 0;
            std::int32_t   capacity = 0;
            if (!NativeImageAccess::ReadImage(image, image + Pool::RootRva, &pool, sizeof(pool), true) || !D::At(pool, Pool::ObjectsOffset, objects) ||
                !D::At(pool, Pool::FlagsOffset, flags) || !D::At(pool, Pool::CapacityOffset, capacity))
                return ClockResult::ReadFailure;
            if (capacity != static_cast<std::int32_t>(Pool::NativeCapacity) || !objects || !flags || identity.ped < objects ||
                identity.ped - objects >= Pool::NativeCapacity * Pool::ObjectStride || (identity.ped - objects) % Pool::ObjectStride)
                return ClockResult::BindingChanged;
            const auto    slot = static_cast<std::uint32_t>((identity.ped - objects) / Pool::ObjectStride);
            unsigned char generation = 0;
            if (!D::At(flags, slot, generation))
                return ClockResult::ReadFailure;
            result = NativePedPoolReader({image}, owner).Resolve((slot << 8) | generation, output);
            return result != ClockResult::Ok ? result : output.nativePed == identity.ped ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        ClockResult Capture(std::uintptr_t image, const NativePlayerInfoMemory& players, CheckOwner owner, Snapshot& value,
                            bool useRestoreGuard = true) noexcept
        {
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::BindingChanged;
            if ((result = D::Capture(image, value.world)) != ClockResult::Ok)
                return result;
            if (!NativeImageAccess::ReadImage(image, image + P::CameraRva, value.camera.data(), value.camera.size(), true))
                return ClockResult::ReadFailure;
            value.noEffect = useRestoreGuard && value.camera[0x33] != 0;
            if (!value.noEffect)
            {
                if (!NativePedReferenceMemory::Revalidate({image}))
                    return ClockResult::UnsupportedBuild;
                if ((result = Player(image, players, owner, value.player)) != ClockResult::Ok)
                    return result;
                unsigned char type = 0;
                if (!D::Writable(value.player.nativePed + R::EntityHeadOffset, sizeof(value.head)) ||
                    !D::At(value.player.nativePed, R::EntityHeadOffset, value.head) || !D::At(value.player.nativePed, 0x634, value.flags) ||
                    !D::At(value.player.nativePed, 0x5fc, value.mode) || !D::At(value.player.nativePed, R::TypeOffset, type) ||
                    !NativeImageAccess::ReadImage(image, image + R::FreeHeadRva, &value.freeHead, sizeof(value.freeHead), true) ||
                    !NativeImageAccess::ReadImage(image, image + R::ReferencesRva, value.bank.data(), sizeof(value.bank), true))
                    return ClockResult::ReadFailure;
                if ((type & 7) != 3 || (value.flags & 0x100) || !Graph(image, value))
                    return ClockResult::BindingChanged;
            }
            if (!D::Fence(image, value.world) || !Profile(image))
                return ClockResult::BindingChanged;
            return D::Owner(image, owner);
        }
        bool Unregister(std::uintptr_t image, Snapshot& value, std::uintptr_t cell) noexcept
        {
            std::uintptr_t* previous = &value.head;
            for (std::size_t count = 0; *previous && count != R::ReferenceCount; ++count)
            {
                std::size_t index = 0;
                if (!Index(image, *previous, index))
                    return false;
                auto& node = value.bank[index];
                if (node.cell == cell)
                {
                    const auto removed = *previous;
                    *previous = node.next;
                    node.next = value.freeHead;
                    node.cell = 0;
                    value.freeHead = removed;
                    return true;
                }
                previous = &node.next;
            }
            return false;
        }
        bool Register(std::uintptr_t image, Snapshot& value, std::uintptr_t cell) noexcept
        {
            std::size_t index = 0;
            if (!Index(image, value.freeHead, index))
                return false;
            const auto added = value.freeHead;
            auto&      node = value.bank[index];
            value.freeHead = node.next;
            node.next = value.head;
            node.cell = cell;
            value.head = added;
            return true;
        }
        bool Plan(std::uintptr_t image, Snapshot& value) noexcept
        {
            if (value.noEffect)
                return true;
            // Deliberately exclude the source's global-pool prune fallback.
            // A guessed available registration is not an acknowledged ref.
            if (!value.freeHead)
                return false;
            const auto primary = image + P::CameraRva + 0x7f8, secondary = image + P::CameraRva + 0x800;
            if (Get<std::uintptr_t>(value, 0x7f8) && !Unregister(image, value, primary))
                return false;
            Put(value, 0x7f8, value.player.nativePed);
            if (!Register(image, value, primary))
                return false;
            if (value.mode == 0x3d && (!Unregister(image, value, primary) || !Register(image, value, primary)))
                return false;
            if (Get<std::uintptr_t>(value, 0x800) && !Unregister(image, value, secondary))
                return false;
            Put(value, 0x800, std::uintptr_t{0});
            Put(value, 0xaf4, std::uint16_t{1});
            Put(value, 0x30, std::uint16_t{1});
            Put(value, 0x36, std::uint8_t{0});
            Put(value, 0xaec, std::uint32_t{30});
            Put(value, 0x150, std::uint32_t{0});
            Put(value, 0x38, std::uint8_t{0});
            Put(value, 0x54, std::uint8_t{0});
            Put(value, 0xb0, std::uint32_t{0});
            Put(value, 0xaf0, static_cast<std::uint16_t>(value.mode == 0x3b || value.mode == 0x39 || value.mode == 0x35 ? 0x12 : 4));
            Put(value, 0x45, std::uint16_t{0});
            Put(value, 0x3d, std::uint16_t{0});
            Put(value, 0x174, std::uint32_t{0});
            Put(value, 0x37, std::uint8_t{1});
            Put(value, 0x24, std::uint8_t{1});
            return Graph(image, value);
        }
        struct TypeWitness
        {
            std::uintptr_t                candidate = 0, base = 0, selected = 0;
            std::array<unsigned char, 12> candidateHeader{}, baseHeader{};
        };
        struct ProviderSnapshot
        {
            D::Value                            graph;
            NativeStartupProvidersAccess::Value providers;
            std::uintptr_t                      engine = 0, member = 0;
            TypeWitness                         viewportType, managerType;
            std::uint8_t                        flag = 0;
        };
        bool Section(std::uintptr_t image, std::uintptr_t rva, std::size_t size, bool code) noexcept
        {
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!NativeImageAccess::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > Pool::ImageSize - sizeof(nt) ||
                !NativeImageAccess::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
                nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (at > Pool::ImageSize || bytes > Pool::ImageSize - at || !NativeImageAccess::ReadImage(image, image + at, sections.data(), bytes))
                return false;
            const DWORD required = IMAGE_SCN_MEM_READ | (code ? IMAGE_SCN_MEM_EXECUTE : 0);
            const DWORD forbidden = IMAGE_SCN_MEM_WRITE | (code ? 0 : IMAGE_SCN_MEM_EXECUTE);
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (NativeImageAccess::SectionContains(sections[i], rva, size, required, forbidden))
                    return true;
            return false;
        }
        bool JumpCutProfile(std::uintptr_t image) noexcept
        {
            if (!Profile(image))
                return false;
            const auto pins = [&](const auto& values)
            {
                for (const auto& pin : values)
                {
                    std::array<unsigned char, P::JumpCut.size()> actual{};
                    if (!pin.size || pin.size > actual.size() || !Section(image, pin.rva, pin.size, pin.code) ||
                        !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) ||
                        std::memcmp(actual.data(), pin.bytes, pin.size))
                        return false;
                }
                return true;
            };
            for (const auto& pin : P::JumpCutPins)
            {
                std::array<unsigned char, P::JumpCut.size()> actual{};
                if (!Section(image, pin.rva, pin.size, true) || !NativeImageAccess::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return pins(NativeStartupProvidersProfile::pins);
        }
        bool Type(std::uintptr_t image, std::uintptr_t object, std::uintptr_t cacheRva, TypeWitness& value, D::Value& graph) noexcept
        {
            namespace T = NativeStartupProvidersProfile;
            if (!NativeImageAccess::ReadImage(image, image + cacheRva, &value.base, sizeof(value.base), true) || !value.base ||
                !D::At(object, T::objectClassOffset, value.candidate) || !value.candidate || !D::Add(image, value.candidate, graph) ||
                !D::Add(image, value.base, graph) || !D::At(value.candidate, T::classChainOffset, value.candidateHeader) ||
                !D::At(value.base, T::classChainOffset, value.baseHeader))
                return false;
            std::int32_t   candidateIndex = -1, baseIndex = -1;
            std::uintptr_t chain = 0;
            std::memcpy(&chain, value.candidateHeader.data(), sizeof(chain));
            std::memcpy(&candidateIndex, value.candidateHeader.data() + 8, sizeof(candidateIndex));
            std::memcpy(&baseIndex, value.baseHeader.data() + 8, sizeof(baseIndex));
            bool matches = false;
            return candidateIndex >= 0 && candidateIndex <= NativeClassChainAccess::maximumIndex && baseIndex >= 0 && baseIndex <= candidateIndex &&
                   NativeClassChainAccess(&D::Copy).IsChildOf(value.candidate, value.base, matches) && matches &&
                   D::At(chain, static_cast<std::size_t>(baseIndex) * 8, value.selected) && value.selected == value.base + T::classChainOffset;
        }
        bool SameType(const TypeWitness& a, const TypeWitness& b) noexcept
        {
            return a.candidate == b.candidate && a.base == b.base && a.selected == b.selected && a.candidateHeader == b.candidateHeader &&
                   a.baseHeader == b.baseHeader;
        }
        bool SameProvider(const ProviderSnapshot& a, const ProviderSnapshot& b) noexcept
        {
            return D::Same(a.graph, b.graph, true) && a.engine == b.engine && a.member == b.member && a.flag == b.flag &&
                   a.providers.controllerAddress == b.providers.controllerAddress && a.providers.viewportAddress == b.providers.viewportAddress &&
                   a.providers.managerAddress == b.providers.managerAddress && SameType(a.viewportType, b.viewportType) &&
                   SameType(a.managerType, b.managerType);
        }
        ClockResult Provider(std::uintptr_t image, const Snapshot& camera, CheckOwner owner, ProviderSnapshot& output) noexcept
        {
            namespace T = NativeStartupProvidersProfile;
            auto result = D::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!JumpCutProfile(image) || Get<std::uint8_t>(camera, 0x47) != 0)
                return ClockResult::BindingChanged;
            output.graph = camera.world;
            if (!NativeStartupProvidersAccess(image, &D::Copy).Read(camera.world.world, output.providers) ||
                !NativeImageAccess::ReadImage(image, image + T::engineRva, &output.engine, sizeof(output.engine), true) ||
                !D::Add(image, output.engine, output.graph) || !D::Add(image, output.providers.controllerAddress, output.graph) ||
                !D::Add(image, output.providers.viewportAddress, output.graph) || !D::Add(image, output.providers.managerAddress, output.graph) ||
                !Type(image, output.providers.viewportAddress, T::viewportClassCacheRva, output.viewportType, output.graph) ||
                !Type(image, output.providers.managerAddress, T::managerClassCacheRva, output.managerType, output.graph))
                return ClockResult::BindingChanged;
            std::uintptr_t managerVtable = 0;
            if (!D::At(output.providers.managerAddress, 0, managerVtable) || managerVtable != image + T::managerVtableRva ||
                !D::At(output.providers.managerAddress, 0x28, output.member) || !output.member || !D::Add(image, output.member, output.graph) ||
                !D::At(output.member, 0x288, output.flag) || output.flag != 0 || !D::Fence(image, output.graph) || !JumpCutProfile(image))
                return ClockResult::BindingChanged;
            // This is the source's real first-member false branch with DL=1.
            // No lazy manager/class initialization, alternate member or true
            // reflected UI event may be discovered after the first effect.
            return D::Owner(image, owner);
        }
        bool PlanJumpCut(std::uintptr_t image, Snapshot& value) noexcept
        {
            if (!value.freeHead || Get<std::uint8_t>(value, 0x47))
                return false;
            const auto primary = image + P::CameraRva + 0x7f8;
            if (Get<std::uintptr_t>(value, 0x7f8) && !Unregister(image, value, primary))
                return false;
            Put(value, 0x7f8, value.player.nativePed);
            if (!Register(image, value, primary))
                return false;
            if (value.mode == 0x3d && (!Unregister(image, value, primary) || !Register(image, value, primary)))
                return false;
            // Unlike ordinary Restore, original JumpCut keeps the secondary
            // target,37 and174; the exact source writes WORD35 rather than37.
            Put(value, 0x35, std::uint16_t{1});
            Put(value, 0x30, std::uint16_t{1});
            Put(value, 0xaf4, std::uint16_t{2});
            Put(value, 0xb0, std::uint32_t{0});
            Put(value, 0x150, std::uint32_t{0});
            Put(value, 0x38, std::uint8_t{0});
            Put(value, 0xaec, std::uint32_t{30});
            Put(value, 0x54, std::uint8_t{0});
            Put(value, 0x24, std::uint8_t{1});
            Put(value, 0x45, std::uint16_t{0});
            Put(value, 0xaf0, static_cast<std::uint16_t>(value.mode == 0x3b || value.mode == 0x39 || value.mode == 0x35 ? 0x12 : 4));
            Put(value, 0x3d, std::uint16_t{0});
            return Graph(image, value);
        }
        ClockResult ExchangeBehind(std::uintptr_t address, std::uint8_t expected, std::uint8_t target, std::uint8_t& observed, bool& accepted,
                                   std::uint32_t& exception) noexcept
        {
            __try
            {
                observed = static_cast<std::uint8_t>(
                    _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address), static_cast<char>(target), static_cast<char>(expected)));
                accepted = observed == expected;
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::WriteFailure;
            }
        }
#if !defined(MTA_DE_CAMERA_JUMPCUT_NATIVE_INVOKE)
        ClockResult NativeJumpCutCall(NativeCameraRestoreCalls::Function function, std::uintptr_t camera, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(camera);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif

#if !defined(MTA_DE_CAMERA_RESTORE_NATIVE_INVOKE)
        ClockResult NativeCall(NativeCameraRestoreCalls::Function function, std::uintptr_t camera, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(camera);
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
    struct NativeCameraRestore::Data
    {
        CameraRestoreDetail::Snapshot         before, expected, observed, bookend;
        CameraRestoreDetail::Snapshot         prefix;
        CameraRestoreDetail::ProviderSnapshot providerBefore, providerObserved, providerBookend;
        NativeCameraRestoreCalls::Receipt     jumpCut;
    };
    NativeCameraRestore::NativeCameraRestore() noexcept = default;
    NativeCameraRestore::~NativeCameraRestore() = default;
    ClockResult NativeCameraRestore::Restore(std::uintptr_t image, const NativePlayerInfoMemory& players, CheckOwner owner,
                                             CheckOwner afterApplicationFrameOwner, Receipt* output, Invoke invoke) noexcept
    {
        namespace C = CameraRestoreDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraRestoreProfile;
        Receipt receipt;
        receipt.camera = image + P::CameraRva;
        const auto finish = [&](ClockResult result)
        {
            if (receipt.entered && result != ClockResult::Ok)
                debt.store(true);
            receipt.result = result;
            receipt.debt = debt.load();
            if (output)
                *output = receipt;
            return result;
        };
        if (busy.exchange(true))
            return finish(ClockResult::Busy);
        struct BusyScope
        {
            std::atomic<bool>& busy;
            ~BusyScope() { busy.store(false); }
        } busyScope{busy};
        if (debt.load())
            return finish(ClockResult::RestoreFailure);
        if (!owner || !afterApplicationFrameOwner)
            return finish(ClockResult::NotInitialized);
        auto result = afterApplicationFrameOwner();
        if (result != ClockResult::Ok)
            return finish(result);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        if (!C::Profile(image))
            return finish(ClockResult::UnsupportedBuild);
        try
        {
            data = std::make_unique<Data>();
        }
        catch (...)
        {
            return finish(ClockResult::WriteFailure);
        }
        if ((result = C::Capture(image, players, owner, data->before)) != ClockResult::Ok ||
            (result = C::Capture(image, players, owner, data->observed)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(data->before, data->observed))
            return finish(ClockResult::BindingChanged);
        data->expected = data->before;
        if (!C::Plan(image, data->expected))
            return finish(ClockResult::BindingChanged);
        receipt.player = data->before.player.nativePed;
        receipt.handle = data->before.player.handle;
        receipt.playerMode = data->before.mode;
        receipt.noEffect = data->before.noEffect;
        receipt.root = data->before.world.root;
        receipt.world = data->before.world.world;
        receipt.beforeHead = data->before.head;
        receipt.beforeFreeHead = data->before.freeHead;
        if ((result = afterApplicationFrameOwner()) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_CAMERA_RESTORE_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_CAMERA_RESTORE_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &C::NativeCall;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeCameraRestoreCalls::Function>(image + P::EntryRva), receipt.camera, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        if ((result = afterApplicationFrameOwner()) != ClockResult::Ok || (result = C::Capture(image, players, owner, data->observed)) != ClockResult::Ok ||
            (result = C::Capture(image, players, owner, data->bookend)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(data->observed, data->bookend) || !C::Same(data->expected, data->observed))
            return finish(ClockResult::BindingChanged);
        if ((result = afterApplicationFrameOwner()) != ClockResult::Ok)
            return finish(result);
        receipt.afterHead = data->observed.head;
        receipt.afterFreeHead = data->observed.freeHead;
        for (std::size_t index = 0; index != data->before.bank.size(); ++index)
            if (std::memcmp(&data->before.bank[index], &data->observed.bank[index], sizeof(C::Node)))
                ++receipt.changedReferenceNodes;
        receipt.effectsVerified = true;
        data.reset();
        return finish(ClockResult::Ok);
    }
    ClockResult NativeCameraRestore::RestoreWithJumpCut(std::uintptr_t image, const NativePlayerInfoMemory& players, CheckOwner owner,
                                                        CheckOwner afterApplicationFrameOwner, Receipt* output, Invoke invoke,
                                                        NativeCameraRestoreCalls::ExchangeBehind exchange) noexcept
    {
        namespace C = CameraRestoreDetail;
        namespace D = NativeStreamingCompletionContext;
        namespace P = NativeCameraRestoreProfile;
        Receipt receipt;
        receipt.operation = NativeCameraRestoreCalls::Operation::JumpCut;
        receipt.entryRva = P::JumpCutRva;
        receipt.camera = image + P::CameraRva;
        bool       journalActive = false;
        const auto save = [&]()
        {
            receipt.effectsEntered = receipt.entered || receipt.behindAccepted || (receipt.behindWriteEntered && !receipt.behindWriteReturned);
            if (journalActive)
                data->jumpCut = receipt;
        };
        const auto finish = [&](ClockResult result)
        {
            save();
            if (receipt.effectsEntered && result != ClockResult::Ok)
                debt.store(true);
            receipt.result = result;
            receipt.debt = debt.load();
            save();
            if (output)
                *output = receipt;
            return result;
        };
        if (busy.exchange(true))
            return finish(ClockResult::Busy);
        struct BusyScope
        {
            std::atomic<bool>& busy;
            ~BusyScope() { busy.store(false); }
        } busyScope{busy};
        if (debt.load())
            return finish(ClockResult::RestoreFailure);
        if (!owner || !afterApplicationFrameOwner)
            return finish(ClockResult::NotInitialized);
        auto result = afterApplicationFrameOwner();
        if (result != ClockResult::Ok)
            return finish(result);
        if (!D::TryEnter())
            return finish(ClockResult::Busy);
        D::Scope scope;
        if (!C::JumpCutProfile(image))
            return finish(ClockResult::UnsupportedBuild);
        try
        {
            data = std::make_unique<Data>();
        }
        catch (...)
        {
            return finish(ClockResult::WriteFailure);
        }
        journalActive = true;
        if ((result = C::Capture(image, players, owner, data->before, false)) != ClockResult::Ok ||
            (result = C::Provider(image, data->before, owner, data->providerBefore)) != ClockResult::Ok ||
            (result = C::Capture(image, players, owner, data->observed, false)) != ClockResult::Ok ||
            (result = C::Provider(image, data->observed, owner, data->providerObserved)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(data->before, data->observed) || !C::SameProvider(data->providerBefore, data->providerObserved))
            return finish(ClockResult::BindingChanged);
        data->prefix = data->before;
        C::Put(data->prefix, 0x22, std::uint8_t{1});
        data->expected = data->prefix;
        if (!C::PlanJumpCut(image, data->expected))
            return finish(ClockResult::BindingChanged);
        receipt.player = data->before.player.nativePed;
        receipt.handle = data->before.player.handle;
        receipt.playerMode = data->before.mode;
        receipt.root = data->before.world.root;
        receipt.world = data->before.world.world;
        receipt.beforeHead = data->before.head;
        receipt.beforeFreeHead = data->before.freeHead;
        receipt.behindBefore = C::Get<std::uint8_t>(data->before, 0x22);
        receipt.behindObserved = receipt.behindBefore;
        // Both source operations and their whole expected effects are prepared
        // before accepting even the first byte. This is the documented DE
        // named policy: its native follow-ped consumer computes yaw later.
        // No guessed classic this+120 cache or manual CCam yaw is published.
        if ((result = afterApplicationFrameOwner()) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
        if (receipt.behindBefore != 1)
        {
#if defined(MTA_DE_CAMERA_JUMPCUT_BEHIND_EXCHANGE)
            const auto write = exchange ? exchange : &MTA_DE_CAMERA_JUMPCUT_BEHIND_EXCHANGE;
#else
            const auto write = exchange ? exchange : &C::ExchangeBehind;
#endif
            receipt.behindWriteEntered = true;
            save();
            result = write(receipt.camera + 0x22, receipt.behindBefore, 1, receipt.behindObserved, receipt.behindAccepted, receipt.exceptionCode);
            receipt.behindWriteReturned = result == ClockResult::Ok;
            save();
            if (result != ClockResult::Ok)
                return finish(result);
            if (!receipt.behindAccepted)
                return finish(ClockResult::OwnerConflict);
        }
        if ((result = afterApplicationFrameOwner()) != ClockResult::Ok ||
            (result = C::Capture(image, players, owner, data->observed, false)) != ClockResult::Ok ||
            (result = C::Provider(image, data->observed, owner, data->providerObserved)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(data->prefix, data->observed) || !C::SameProvider(data->providerBefore, data->providerObserved))
            return finish(ClockResult::BindingChanged);
        receipt.behindVerified = true;
        save();
        if ((result = afterApplicationFrameOwner()) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_CAMERA_JUMPCUT_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_CAMERA_JUMPCUT_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &C::NativeJumpCutCall;
#endif
        receipt.entered = true;
        save();
        result = entry(reinterpret_cast<NativeCameraRestoreCalls::Function>(image + P::JumpCutRva), receipt.camera, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        save();
        if ((result = afterApplicationFrameOwner()) != ClockResult::Ok ||
            (result = C::Capture(image, players, owner, data->observed, false)) != ClockResult::Ok ||
            (result = C::Provider(image, data->observed, owner, data->providerObserved)) != ClockResult::Ok ||
            (result = C::Capture(image, players, owner, data->bookend, false)) != ClockResult::Ok ||
            (result = C::Provider(image, data->bookend, owner, data->providerBookend)) != ClockResult::Ok)
            return finish(result);
        if (!C::Same(data->observed, data->bookend) || !C::Same(data->expected, data->observed) ||
            !C::SameProvider(data->providerBefore, data->providerObserved) || !C::SameProvider(data->providerObserved, data->providerBookend))
            return finish(ClockResult::BindingChanged);
        if ((result = afterApplicationFrameOwner()) != ClockResult::Ok)
            return finish(result);
        receipt.afterHead = data->observed.head;
        receipt.afterFreeHead = data->observed.freeHead;
        for (std::size_t index = 0; index != data->before.bank.size(); ++index)
            if (std::memcmp(&data->before.bank[index], &data->observed.bank[index], sizeof(C::Node)))
                ++receipt.changedReferenceNodes;
        receipt.effectsVerified = true;
        const auto completed = finish(ClockResult::Ok);
        journalActive = false;
        data.reset();
        return completed;
    }
}
