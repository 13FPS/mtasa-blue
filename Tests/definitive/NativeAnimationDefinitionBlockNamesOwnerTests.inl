/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual definition BlockName borrows and native fixed118 first-match SDK policy
 ****************************************************************************/
namespace AnimationDefinitionBlockNamesOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationManagerOwnerTest;
    std::uintptr_t                changedAddress = 0;
    std::array<unsigned char, 40> replacement{};
    std::string*                  callerName = nullptr;
    std::uint64_t*                callerGeneration = nullptr;
    bool                          changed = false;
    void                          ChangeCaller() noexcept
    {
        (*callerName)[0] = 'X';
        *callerGeneration = 0;
    }
    void ChangePrefix() noexcept
    {
        std::memcpy(reinterpret_cast<void*>(changedAddress), replacement.data(), replacement.size());
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
        // banks. Model only its exact two-page prefix as RW in the synthetic PE; changing
        // VirtualProtect alone cannot satisfy the shipping section qualifier.
        // Keep surrounding immutable dispatch slots/literals in their RO spans.
        constexpr auto definitionPage = NativeAnimationAssocDefinitionProfile::BankRva & ~std::uintptr_t{0xfff};
        constexpr auto definitionSpan = ((NativeAnimationAssocDefinitionProfile::BankRva +
                                          NativeAnimationAssociationGroupPrefixCount * NativeAnimationAssocDefinitionProfile::RowBytes + 0xfff) &
                                         ~std::uintptr_t{0xfff}) -
                                        definitionPage;
        const auto dos = fixture.Get<IMAGE_DOS_HEADER>(0);
        const auto nt = fixture.Get<IMAGE_NT_HEADERS64>(static_cast<std::uintptr_t>(dos.e_lfanew));
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
            if (section.VirtualAddress < definitionPage && end > definitionPage + definitionSpan)
            {
                Check(!split && !section.SizeOfRawData && (section.Characteristics & IMAGE_SCN_MEM_READ) &&
                          !(section.Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE)),
                      "the new row page splits only its existing synthetic read-only zero section");
                auto before = section, data = section, after = section;
                before.Misc.VirtualSize = static_cast<DWORD>(definitionPage - section.VirtualAddress);
                data.VirtualAddress = static_cast<DWORD>(definitionPage);
                data.Misc.VirtualSize = static_cast<DWORD>(definitionSpan);
                data.Characteristics |= IMAGE_SCN_MEM_WRITE;
                after.VirtualAddress = static_cast<DWORD>(definitionPage + definitionSpan);
                after.Misc.VirtualSize = static_cast<DWORD>(end - definitionPage - definitionSpan);
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
        for (const auto& pin : P::BlockNameCode)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        saved.Global(P::GroupSlotRva, fixture.image + P::GroupRva, PAGE_READONLY);
        saved.Global(P::CountRva, static_cast<std::int32_t>(NativeAnimationAssociationGroupPrefixCount));
        std::array<std::array<unsigned char, P::RowBytes>, NativeAnimationAssociationGroupPrefixCount> rows{};
        for (std::uint32_t index = 0; index != rows.size(); ++index)
        {
            std::snprintf(reinterpret_cast<char*>(rows[index].data()), 16, "g%03u", index);
            std::snprintf(reinterpret_cast<char*>(rows[index].data() + P::BlockNameOffset), P::BlockNameBytes, "block%03u", index);
        }
        std::memset(rows[4].data() + P::BlockNameOffset, 0, P::BlockNameBytes);
        std::memcpy(rows[4].data() + P::BlockNameOffset, "PeD", 4);
        std::memset(rows[7].data() + P::BlockNameOffset, 0, P::BlockNameBytes);
        std::memcpy(rows[7].data() + P::BlockNameOffset, "ped", 4);
        const unsigned char highA[]{0xe9, '_', 'P', 'E', 'D', 0}, highB[]{0xc9, '_', 'P', 'E', 'D', 0};
        std::memset(rows[10].data() + P::BlockNameOffset, 0, P::BlockNameBytes);
        std::memcpy(rows[10].data() + P::BlockNameOffset, highA, sizeof(highA));
        std::memset(rows[11].data() + P::BlockNameOffset, 0, P::BlockNameBytes);
        std::memcpy(rows[11].data() + P::BlockNameOffset, highB, sizeof(highB));
        WorldGroundOwnerFixture::Writes::Apply(&saved, P::BankRva, rows.data(), sizeof(rows), PAGE_READWRITE);
        const auto                             generation = runtime.generation.load();
        PrivateAnimationFactoryManagerReceiver manager(runtime.access, generation);
        CAnimManager&                          sdk = manager;
        Hook::watched = 0;
        Hook::watchedBytes = sizeof(std::int32_t);
        Hook::reads = 0;
        Hook::failNext = false;
        Hook::afterRead = nullptr;
        const auto field = [&](std::uint32_t index) { return fixture.image + P::BankRva + index * P::RowBytes + P::BlockNameOffset; };
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
                    std::fprintf(stderr, "Definition block-name action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew the real callback owner for each bounded definition block-name operation");
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
            return !std::memcmp(rows.data(), reinterpret_cast<void*>(fixture.image + P::BankRva), sizeof(rows)) && !host.mallocCalls && !host.freeCalls &&
                   !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained();
        };
        for (const auto index : {0u, 4u, 11u, 117u})
        {
            action("actual current definition BlockName borrow",
                   [&]
                   {
                       std::uintptr_t output = 0x12345678;
                       Check(runtime.access.ReadAnimationGroupBlockName(generation, index, output) == ClockResult::Ok && output == field(index) &&
                                 reinterpret_cast<std::uintptr_t>(sdk.GetAnimBlockName(static_cast<AssocGroupId>(index))) == field(index) && unchanged(),
                             "source block-name getter borrows exact current row+16 through actual observer-only manager without layout/cache/facade adoption");
                   });
        }
        const std::array<std::pair<std::string, std::uint32_t>, 7> requests{{{"pEd", 4},
                                                                             {"block117", 117},
                                                                             {std::string(reinterpret_cast<const char*>(highA)), 10},
                                                                             {std::string(reinterpret_cast<const char*>(highB)), 11},
                                                                             {std::string("\xc8_pEd"), 118},
                                                                             {"", 118},
                                                                             {std::string(200, 'z'), 118}}};
        for (const auto& request : requests)
        {
            action("native exact fixed118 firstmatch or default",
                   [&]
                   {
                       std::uint32_t output = 0xdeadbeef;
                       Check(runtime.access.FindFirstAnimationGroupByBlockName(generation, request.first, output) == ClockResult::Ok &&
                                 output == request.second && unchanged(),
                             "native lookup preserves first ASCII-fold match, unequal-high-byte distinction, full unbounded query and exact missing118");
                   });
            action("actual SDK first association group source policy",
                   [&]
                   {
                       Check(static_cast<std::uint32_t>(sdk.GetFirstAssocGroup(request.first.c_str())) == request.second && unchanged(),
                             "actual manager applies the existing source first-group policy without a new association or complete game provider");
                   });
        }
        for (const auto count : {std::int32_t{0}, std::int32_t{-1}})
        {
            saved.Global(P::CountRva, count);
            action("fixed native prefix is independent of mutable current count",
                   [&]
                   {
                       Hook::watched = fixture.image + P::CountRva;
                       Hook::watchedBytes = sizeof(std::int32_t);
                       Hook::reads = 0;
                       std::uint32_t output = 0xdeadbeef;
                       Check(runtime.access.FindFirstAnimationGroupByBlockName(generation, std::string("block117"), output) == ClockResult::Ok &&
                                 output == 117 && !Hook::reads,
                             "genuine FF1070 fixed prefix never substitutes or observes the unrelated current definition count value");
                       std::uintptr_t name = 0x12345678;
                       Check(runtime.access.ReadAnimationGroupBlockName(generation, 0, name) == ClockResult::NotInitialized && name == 0x12345678,
                             "current-extent block-name getter remains separate from the fixed-prefix search contract");
                       Hook::watched = 0;
                   });
        }
        saved.Global(P::CountRva, static_cast<std::int32_t>(NativeAnimationAssociationGroupPrefixCount));
        action(
            "source NUL semantics survive internal copied transport",
            [&]
            {
                std::uint32_t output = 0xdeadbeef;
                Check(runtime.access.FindFirstAnimationGroupByBlockName(generation, std::string("PeD\0ignored", 11), output) == ClockResult::Ok && output == 4,
                      "the native source string ends at NUL even if the internal copied transport contains later CPU bytes");
                Check(refuses([&] { static_cast<void>(sdk.GetFirstAssocGroup(nullptr)); }), "actual SDK rejects null input before inventing a lookup result");
            });
        action("full group ID and generation refuse without changing output",
               [&]
               {
                   std::uintptr_t name = 0x12345678;
                   std::uint32_t  selected = 0xdeadbeef;
                   Check(runtime.access.ReadAnimationGroupBlockName(generation, 118, name) == ClockResult::InvalidArgument && name == 0x12345678 &&
                             runtime.access.ReadAnimationGroupBlockName(generation, 65536, name) == ClockResult::InvalidArgument && name == 0x12345678 &&
                             runtime.access.ReadAnimationGroupBlockName(generation, 0xffffffffu, name) == ClockResult::InvalidArgument && name == 0x12345678 &&
                             runtime.access.FindFirstAnimationGroupByBlockName(generation + 1, std::string("PeD"), selected) == ClockResult::OwnerConflict &&
                             selected == 0xdeadbeef,
                         "full32 group identity cannot wrap and stale-generation lookup cannot publish a missing/default sentinel");
               });
        action("copy CPU query and owner intent before callbacks",
               [&]
               {
                   std::string   query = "PeD";
                   auto          sourceGeneration = generation;
                   std::uint32_t output = 0xdeadbeef;
                   callerName = &query;
                   callerGeneration = &sourceGeneration;
                   Hook::watched = field(0);
                   Hook::watchedBytes = P::BlockNameBytes;
                   Hook::afterRead = &ChangeCaller;
                   const auto result = runtime.access.FindFirstAnimationGroupByBlockName(sourceGeneration, query, output);
                   Hook::afterRead = nullptr;
                   callerName = nullptr;
                   callerGeneration = nullptr;
                   Check(result == ClockResult::Ok && output == 4 && query[0] == 'X' && !sourceGeneration && unchanged(),
                         "actual native provider uses copied unbounded source spelling/generation throughout its callbacks");
               });
        for (const auto target : {4u, 117u})
        {
            action("changed searched native prefix refuses",
                   [&]
                   {
                       const std::string query = target == 4 ? "PeD" : "absent";
                       Hook::watched = field(target);
                       Hook::watchedBytes = P::BlockNameBytes;
                       Hook::reads = 0;
                       Hook::afterRead = &ChangePrefix;
                       replacement.fill(0);
                       std::memcpy(replacement.data(), query.c_str(), query.size() + 1);
                       changedAddress = field(0);
                       changed = false;
                       std::uint32_t output = 0xdeadbeef;
                       const auto    result = runtime.access.FindFirstAnimationGroupByBlockName(generation, query, output);
                       Hook::afterRead = nullptr;
                       fixture.Put(P::BankRva, rows[0]);
                       Check(changed && result == ClockResult::BindingChanged && output == 0xdeadbeef && unchanged(),
                             "every earlier searched block spelling is compared on both firstmatch and full118 miss before result publication");
                   });
        }
        action("failed actual native BlockName read is refusal",
               [&]
               {
                   Hook::watched = field(0);
                   Hook::watchedBytes = P::BlockNameBytes;
                   Hook::failNext = true;
                   std::uintptr_t name = 0x12345678;
                   Check(runtime.access.ReadAnimationGroupBlockName(generation, 0, name) == ClockResult::ReadFailure && name == 0x12345678 &&
                             !runtime.stopping.load(),
                         "a guarded native payload read failure cannot become successful null or a copied CPU name");
               });
        action("final actual owner bookend is mandatory",
               [&]
               {
                   const auto previous = timingCallbackGeneration;
                   Hook::watched = field(4);
                   Hook::watchedBytes = P::BlockNameBytes;
                   Hook::reads = 0;
                   Hook::afterRead = &LoseFinalOwner;
                   changed = false;
                   std::uint32_t output = 0xdeadbeef;
                   const auto    result = runtime.access.FindFirstAnimationGroupByBlockName(generation, std::string("PeD"), output);
                   Hook::afterRead = nullptr;
                   timingCallbackGeneration = previous;
                   Check(changed && result == ClockResult::OwnerConflict && output == 0xdeadbeef && unchanged(),
                         "last source owner loss preserves lookup output without native mutation/debt");
               });
        Hook::watched = 0;
        Hook::watchedBytes = sizeof(std::int32_t);
        auto wrongCode = P::FirstGroupCode;
        wrongCode[0] ^= 1;
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xff1070, wrongCode.data(), wrongCode.size(), PAGE_EXECUTE_READ);
        action("new qualification does not alter old definition channels",
               [&]
               {
                   std::uintptr_t name = 0x12345678;
                   std::uint32_t  selected = 0xdeadbeef;
                   std::int32_t   count = 0;
                   Check(runtime.access.ReadAnimationGroupBlockName(generation, 0, name) == ClockResult::BindingChanged && name == 0x12345678 &&
                             runtime.access.FindFirstAnimationGroupByBlockName(generation, std::string("PeD"), selected) == ClockResult::BindingChanged &&
                             selected == 0xdeadbeef && runtime.access.ReadAnimationAssocDefinitionCount(generation, count) == ClockResult::Ok && count == 118 &&
                             runtime.access.ReadAnimationGroupName(generation, 0, name) == ClockResult::Ok && name == fixture.image + P::BankRva && unchanged(),
                         "only new block-name operations acquire FF1070 proof; existing count/group-name observations retain their actual prerequisites");
               });
        WorldGroundOwnerFixture::Writes::Apply(&saved, 0xff1070, P::FirstGroupCode.data(), P::FirstGroupCode.size(), PAGE_EXECUTE_READ);
        Check(refuses([&] { static_cast<void>(sdk.GetAnimBlockName(static_cast<AssocGroupId>(0))); }) &&
                  refuses([&] { static_cast<void>(sdk.GetFirstAssocGroup("PeD")); }) && unchanged(),
              "CPU observer facade grants neither query callback authority nor native association lifetime between callbacks");
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::watched = 0;
        Hook::watchedBytes = sizeof(std::int32_t);
        WorldGroundOwnerFixture::Retire(service, fixture);
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production animation definition block names owner fixture completed");
        std::puts("Production actual definition block name SDK fixture completed");
    }
}
