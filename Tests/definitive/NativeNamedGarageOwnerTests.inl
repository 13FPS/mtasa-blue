/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual named garage SDK properties inside the same retained native callback
 ****************************************************************************/
namespace
{
    struct NamedGarageOwnerContext
    {
        NativeGarageGeometry geometryOutput;
        CVector              geometryPosition{123, 456, 789};
        std::array<float, 4> geometryDirection{}, geometryBounds{};
        std::array<float, 3> geometrySize{};
        NativeGarageName     name{'B', 'e', 'T', 'a', 0, 0, 0, 0};
        CGarage*             sdk = nullptr;
        ClockResult          result = ClockResult::NotInitialized;
        unsigned             operation = 0, refusal = 0;
        std::uint8_t         type = 255, typeOutput = 0xe7;
        bool                 open = true, openOutput = false, threw = false;
    };
    void NamedGarageOwnerHandler(NativeGameTimingAccess& access, void* opaque)
    {
        auto&      context = *static_cast<NamedGarageOwnerContext*>(opaque);
        const auto generation = timingCallbackGeneration;
        const auto admission = runtime.admissionGeneration.load();
        const auto refreshed = runtime.refreshed.load();
        if (context.refusal == 1)
            ++timingCallbackGeneration;
        if (context.refusal == 2)
            runtime.admissionGeneration.store(admission + 1);
        if (context.refusal == 3)
            ++callbackDepth;
        if (context.refusal == 4)
            runtime.refreshed.store(GetTickCount64() - ClockServiceLeaseMsV1 - 1);
        context.threw = false;
        try
        {
            switch (context.operation)
            {
                case 0:
                    context.result = access.ReadNamedGarageType(context.name, context.typeOutput);
                    break;
                case 1:
                    context.result = access.SetNamedGarageType(context.name, context.type);
                    break;
                case 2:
                    context.result = access.ReadNamedGarageOpen(context.name, context.openOutput);
                    break;
                case 3:
                    context.result = access.SetNamedGarageOpen(context.name, context.open);
                    break;
                case 4:
                    context.typeOutput = context.sdk->GetType();
                    context.result = ClockResult::Ok;
                    break;
                case 5:
                    context.sdk->SetType(context.type);
                    context.result = ClockResult::Ok;
                    break;
                case 6:
                    context.openOutput = context.sdk->IsOpen();
                    context.result = ClockResult::Ok;
                    break;
                case 7:
                    context.sdk->SetOpen(context.open);
                    context.result = ClockResult::Ok;
                    break;
                case 8:
                    context.result = access.ReadNamedGarageGeometry(context.name, context.geometryOutput);
                    break;
                case 9:
                    context.sdk->GetPosition(context.geometryPosition);
                    context.result = ClockResult::Ok;
                    break;
                case 10:
                    context.sdk->GetDirection(context.geometryDirection[0], context.geometryDirection[1], context.geometryDirection[2],
                                              context.geometryDirection[3]);
                    context.result = ClockResult::Ok;
                    break;
                case 11:
                    context.sdk->GetSize(context.geometrySize[0], context.geometrySize[1], context.geometrySize[2]);
                    context.result = ClockResult::Ok;
                    break;
                case 12:
                    context.sdk->GetBoundingBox(context.geometryBounds[0], context.geometryBounds[1], context.geometryBounds[2], context.geometryBounds[3]);
                    context.result = ClockResult::Ok;
                    break;
                default:
                    std::abort();
            }
        }
        catch (const std::runtime_error&)
        {
            context.threw = true;
            context.result = ClockResult::WriteFailure;
        }
        runtime.refreshed.store(refreshed);
        if (context.refusal == 3)
            --callbackDepth;
        runtime.admissionGeneration.store(admission);
        timingCallbackGeneration = generation;
    }
    void NativeNamedGaragesOwnedSdk(ClockImageFixture& fixture)
    {
        namespace G = NativeNamedGarageProfile;
        struct Saved
        {
            std::uintptr_t             rva;
            std::vector<unsigned char> bytes;
            DWORD                      protection;
        };
        std::vector<Saved> saved;
        const auto         admit = [&](std::uintptr_t rva, const void* bytes, std::size_t size, DWORD protection)
        {
            MEMORY_BASIC_INFORMATION page{};
            Check(::VirtualQuery(reinterpret_cast<void*>(fixture.image + rva), &page, sizeof(page)) == sizeof(page),
                  "query actual owned image protection before named garage admission");
            Saved  old{rva, std::vector<unsigned char>(size), page.Protect};
            SIZE_T copied = 0;
            Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + rva), old.bytes.data(), size, &copied) && copied == size,
                  "save only bounded named garage profile/count/selected row facts");
            saved.push_back(std::move(old));
            fixture.Patch(rva, bytes, size);
            DWORD prior = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + rva), size, protection, &prior) != FALSE,
                  "admit exact named garage code/constants/data with genuine OS image protection");
        };
        // Give only the count page its own writable PE section when the
        // generic clock image has a larger immutable gap here. This qualifies
        // a known field, never a native garage capacity inferred from a gap.
        const auto oldNt = fixture.Get<IMAGE_NT_HEADERS64>(0x100);
        const auto sectionsRva = std::uintptr_t{0x100} + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + oldNt.FileHeader.SizeOfOptionalHeader;
        std::array<IMAGE_SECTION_HEADER, 96> originalSections{};
        Check(oldNt.FileHeader.NumberOfSections && oldNt.FileHeader.NumberOfSections < originalSections.size(), "owned fixture section table is bounded");
        SIZE_T copied = 0;
        Check(::ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(fixture.image + sectionsRva), originalSections.data(),
                                  oldNt.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER), &copied) &&
                  copied == oldNt.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER),
              "read the current owned PE section headers");
        std::vector<IMAGE_SECTION_HEADER> sections;
        const auto                        countPage = static_cast<DWORD>(G::CountRva & ~std::uintptr_t{0xfff});
        for (unsigned i = 0; i != oldNt.FileHeader.NumberOfSections; ++i)
        {
            const auto& section = originalSections[i];
            if (G::CountRva < section.VirtualAddress || G::CountRva - section.VirtualAddress >= section.Misc.VirtualSize ||
                (section.Characteristics & IMAGE_SCN_MEM_WRITE))
            {
                sections.push_back(section);
                continue;
            }
            Check(!section.SizeOfRawData && countPage >= section.VirtualAddress && countPage + 0x1000 <= section.VirtualAddress + section.Misc.VirtualSize,
                  "split only the existing zero-filled section containing the exact count page");
            if (countPage != section.VirtualAddress)
            {
                auto before = section;
                before.Misc.VirtualSize = countPage - section.VirtualAddress;
                sections.push_back(before);
            }
            auto countSection = section;
            std::memcpy(countSection.Name, ".grgc\0\0\0", 8);
            countSection.VirtualAddress = countPage;
            countSection.Misc.VirtualSize = 0x1000;
            countSection.Characteristics |= IMAGE_SCN_MEM_WRITE;
            sections.push_back(countSection);
            if (countPage + 0x1000 != section.VirtualAddress + section.Misc.VirtualSize)
            {
                auto after = section;
                after.VirtualAddress = countPage + 0x1000;
                after.Misc.VirtualSize = section.VirtualAddress + section.Misc.VirtualSize - after.VirtualAddress;
                sections.push_back(after);
            }
        }
        Check(sections.size() <= originalSections.size(), "the named count needs at most two additional owned headers");
        auto nt = oldNt;
        nt.FileHeader.NumberOfSections = static_cast<WORD>(sections.size());
        admit(0x100, &nt, sizeof(nt), PAGE_READONLY);
        admit(sectionsRva, sections.data(), sections.size() * sizeof(IMAGE_SECTION_HEADER), PAGE_READONLY);
        for (const auto& pin : NativePedPoolProfile::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : G::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeNamedGarageGeometryProfile::Code)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeGameStateProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : NativeBorrowedGcProfile::pins)
            admit(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : NativeStartGameProfile::Code)
            if (pin.rva == 0xab879c || pin.rva == 0xab92f0 || pin.rva == 0x1af4ef0)
                admit(pin.rva, pin.bytes.data(), pin.size, PAGE_EXECUTE_READ);
        admit(G::FoldRva, G::Fold, sizeof(G::Fold), PAGE_READONLY);
        const std::uintptr_t zero = 0;
        const std::uint32_t  playing = 9, marker = 0, count = 3;
        const unsigned char  initialized = 1;
        const auto           thread = GetCurrentThreadId();
        admit(NativePedPoolProfile::RootRva, &zero, sizeof(zero), PAGE_READWRITE);
        admit(NativeGameStateProfile::stateRva, &playing, sizeof(playing), PAGE_READWRITE);
        admit(NativeBorrowedGcProfile::markerRva, &marker, sizeof(marker), PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadInitializedRva, &initialized, 1, PAGE_READWRITE);
        admit(NativeStartGameProfile::ThreadIdRva, &thread, sizeof(thread), PAGE_READWRITE);
        admit(G::CountRva, &count, sizeof(count), PAGE_READWRITE);
        std::array<unsigned char, G::RowBytes * 3> rows{};
        const NativeGarageName alpha{'a', 'l', 'p', 'h', 'a', 0, 0, 0}, beta{'b', 'E', 't', 'A', 0, 0, 0, 0}, duplicate{'B', 'E', 'T', 'A', 0, 0, 0, 0};
        std::memcpy(rows.data() + G::NameOffset, alpha.data(), alpha.size());
        std::memcpy(rows.data() + G::RowBytes + G::NameOffset, beta.data(), beta.size());
        std::memcpy(rows.data() + 2 * G::RowBytes + G::NameOffset, duplicate.data(), duplicate.size());
        rows[G::TypeOffset] = 5;
        rows[G::RowBytes + G::TypeOffset] = 17;
        rows[2 * G::RowBytes + G::TypeOffset] = 99;
        rows[G::RowBytes + G::DoorStateOffset] = 4;
        const NativeGarageGeometry expectedGeometry{{0x3f800000u, 0x80000000u, 0x40a00000u, 0x7fc12345u, 0x7fa54321u, 0x7f800000u, 0xff800000u, 0x42c80000u,
                                                     0x7f7fffffu, 0xff7fffffu, 1u, 0x80000001u, 0x00800000u, 0x80800000u}};
        std::memcpy(rows.data() + G::RowBytes, expectedGeometry.words.data(), sizeof(expectedGeometry.words));
        admit(G::BankRva, rows.data(), rows.size(), PAGE_READWRITE);
        ClockReaderTestHost::image = fixture.image;
        ClockNamedGarageOwnerTest::nameAddress = fixture.image + G::BankRva + G::RowBytes + G::NameOffset;
        ClockNamedGarageOwnerTest::countAddress = fixture.image + G::CountRva;
        ClockNamedGarageOwnerTest::geometryAddress = fixture.image + G::BankRva + G::RowBytes;
        ClockNamedGarageOwnerTest::geometryReadAction = 0;
        ClockNamedGarageOwnerTest::stores = ClockNamedGarageOwnerTest::action = 0;
        struct NamedGarageOwnerSdk final : CGarageNamedDE
        {
            NamedGarageOwnerSdk(NativeNamedGarageAccess& access, const NativeGarageName& name) : CGarageNamedDE(access, name) {}
        };
        NamedGarageOwnerContext context;
        NamedGarageOwnerSdk     garage(runtime.access, context.name);
        context.sdk = &garage;
        Check(runtime.access.ReadNamedGarageType(context.name, context.typeOutput) == ClockResult::NotInitialized && context.typeOutput == 0xe7 &&
                  runtime.access.SetNamedGarageOpen(context.name, true) == ClockResult::NotInitialized && !ClockNamedGarageOwnerTest::stores,
              "unbound native timing access grants no named garage row or implicit mutation");
        const auto retire = [&](NativeClockService& service)
        {
            if (!service.End())
            {
                fixture.invokeTimer();
                Check(service.End(), "same natural callback acknowledges named-property owner retirement");
            }
            Check(!service.IsRetained() && !runtime.timingHandler && !runtime.pedReferences.IsRetained() &&
                      !NativeServiceAdmission::IsClaimed(NativeServiceAdmission::Category::Clock),
                  "named properties retire callback/hooks/admission without creating ped references or a numeric garage factory");
        };
        const auto resetRows = [&]
        {
            fixture.Put(G::CountRva, count);
            fixture.Put(G::BankRva, rows);
            ClockNamedGarageOwnerTest::stores = ClockNamedGarageOwnerTest::action = 0;
            context.refusal = 0;
            context.typeOutput = 0xe7;
            context.openOutput = false;
        };
        {
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &NamedGarageOwnerHandler, &context);
            const auto invoke = [&]
            {
                Check(service.Renew(runtime.generation.load()), "renew actual named garage callback");
                fixture.invokeTimer();
            };
            context.operation = 4;
            invoke();
            Check(context.result == ClockResult::Ok && !context.threw && context.typeOutput == 17 && !runtime.pedReferences.IsRetained(),
                  "actual SDKGetType reacquires the first native name match inside this callback without a ped or SDKordinal conversion");
            context.operation = 8;
            context.geometryOutput.words.fill(0xdeadbeefu);
            invoke();
            Check(context.result == ClockResult::Ok && context.geometryOutput.words == expectedGeometry.words && !ClockNamedGarageOwnerTest::stores,
                  "current native owner publishes all14coherent raw geometry words without native writes or an SDKnumericID mapping");
            context.geometryOutput.words.fill(0xdeadbeefu);
            const auto outsideGeometry = context.geometryOutput.words;
            Check(runtime.access.ReadNamedGarageGeometry(context.name, context.geometryOutput) != ClockResult::Ok &&
                      context.geometryOutput.words == outsideGeometry,
                  "copied name and geometry output never grant access outside the retained callback");
            for (unsigned operation = 9; operation != 13; ++operation)
            {
                context.operation = operation;
                invoke();
                Check(context.result == ClockResult::Ok && !context.threw,
                      "each actual SDKposition/direction/size/bounds getter dispatches once through the current named owner");
            }
            std::array<std::uint32_t, 14> sdkWords{};
            std::memcpy(sdkWords.data(), &context.geometryPosition, 3 * sizeof(std::uint32_t));
            std::memcpy(sdkWords.data() + 3, context.geometryDirection.data(), 4 * sizeof(std::uint32_t));
            std::memcpy(sdkWords.data() + 7, context.geometrySize.data(), 3 * sizeof(std::uint32_t));
            std::memcpy(sdkWords.data() + 10, context.geometryBounds.data(), 4 * sizeof(std::uint32_t));
            Check(sdkWords == expectedGeometry.words && sdkWords[7] == 0x42c80000u,
                  "all8actualSDKproperties preserveNaN/signzero/infinitybits and rawupperZ Height100 rather than subtractingoriginZ5");
            context.operation = 8;
            context.geometryOutput.words.fill(0xdeadbeefu);
            const auto geometrySentinel = context.geometryOutput.words;
            ClockNamedGarageOwnerTest::geometryReadAction = 1;
            invoke();
            Check(context.result == ClockResult::OwnerConflict && context.geometryOutput.words == geometrySentinel && !runtime.stopping.load() &&
                      fixture.Get<std::uint32_t>(G::BankRva + G::RowBytes + 7 * sizeof(std::uint32_t)) == 0x12345678u && !ClockNamedGarageOwnerTest::stores,
                  "mixed raw geometry refuses complete output, preserves native progress and does not fabricate a mutation or Stop");
            invoke();
            Check(context.result == ClockResult::Ok && context.geometryOutput.words[7] == 0x12345678u,
                  "fresh same-owner geometry observation may see later native progress without replaying a failed read");
            fixture.Put(G::BankRva + G::RowBytes, expectedGeometry.words);
            const auto stores = ClockNamedGarageOwnerTest::stores;
            context.typeOutput = 0xe7;
            Check(runtime.access.ReadNamedGarageType(context.name, context.typeOutput) != ClockResult::Ok && context.typeOutput == 0xe7,
                  "a retained named SDK input does not grant property access outside its callback");
            ClockResult foreign = ClockResult::Ok;
            std::thread other([&] { foreign = runtime.access.SetNamedGarageType(context.name, 7); });
            other.join();
            Check(foreign != ClockResult::Ok && ClockNamedGarageOwnerTest::stores == stores, "foreign threads cannot borrow named property writes");
            context.operation = 5;
            for (const auto type : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{43}, std::uint8_t{127}, std::uint8_t{128}, std::uint8_t{255}})
            {
                context.type = type;
                const auto before = ClockNamedGarageOwnerTest::stores;
                invoke();
                Check(context.result == ClockResult::Ok && !context.threw && ClockNamedGarageOwnerTest::stores == before + 1 &&
                          fixture.Get<std::uint8_t>(G::BankRva + G::RowBytes + G::TypeOffset) == type &&
                          fixture.Get<std::uint8_t>(G::BankRva + G::RowBytes + G::DoorStateOffset) == 4 &&
                          fixture.Get<std::uint8_t>(G::BankRva + 2 * G::RowBytes + G::TypeOffset) == 99,
                      "actual SDKSetType preserves full byte intent, first-match identity and adjacent door/duplicate state");
            }
            context.operation = 6;
            invoke();
            Check(context.result == ClockResult::Ok && !context.openOutput, "actual SDKIsOpen uses MTA1/3 rather than native script1/4");
            context.operation = 7;
            for (const bool open : {false, true})
            {
                context.open = open;
                const auto before = ClockNamedGarageOwnerTest::stores;
                invoke();
                Check(context.result == ClockResult::Ok && !context.threw && ClockNamedGarageOwnerTest::stores == before + 1 &&
                          fixture.Get<std::uint8_t>(G::BankRva + G::RowBytes + G::DoorStateOffset) == (open ? 3 : 2),
                      "actual SDKSetOpen dispatches once to direct MTA3/2 storage through the retained owner");
                context.operation = 6;
                invoke();
                Check(context.result == ClockResult::Ok && context.openOutput == open, "actual named SDK readback preserves MTA door policy");
                context.operation = 7;
            }
            context.operation = 8;
            for (unsigned refusal = 1; refusal != 5; ++refusal)
            {
                context.refusal = refusal;
                context.geometryOutput.words.fill(0xdeadbeefu);
                const auto beforeGeometry = context.geometryOutput.words;
                invoke();
                Check(context.result != ClockResult::Ok && context.geometryOutput.words == beforeGeometry && !runtime.stopping.load(),
                      "stale callback/admission generation, nested depth and expired lease refuse before any named effect");
            }
            context.refusal = 0;
            const auto originalName = context.name;
            context.name = NativeGarageName{'m', 'i', 's', 's', 'i', 'n', 'g', 0};
            context.operation = 1;
            const auto before = ClockNamedGarageOwnerTest::stores;
            invoke();
            Check(context.result == ClockResult::InvalidArgument && !runtime.stopping.load() && ClockNamedGarageOwnerTest::stores == before,
                  "missing named association refuses without inventing a numeric fallback or poisoning the owner");
            context.name = originalName;
            retire(service);
        }
        for (unsigned action = 1; action != 5; ++action)
        {
            resetRows();
            context.operation = 3;
            context.open = true;
            NativeClockService service;
            BeginOwned(service, fixture, false, true, true, &NamedGarageOwnerHandler, &context);
            ClockNamedGarageOwnerTest::action = action;
            fixture.invokeTimer();
            const auto value = fixture.Get<std::uint8_t>(G::BankRva + G::RowBytes + G::DoorStateOffset);
            Check(context.result != ClockResult::Ok && runtime.stopping.load() && ClockNamedGarageOwnerTest::stores == 1 &&
                      value == (action == 4 ? static_cast<std::uint8_t>(4 ^ 0x80) : std::uint8_t{3}),
                  "name/count loss, unknown partial fault and selected-byte race stop the actual owner after one attempt while retaining native progress");
            retire(service);
            Check(ClockNamedGarageOwnerTest::stores == 1 && fixture.Get<std::uint8_t>(G::BankRva + G::RowBytes + G::DoorStateOffset) == value,
                  "real callback cleanup never replays or restores persistent named garage intent");
        }
        ClockNamedGarageOwnerTest::nameAddress = ClockNamedGarageOwnerTest::countAddress = ClockNamedGarageOwnerTest::geometryAddress = 0;
        ClockNamedGarageOwnerTest::geometryReadAction = 0;
        ClockNamedGarageOwnerTest::stores = ClockNamedGarageOwnerTest::action = 0;
        for (auto it = saved.rbegin(); it != saved.rend(); ++it)
        {
            fixture.Patch(it->rva, it->bytes.data(), it->bytes.size());
            DWORD ignored = 0;
            Check(::VirtualProtect(reinterpret_cast<void*>(fixture.image + it->rva), it->bytes.size(), it->protection, &ignored) != FALSE,
                  "restore only task-owned fixture bytes/protection after owner retirement");
        }
        std::puts("Production native named garage geometry callback owner fixture completed");
        std::puts("Actual named garage SDK callback owner fixture completed");
        std::puts("Production native named garage callback owner fixture completed");
    }
}
