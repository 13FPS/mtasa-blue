/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual signed definition count and immediate native group-name SDK observations
 ****************************************************************************/
namespace AnimationDefinitionsOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationManagerOwnerTest;
    std::uintptr_t changedAddress = 0;
    std::uint64_t* callerGeneration = nullptr;
    std::uint32_t* callerIndex = nullptr;
    bool           changed = false;
    void           ChangeCaller() noexcept
    {
        *callerGeneration = 0;
        *callerIndex = 0xffffffffu;
    }
    void ChangeNameAfterPayload() noexcept
    {
        if (Hook::reads < 2)
        {
            Hook::afterRead = &ChangeNameAfterPayload;
            return;
        }
        *reinterpret_cast<unsigned char*>(changedAddress) ^= 1;
        changed = true;
    }
    void LoseFinalOwner() noexcept
    {
        if (Hook::reads < 2)
        {
            Hook::afterRead = &LoseFinalOwner;
            return;
        }
        ++timingCallbackGeneration;
        changed = true;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        // The new real definition bank is earlier than the old writable clock
        // banks. Model only its exact page as RW in the synthetic PE; changing
        // VirtualProtect alone cannot satisfy the shipping section qualifier.
        // Keep surrounding immutable dispatch slots/literals in their RO spans.
        constexpr auto definitionPage = NativeAnimationAssocDefinitionProfile::BankRva & ~std::uintptr_t{0xfff};
        const auto     dos = fixture.Get<IMAGE_DOS_HEADER>(0);
        const auto     nt = fixture.Get<IMAGE_NT_HEADERS64>(static_cast<std::uintptr_t>(dos.e_lfanew));
        const auto sectionRva = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        std::vector<IMAGE_SECTION_HEADER> oldSections(nt.FileHeader.NumberOfSections), sections;
        SIZE_T                            copied = 0;
        Check(ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + sectionRva), oldSections.data(),
                                oldSections.size() * sizeof(IMAGE_SECTION_HEADER), &copied) &&
                  copied == oldSections.size() * sizeof(IMAGE_SECTION_HEADER),
              "read actual owned PE geometry before admitting the bounded definition data page");
        bool split = false;
        for (const auto& section : oldSections)
        {
            const auto end = static_cast<std::uintptr_t>(section.VirtualAddress) + section.Misc.VirtualSize;
            if (section.VirtualAddress < definitionPage && end > definitionPage + 0x1000)
            {
                Check(!split && !section.SizeOfRawData && (section.Characteristics & IMAGE_SCN_MEM_READ) &&
                          !(section.Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)),
                      "the new row page splits only its existing synthetic read-only zero section");
                auto before = section, data = section, after = section;
                before.Misc.VirtualSize = static_cast<DWORD>(definitionPage - section.VirtualAddress);
                data.VirtualAddress = static_cast<DWORD>(definitionPage);
                data.Misc.VirtualSize = 0x1000;
                data.Characteristics |= IMAGE_SCN_MEM_WRITE;
                after.VirtualAddress = static_cast<DWORD>(definitionPage + 0x1000);
                after.Misc.VirtualSize = static_cast<DWORD>(end - definitionPage - 0x1000);
                sections.push_back(before);
                sections.push_back(data);
                sections.push_back(after);
                split = true;
            }
            else
                sections.push_back(section);
        }
        Check(
            split && sections.size() == oldSections.size() + 2 && sections.size() < 96 && sectionRva + sections.size() * sizeof(IMAGE_SECTION_HEADER) <= 0x1000,
            "bounded RO/RW/RO split retains adjacent virtual ranges inside the original owned header page");
        const auto sectionCount = static_cast<WORD>(sections.size());
        saved.Global(static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, FileHeader) + offsetof(IMAGE_FILE_HEADER, NumberOfSections),
                     sectionCount, PAGE_READONLY);
        WorldGroundOwnerFixture::Writes::Apply(&saved, sectionRva, sections.data(), sections.size() * sizeof(IMAGE_SECTION_HEADER), PAGE_READONLY);

        const auto admit = [&](const auto& pins)
        {
            for (const auto& pin : pins)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        };
        for (const auto& pin : NativeStreamingLoadAllProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeStartupOwnerProfile::pins);
        admit(NativeStartupWorldProfile::pins);
        for (const auto& pin : NativeUObjectMembershipProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        admit(NativeGameStateProfile::pins);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        alignas(std::uintptr_t) std::array<unsigned char, 0x180> root{};
        NativeStreamingLoadAllFixture::Graph graph(fixture.image, reinterpret_cast<std::uintptr_t>(root.data()), &WorldGroundOwnerFixture::Writes::Apply,
                                                   &saved);
        NativePedClothesFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        NativePedClothesFixture::NameState names;
        names.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::State host;
        host.Reset(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        Host::active = &host;
        for (const auto& pin : NativeAnimationSequenceRowProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        // Cleanup exercises the real fresh frame producers, so their existing
        // instruction profile must be installed before binding either row.
        for (const auto& pin : NativeAnimationSequenceFrameBindProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeAnimationSequenceNameProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        NativeAnimationBlockFactoryFixture::Install(fixture.image, &WorldGroundOwnerFixture::Writes::Apply, &saved);
        for (const auto& pin : NativeAnimationHierarchyReadProfile::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xfec67a, private_hierarchy_fixture::CacheProof, sizeof(private_hierarchy_fixture::CacheProof),
                                               PAGE_EXECUTE_READ);
        graph.Reset();
        saved.Global(NativeGameStateProfile::stateRva, std::uint32_t{9});
        saved.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(1));
        saved.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId());
        saved.Global(NativeBorrowedGcProfile::markerRva, LONG{0});
        const auto previousImage = ClockReaderTestHost::image;
        ClockReaderTestHost::image = fixture.image;
        WorldGroundOwnerFixture::Callback callback;
        NativeClockService                service;
        callback.action = [](NativeGameTimingAccess&) {};
        BeginOwned(service, fixture, false, true, true, &WorldGroundOwnerFixture::Callback::Invoke, &callback);
        // BeginOwned replaces admission before shipping's module-pin step.
        // This scope tests that lifetime too, so pin the real callback code.
        HMODULE cleanupModule = nullptr;
        Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&HookClockContinuation),
                                 &cleanupModule) &&
                  cleanupModule,
              "cleanup fixture obtains the genuine process-lifetime pin for its compiled callback module");
        runtime.modulePinned.store(true);
        namespace P = NativeAnimationAssocDefinitionProfile;
        for (const auto& pin : P::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        saved.Global(P::GroupSlotRva, fixture.image + P::GroupRva, PAGE_READONLY);
        saved.Global(P::CountRva, std::int32_t{3});
        std::array<std::array<unsigned char, P::RowBytes>, 3> rows{};
        const unsigned char                                   first[]{'p', 'e', 'd', 0};
        const unsigned char                                   second[]{'R', 'A', 'W', 0xe9, 0};
        const unsigned char                                   last[]{'l', 'a', 's', 't', 0};
        std::memcpy(rows[0].data(), first, sizeof(first));
        std::memcpy(rows[1].data(), second, sizeof(second));
        std::memcpy(rows[2].data(), last, sizeof(last));
        for (std::size_t i = 0; i != rows.size(); ++i)
            WorldGroundOwnerFixture::Writes::Apply(&saved, P::BankRva + i * P::RowBytes, rows[i].data(), rows[i].size(), PAGE_READWRITE);
        const auto                             generation = runtime.generation.load();
        PrivateAnimationFactoryManagerReceiver manager(runtime.access, generation);
        CAnimManager&                          sdk = manager;
        Hook::watched = fixture.image + P::CountRva;
        Hook::reads = 0;
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        const auto action = [&](const char* label, const auto& body)
        {
            callback.action = [&](NativeGameTimingAccess&)
            {
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    std::fprintf(stderr, "Animation definition action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew only the current definition observation callback owner");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto refuses = [](const auto& call)
        {
            try
            {
                call();
            }
            catch (const std::exception&)
            {
                return true;
            }
            return false;
        };
        const auto unchanged = [&]
        {
            for (std::size_t i = 0; i != rows.size(); ++i)
                if (std::memcmp(rows[i].data(), reinterpret_cast<void*>(fixture.image + P::BankRva + i * P::RowBytes), P::RowBytes))
                    return false;
            return !host.mallocCalls && !host.freeCalls && !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained();
        };
        for (const auto value :
             {(std::numeric_limits<std::int32_t>::min)(), std::int32_t{-1}, std::int32_t{0}, std::int32_t{1}, (std::numeric_limits<std::int32_t>::max)()})
        {
            saved.Global(P::CountRva, value);
            action("raw signed definition count through actual SDK",
                   [&]
                   {
                       std::int32_t observed = 0x13579bdf;
                       Check(runtime.access.ReadAnimationAssocDefinitionCount(generation, observed) == ClockResult::Ok && observed == value &&
                                 sdk.GetNumAnimAssocDefinitions() == value && unchanged(),
                             "definition count preserves all signed32 bits without borrowing name-range, block-count or normalization policy");
                   });
        }
        saved.Global(P::CountRva, std::int32_t{3});
        const char* borrowed = nullptr;
        for (std::uint32_t index = 0; index != 3; ++index)
        {
            action("actual current definition group-name borrow",
                   [&]
                   {
                       std::uintptr_t address = 0x12345678;
                       Check(runtime.access.ReadAnimationGroupName(generation, index, address) == ClockResult::Ok &&
                                 address == fixture.image + P::BankRva + index * P::RowBytes,
                             "native group name resolves exactly the selected current56-byte definition row");
                       borrowed = sdk.GetAnimGroupName(static_cast<AssocGroupId>(index));
                       Check(reinterpret_cast<std::uintptr_t>(borrowed) == address &&
                                 std::memcmp(borrowed, rows[index].data(),
                                             index == 0   ? sizeof(first)
                                             : index == 1 ? sizeof(second)
                                                          : sizeof(last)) == 0 &&
                                 unchanged(),
                             "actual observer-only manager returns the immediate raw native NUL spelling, including high bytes, without copy/cache/facade");
                   });
        }
        action("out-of-domain indices preserve native output",
               [&]
               {
                   std::uintptr_t address = 0x12345678;
                   Check(runtime.access.ReadAnimationGroupName(generation, 3, address) == ClockResult::InvalidArgument && address == 0x12345678 &&
                             runtime.access.ReadAnimationGroupName(generation, 0xffffffffu, address) == ClockResult::InvalidArgument && address == 0x12345678 &&
                             refuses([&] { static_cast<void>(sdk.GetAnimGroupName(static_cast<AssocGroupId>(0xffffffffu))); }) && unchanged(),
                         "full uint32 IDs are range-checked before row arithmetic and never wrap into an earlier definition");
               });
        action("nonpositive definition count is no name range",
               [&]
               {
                   fixture.Put(P::CountRva, std::int32_t{0});
                   std::uintptr_t address = 0x12345678;
                   const auto     result = runtime.access.ReadAnimationGroupName(generation, 0, address);
                   fixture.Put(P::CountRva, std::int32_t{3});
                   Check(result == ClockResult::NotInitialized && address == 0x12345678 && unchanged(),
                         "a valid raw zero count cannot supply a fabricated native name pointer");
               });
        action("owner generation preserves every output",
               [&]
               {
                   std::int32_t   count = 0x13579bdf;
                   std::uintptr_t address = 0x12345678;
                   Check(runtime.access.ReadAnimationAssocDefinitionCount(generation + 1, count) == ClockResult::OwnerConflict && count == 0x13579bdf &&
                             runtime.access.ReadAnimationGroupName(generation + 1, 0, address) == ClockResult::OwnerConflict && address == 0x12345678,
                         "wrong generation cannot publish current count or a group-name borrow");
               });
        action("copied generation and full index intent",
               [&]
               {
                   auto           sourceGeneration = generation;
                   std::uint32_t  sourceIndex = 1;
                   std::uintptr_t address = 0x12345678;
                   callerGeneration = &sourceGeneration;
                   callerIndex = &sourceIndex;
                   Hook::afterRead = &ChangeCaller;
                   const auto result = runtime.access.ReadAnimationGroupName(sourceGeneration, sourceIndex, address);
                   callerGeneration = nullptr;
                   callerIndex = nullptr;
                   Check(result == ClockResult::Ok && address == fixture.image + P::BankRva + P::RowBytes && !sourceGeneration && sourceIndex == 0xffffffffu &&
                             unchanged(),
                         "actual owner copies generation/index before a native count-read callback can mutate caller arguments");
               });
        action("name changed between native bookends refuses",
               [&]
               {
                   Hook::reads = 0;
                   changedAddress = fixture.image + P::BankRva;
                   changed = false;
                   Hook::afterRead = &ChangeNameAfterPayload;
                   std::uintptr_t address = 0x12345678;
                   const auto     result = runtime.access.ReadAnimationGroupName(generation, 0, address);
                   Hook::afterRead = nullptr;
                   fixture.Put(P::BankRva, rows[0]);
                   Check(changed && result == ClockResult::BindingChanged && address == 0x12345678 && unchanged(),
                         "changed actual native NUL spelling cannot become a successful stale borrow");
               });
        action("unterminated owned row refuses without cross-row read",
               [&]
               {
                   std::array<unsigned char, P::RowBytes> noTerminator;
                   noTerminator.fill(0xa5);
                   fixture.Put(P::BankRva, noTerminator);
                   std::uintptr_t address = 0x12345678;
                   const auto     result = runtime.access.ReadAnimationGroupName(generation, 0, address);
                   fixture.Put(P::BankRva, rows[0]);
                   Check(result == ClockResult::BindingChanged && address == 0x12345678 && unchanged(),
                         "name qualification stays inside the genuine current row span instead of guessing an SA field size or adopting a neighbouring string");
               });
        action("failed scalar read stays refusal",
               [&]
               {
                   Hook::failNext = true;
                   std::int32_t count = 0x13579bdf;
                   Check(runtime.access.ReadAnimationAssocDefinitionCount(generation, count) == ClockResult::ReadFailure && count == 0x13579bdf &&
                             !runtime.stopping.load(),
                         "failed definition count observation preserves output instead of returning synthetic zero");
               });
        action("lost final generation preserves signed count",
               [&]
               {
                   const auto previous = timingCallbackGeneration;
                   Hook::reads = 0;
                   changed = false;
                   Hook::afterRead = &LoseFinalOwner;
                   std::int32_t count = 0x13579bdf;
                   const auto   result = runtime.access.ReadAnimationAssocDefinitionCount(generation, count);
                   Hook::afterRead = nullptr;
                   timingCallbackGeneration = previous;
                   Check(changed && result == ClockResult::OwnerConflict && count == 0x13579bdf && unchanged(),
                         "last owner bookend is required before a raw definition count is published");
               });
        Check(refuses([&] { static_cast<void>(sdk.GetNumAnimAssocDefinitions()); }) &&
                  refuses([&] { static_cast<void>(sdk.GetAnimGroupName(static_cast<AssocGroupId>(0))); }) && unchanged(),
              "observer CPU manager does not lend engine-callback authority to count or name outside its current admission");
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::watched = 0;
        WorldGroundOwnerFixture::Retire(service, fixture);
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production animation definitions owner fixture completed");
        std::puts("Production actual animation definition SDK fixture completed");
    }
}
