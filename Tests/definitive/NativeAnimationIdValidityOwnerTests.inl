/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual signed ID selection borrows current native cookie-owned rows
 ****************************************************************************/
namespace AnimationIdValidityOwnerFixture
{
    namespace Host = NativeCustomAnimSequenceStorageFixture;
    namespace Hook = ClockAnimationManagerOwnerTest;
    std::uintptr_t rows = 0, child = 0, childBase = 0;
    std::uint64_t* callerGeneration = nullptr;
    std::uint32_t* callerGroup = nullptr;
    std::uint32_t* callerAnim = nullptr;
    bool           changed = false;
    unsigned       transition = 0;
    void           ChangeCaller() noexcept
    {
        *callerGeneration = 0;
        *callerGroup = 0xffffffffu;
        *callerAnim = 0xffffffffu;
    }
    void ChangeAfterPayload() noexcept
    {
        if (Hook::reads < 3)
        {
            Hook::afterRead = &ChangeAfterPayload;
            return;
        }
        if (transition == 0)
        {
            const std::uint16_t id = 7;
            std::memcpy(reinterpret_cast<void*>(child + 0x0a), &id, 2);
        }
        if (transition == 1)
        {
            const std::uint64_t cap = 2;
            std::memcpy(reinterpret_cast<void*>(childBase), &cap, 8);
        }
        if (transition == 2)
        {
            const auto pointer = child + 32;
            std::memcpy(reinterpret_cast<void*>(rows + 8), &pointer, 8);
        }
        changed = true;
    }
    void LoseFinalOwner() noexcept
    {
        if (Hook::reads < 8)
        {
            Hook::afterRead = &LoseFinalOwner;
            return;
        }
        ++timingCallbackGeneration;
        changed = true;
    }
    void FailFinalRead() noexcept
    {
        if (Hook::reads < 7)
        {
            Hook::afterRead = &FailFinalRead;
            return;
        }
        Hook::failNext = true;
        changed = true;
    }
    void RunPrepared(ClockImageFixture& fixture)
    {
        WorldGroundOwnerFixture::Writes saved{fixture, {}};
        const auto                      admit = [&](const auto& pins)
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
              "ID fixture obtains the genuine process-lifetime pin for its compiled callback module");
        runtime.modulePinned.store(true);
        namespace P = NativeAnimationGroupArrayProfile;
        namespace A = NativeAnimationStaticAssociationProfile;
        for (const auto& pin : P::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : A::Code)
            WorldGroundOwnerFixture::Writes::Apply(&saved, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        constexpr std::uint64_t outerCapacity = 2, childCapacity = 3;
        constexpr std::size_t   outerBytes = 8 + outerCapacity * 32, childBytes = 8 + childCapacity * 32;
        void*                   outerAllocation = VirtualAlloc(nullptr, outerBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        void*                   childAllocation = VirtualAlloc(nullptr, childBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(outerAllocation && childAllocation, "own only task fixture storage for the two original native-shaped allocations");
        const auto outerBase = reinterpret_cast<std::uintptr_t>(outerAllocation);
        rows = outerBase + 8;
        childBase = reinterpret_cast<std::uintptr_t>(childAllocation);
        child = childBase + 8;
        std::memset(outerAllocation, 0xa5, outerBytes);
        std::memset(childAllocation, 0xa5, childBytes);
        std::memcpy(outerAllocation, &outerCapacity, 8);
        std::memcpy(childAllocation, &childCapacity, 8);
        const std::uint32_t num = 1, offset = 0, zero = 0;
        std::memcpy(reinterpret_cast<void*>(rows + 8), &child, 8);
        std::memcpy(reinterpret_cast<void*>(rows + 0x10), &num, 4);
        std::memcpy(reinterpret_cast<void*>(rows + 0x14), &offset, 4);
        std::memcpy(reinterpret_cast<void*>(rows + 32 + 0x10), &zero, 4);
        const std::array<std::uint16_t, 3> ids{0, 0xffffu, 2};
        for (std::size_t i = 0; i != ids.size(); ++i)
            std::memcpy(reinterpret_cast<void*>(child + i * 32 + 0x0a), &ids[i], 2);
        // Child node/hierarchy and parent block fields remain unrelated bytes.
        // This observer must not turn the ID-only source into a facade borrow.
        saved.Global(P::GlobalRva, rows);
        saved.Global(P::CountRva, std::int32_t{2});
        std::array<unsigned char, outerBytes> outerExpected{};
        std::array<unsigned char, childBytes> childExpected{};
        std::memcpy(outerExpected.data(), outerAllocation, outerBytes);
        std::memcpy(childExpected.data(), childAllocation, childBytes);
        const auto                             generation = runtime.generation.load();
        PrivateAnimationFactoryManagerReceiver manager(runtime.access, generation);
        CAnimManager&                          sdk = manager;
        Hook::watched = fixture.image + P::GlobalRva;
        Hook::watchedBytes = 8;
        Hook::reads = 0;
        Hook::failNext = false;
        Hook::afterRead = nullptr;
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
                    std::fprintf(stderr, "ID validity action %s refused: %s\n", label, error.what());
                    Check(false, label);
                }
            };
            Check(service.Renew(generation), "renew actual owner for each bounded ID observation");
            fixture.invokeTimer();
            callback.action = [](NativeGameTimingAccess&) {};
        };
        const auto unchanged = [&]
        {
            return !std::memcmp(outerExpected.data(), outerAllocation, outerBytes) && !std::memcmp(childExpected.data(), childAllocation, childBytes) &&
                   !host.mallocCalls && !host.freeCalls && !runtime.customSequences.IsRetained() && !runtime.privateHierarchies.IsRetained();
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
        action("actual signed ID and logical/capacity separation",
               [&]
               {
                   std::int16_t id = 123;
                   Check(runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 1, id) == ClockResult::Ok && id == -1 && unchanged(),
                         "actual provider returns original signedWORD sentinel without fallback");
               });
        for (std::uint32_t animation = 0; animation != 3; ++animation)
        {
            action("actual source SDK comparison",
                   [&]
                   {
                       Check(sdk.IsValidAnim(0, animation) == (animation != 1) && unchanged(),
                             "actual SDK compares signedWORD ID; selected capacity row2 remains valid although NumAnims is only1");
                   });
        }
        action("signed high-bit ID remains signed",
               [&]
               {
                   const std::uint16_t negative = 0x8000u;
                   std::memcpy(reinterpret_cast<void*>(child + 0x0a), &negative, 2);
                   std::int16_t id = 123;
                   const auto   result = runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0, id);
                   const bool   matches = sdk.IsValidAnim(0, 0);
                   std::memcpy(reinterpret_cast<void*>(child + 0x0a), &ids[0], 2);
                   Check(result == ClockResult::Ok && id == (std::numeric_limits<std::int16_t>::min)() && !matches && unchanged(),
                         "rawWORD sign bits are preserved rather than converted into a fallback or unsigned match");
               });
        action("native modulo32 offset selection",
               [&]
               {
                   const std::uint32_t wrapped = 0xffffffffu;
                   const std::uint16_t match = 0;
                   std::memcpy(reinterpret_cast<void*>(rows + 0x14), &wrapped, 4);
                   std::memcpy(reinterpret_cast<void*>(child + 32 + 0x0a), &match, 2);
                   std::int16_t id = 123;
                   const auto   result = runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0, id);
                   const bool   valid = sdk.IsValidAnim(0, 0);
                   std::memcpy(reinterpret_cast<void*>(rows + 0x14), &offset, 4);
                   std::memcpy(reinterpret_cast<void*>(child + 32 + 0x0a), &ids[1], 2);
                   Check(result == ClockResult::Ok && !id && valid && unchanged(),
                         "anim0 minus offsetffffffff selects original child row1 by modulo32 subtraction");
               });
        action("full input and actual cookie guard",
               [&]
               {
                   std::int16_t id = 123;
                   Check(runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 3, id) == ClockResult::InvalidArgument && id == 123 &&
                             runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0xffffffffu, id) == ClockResult::InvalidArgument && id == 123 &&
                             runtime.access.ReadAnimationStaticAssociationAnimID(generation + 1, 0, 0, id) == ClockResult::OwnerConflict && id == 123 &&
                             unchanged(),
                         "full32 ID cannot wrap through child capacity and generation refusal preserves output");
                   const std::uint32_t one = 1;
                   std::memcpy(reinterpret_cast<void*>(rows + 0x14), &one, 4);
                   const auto below = runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0, id);
                   std::memcpy(reinterpret_cast<void*>(rows + 0x14), &offset, 4);
                   Check(below == ClockResult::InvalidArgument && id == 123 && unchanged(),
                         "below-offset subtraction zeroextends its large modulo result without fallback");
               });
        action("Created false short circuits before unrelated child",
               [&]
               {
                   std::int16_t id = 123;
                   Check(!sdk.IsValidAnim(1, 0) && runtime.access.ReadAnimationStaticAssociationAnimID(generation, 1, 0, id) == ClockResult::NotInitialized &&
                             id == 123 && unchanged(),
                         "actual NumAnims0 gate never dereferences the deliberately unrelated child bytes");
               });
        for (const auto pointer : {std::uintptr_t{0}, child + 1, fixture.image + NativeAnimationBlockProfile::BankRva + 8})
        {
            action("current child origin qualification",
                   [&]
                   {
                       std::memcpy(reinterpret_cast<void*>(rows + 8), &pointer, 8);
                       std::int16_t id = 123;
                       const auto   result = runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0, id);
                       std::memcpy(reinterpret_cast<void*>(rows + 8), &child, 8);
                       Check(result != ClockResult::Ok && id == 123 && unchanged(),
                             "null/misaligned/MEM_IMAGE child cannot acquire original native array authority");
                   });
        }
        action("malformed original child cookie",
               [&]
               {
                   const auto bad = (std::numeric_limits<std::uint64_t>::max)();
                   std::memcpy(childAllocation, &bad, 8);
                   std::int16_t id = 123;
                   const auto   result = runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0, id);
                   std::memcpy(childAllocation, &childCapacity, 8);
                   Check(result == ClockResult::BindingChanged && id == 123 && unchanged(),
                         "invalid64-bit original cookie refuses before child row arithmetic");
               });
        action("copied caller intent",
               [&]
               {
                   auto          g = generation;
                   std::uint32_t group = 0, anim = 2;
                   callerGeneration = &g;
                   callerGroup = &group;
                   callerAnim = &anim;
                   Hook::afterRead = &ChangeCaller;
                   std::int16_t id = 123;
                   const auto   result = runtime.access.ReadAnimationStaticAssociationAnimID(g, group, anim, id);
                   Hook::afterRead = nullptr;
                   callerGeneration = nullptr;
                   callerGroup = callerAnim = nullptr;
                   Check(result == ClockResult::Ok && id == 2 && !g && group == 0xffffffffu && anim == 0xffffffffu && unchanged(),
                         "actual provider copies all caller values before native observation callbacks");
               });
        for (transition = 0; transition != 3; ++transition)
        {
            action("parent/child/ID coherent bookends",
                   [&]
                   {
                       Hook::reads = 0;
                       changed = false;
                       Hook::afterRead = &ChangeAfterPayload;
                       std::int16_t id = 123;
                       const auto   result = runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0, id);
                       Hook::afterRead = nullptr;
                       std::memcpy(reinterpret_cast<void*>(child + 0x0a), &ids[0], 2);
                       std::memcpy(childAllocation, &childCapacity, 8);
                       std::memcpy(reinterpret_cast<void*>(rows + 8), &child, 8);
                       Check(changed && result == ClockResult::BindingChanged && id == 123 && unchanged(),
                             "ID/cookie/parent pointer changes after payload cannot publish a torn scalar");
                   });
        }
        action("first native global read failure",
               [&]
               {
                   Hook::failNext = true;
                   std::int16_t id = 123;
                   Check(runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0, id) == ClockResult::ReadFailure && id == 123 &&
                             !runtime.stopping.load(),
                         "first observation failure preserves output without native effect debt");
               });
        action("last distinct native read failure",
               [&]
               {
                   Hook::reads = 0;
                   changed = false;
                   Hook::afterRead = &FailFinalRead;
                   std::int16_t id = 123;
                   const auto   result = runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0, id);
                   Hook::afterRead = nullptr;
                   Hook::failNext = false;
                   Check(changed && result == ClockResult::ReadFailure && id == 123 && unchanged(), "final parent observation failure preserves signed output");
               });
        action("final current owner loss",
               [&]
               {
                   const auto previous = timingCallbackGeneration;
                   Hook::reads = 0;
                   changed = false;
                   Hook::afterRead = &LoseFinalOwner;
                   std::int16_t id = 123;
                   const auto   result = runtime.access.ReadAnimationStaticAssociationAnimID(generation, 0, 0, id);
                   Hook::afterRead = nullptr;
                   timingCallbackGeneration = previous;
                   Check(changed && result == ClockResult::OwnerConflict && id == 123 && unchanged(),
                         "final generation bookend refuses without altering native storage");
               });
        const auto reads = Hook::reads;
        Check(!sdk.IsValidAnim(0, 391) && !sdk.IsValidAnim(0, 0xffffffffu) && Hook::reads == reads,
              "source invalid animation enum remains CPUfalse outside callback with no native group read");
        Check(refuses([&] { static_cast<void>(sdk.IsValidAnim(0, 0)); }) && unchanged(), "valid animation still requires actual current callback admission");
        Hook::afterRead = nullptr;
        Hook::failNext = false;
        Hook::watched = 0;
        Hook::watchedBytes = sizeof(std::int32_t);
        WorldGroundOwnerFixture::Retire(service, fixture);
        Check(unchanged(), "readonly End never frees/retains native-shaped parent or child arrays");
        fixture.Put(P::GlobalRva, std::uintptr_t{0});
        fixture.Put(P::CountRva, std::int32_t{0});
        Check(VirtualFree(childAllocation, 0, MEM_RELEASE) != FALSE && VirtualFree(outerAllocation, 0, MEM_RELEASE) != FALSE,
              "release only task fixture storage after native observer retires");
        rows = child = childBase = 0;
        Host::active = nullptr;
        ClockReaderTestHost::image = previousImage;
        saved.Restore();
        std::puts("Production animation ID validity owner fixture completed");
        std::puts("Production actual animation ID validity SDK fixture completed");
    }
}
