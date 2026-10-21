/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual eight native scalar reads and existing CWeather SDK dispatch
 *****************************************************************************/
#include "../../Client/game_de/NativeWeatherScalarMemory.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include "../../Client/game_de/CWeatherScalarsDE.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace
{
    namespace P = mta::game_de::NativeWeatherScalarProfile;
    namespace T = mta::game_de::NativeWeatherTransitionProfile;
    using Memory = mta::game_de::NativeWeatherScalarMemory;
    using Result = mta::game_de::ClockResult;
    using Field = P::Field;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, why);
            std::exit(1);
        }
    }
}
#if defined(MTA_TEST_WEATHER_SCALAR_CONTROLLED)
namespace
{
    std::uintptr_t selectedImage = 0, deniedImageRva = 0;
    unsigned       imageReads = 0, failImageReadAt = 0, scalarReads = 0;
    bool           buildAllowed = true, denyScalar = false, faultScalar = false;
    DWORD          faultProtection = 0;
    HMODULE WINAPI FixtureModule(LPCWSTR name)
    {
        return name ? ::GetModuleHandleW(name) : reinterpret_cast<HMODULE>(selectedImage);
    }
}
namespace mta::game_de::ScalarFixtureImage
{
    bool CheckExecutableBuild() noexcept
    {
        return buildAllowed;
    }
    bool ImagePages(std::uintptr_t image, std::uintptr_t address, std::size_t size, bool = false, bool = false) noexcept
    {
        return image && image == selectedImage && address >= image && address - image <= P::ImageSize && size <= P::ImageSize - (address - image) &&
               !(deniedImageRva && address <= image + deniedImageRva && size > image + deniedImageRva - address);
    }
    bool ReadImage(std::uintptr_t image, std::uintptr_t address, void* output, std::size_t size, bool writable = false, bool executable = false) noexcept
    {
        ++imageReads;
        if (imageReads == failImageReadAt || !ImagePages(image, address, size, writable, executable))
            return false;
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &copied) && copied == size;
    }
    bool SectionContains(const IMAGE_SECTION_HEADER& section, std::size_t rva, std::size_t size, DWORD required, DWORD forbidden) noexcept
    {
        return NativeImageAccess::SectionContains(section, rva, size, required, forbidden);
    }
}
    #define NativeImageAccess ScalarFixtureImage
    #define GetModuleHandleW  FixtureModule
#endif
#include "../../Client/game_de/NativeWeatherTransitionProfile.cpp"
#include "../../Client/game_de/NativeWeatherScalarProfile.cpp"
#if defined(MTA_TEST_WEATHER_SCALAR_CONTROLLED)
    #undef NativeImageAccess
    #undef GetModuleHandleW
namespace
{
    class OwnedImage
    {
    public:
        unsigned char* bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, P::ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        OwnedImage()
        {
            Check(bytes != nullptr, "allocate only owned ordinary-host image memory");
            selectedImage = reinterpret_cast<std::uintptr_t>(bytes);
            auto& dos = *reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            auto& nt = *reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + 0x100);
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = 2;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::ImageSize);
            auto* sections = reinterpret_cast<IMAGE_SECTION_HEADER*>(bytes + 0x100 + sizeof(IMAGE_NT_HEADERS64));
            sections[0].VirtualAddress = 0x1000;
            sections[0].Misc.VirtualSize = 0x3f40000;
            sections[0].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            sections[1].VirtualAddress = 0x3f41000;
            sections[1].Misc.VirtualSize = static_cast<DWORD>(P::ImageSize - 0x3f41000);
            sections[1].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            for (const auto& pin : T::Code)
                std::memcpy(bytes + pin.rva, pin.bytes.data(), pin.size);
            for (const auto& pin : P::Code)
                std::memcpy(bytes + pin.rva, pin.bytes.data(), pin.size);
            for (const auto& name : P::Names)
            {
                std::memcpy(bytes + name.nameRva, name.name, std::strlen(name.name) + 1);
                const auto pointer = static_cast<std::uint64_t>(selectedImage + name.nameRva);
                std::memcpy(bytes + name.recordRva, &pointer, sizeof(pointer));
                std::memcpy(bytes + name.recordRva + (name.propertyOffset ? 36 : 8), &name.value, sizeof(name.value));
            }
            const wchar_t intensity[] = L"RainbowIntensity", mesh[] = L"RainbowMesh";
            std::memcpy(bytes + P::RainbowIntensityNameRva, intensity, sizeof(intensity));
            std::memcpy(bytes + P::RainbowMeshNameRva, mesh, sizeof(mesh));
            for (std::size_t index = 0; index < P::Fields.size(); ++index)
            {
                const auto value = static_cast<float>(index) / 8.0f;
                std::memcpy(bytes + P::Fields[index], &value, sizeof(value));
            }
        }
        ~OwnedImage()
        {
            selectedImage = 0;
            if (bytes)
                VirtualFree(bytes, 0, MEM_RELEASE);
        }
    };
    bool ScalarPages(std::uintptr_t address, std::size_t size, bool writable, bool executable) noexcept
    {
        ++scalarReads;
        bool known = false;
        for (const auto rva : P::Fields)
            known |= address == selectedImage + rva;
        if (!selectedImage || !known || size != sizeof(float) || writable || executable || denyScalar)
            return false;
        if (faultScalar)
        {
            faultScalar = false;
            if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_NOACCESS, &faultProtection))
                std::abort();
        }
        return true;
    }
    class OwnedAccess : public mta::game_de::NativeWeatherScalarAccess
    {
    public:
        OwnedAccess(const P::Binding& binding) : bound(binding), memory(binding, ScalarPages) {}
        Result   refusal = Result::Ok;
        unsigned calls = 0;
        Field    last = Field::Rain;
        Result   ReadScalar(Field field, float& value) noexcept override
        {
            ++calls;
            last = field;
            if (refusal != Result::Ok)
            {
                value = -999;
                return refusal;
            }
            if (!P::Revalidate(bound))
                return Result::BindingChanged;
            return memory.Read(field, value) ? Result::Ok : Result::ReadFailure;
        }

    private:
        const P::Binding bound;
        const Memory     memory;
    };
    // The unimplemented Set/Reset methods throw in the fixture so tests cannot
    // accidentally turn partial native getters into a fake complete provider.
    class OwnedWeather final : public mta::game_de::CWeatherScalarsDE
    {
    public:
        OwnedWeather(mta::game_de::NativeWeatherTransitionAccess& transition, mta::game_de::NativeWeatherScalarAccess& scalar)
            : CWeatherScalarsDE(transition, scalar)
        {
        }
        void SetAmountOfRain(float) override { Reject(); }
        void ResetAmountOfRain() override { Reject(); }
        bool SetWetRoads(float) override { Reject(); }
        bool ResetWetRoads() override { Reject(); }
        bool SetFoggyness(float) override { Reject(); }
        bool ResetFoggyness() override { Reject(); }
        bool SetFog(float) override { Reject(); }
        bool ResetFog() override { Reject(); }
        bool SetRainFog(float) override { Reject(); }
        bool ResetRainFog() override { Reject(); }
        bool SetWaterFog(float) override { Reject(); }
        bool ResetWaterFog() override { Reject(); }
        bool SetSandstorm(float) override { Reject(); }
        bool ResetSandstorm() override { Reject(); }
        bool SetRainbow(float) override { Reject(); }
        bool ResetRainbow() override { Reject(); }

    private:
        [[noreturn]] static void Reject() { throw std::logic_error("unqualified scalar setter/reset fixture method"); }
    };
    float SdkRead(CWeather& weather, Field field)
    {
        switch (field)
        {
            case Field::Rain:
                return weather.GetAmountOfRain();
            case Field::WetRoads:
                return weather.GetWetRoads();
            case Field::Foggyness:
                return weather.GetFoggyness();
            case Field::Fog:
                return weather.GetFog();
            case Field::RainFog:
                return weather.GetRainFog();
            case Field::WaterFog:
                return weather.GetWaterFog();
            case Field::Sandstorm:
                return weather.GetSandstorm();
            case Field::Rainbow:
                return weather.GetRainbow();
        }
        throw std::logic_error("fixture field outside actual SDK getters");
    }
}
#endif
int main()
{
    P::Binding binding{11};
#if !defined(MTA_TEST_WEATHER_SCALAR_CONTROLLED)
    Check(!P::Qualify(binding) && binding.image == 11, "shipping exact image refuses ordinary host without publishing a binding");
    Check(!P::Revalidate(binding), "shipping live proof refuses arbitrary image identity");
    std::printf("Passed %u native weather scalar checks (64-bit; shipping-wrong-image)\n", checks);
#else
    OwnedImage image;
    Check(P::Qualify(binding) && binding.image == selectedImage, "qualify actual native producer and named-consumer relationships in controlled owned image");
    const auto byteRefusal = [&](std::uintptr_t rva)
    {
        const auto original = image.bytes[rva];
        image.bytes[rva] ^= 1;
        P::Binding canary{11};
        Check(!P::Qualify(canary) && canary.image == 11 && !P::Revalidate(binding),
              "changed native proof refuses new and existing bindings without output mutation");
        image.bytes[rva] = original;
    };
    for (const auto& pin : P::Code)
    {
        byteRefusal(pin.rva);
        byteRefusal(pin.rva + pin.size - 1);
    }
    byteRefusal(T::Code[0].rva);
    for (const auto& name : P::Names)
    {
        byteRefusal(name.nameRva);
        byteRefusal(name.nameRva + std::strlen(name.name));
        byteRefusal(name.recordRva);
        byteRefusal(name.recordRva + (name.propertyOffset ? 36 : 8));
    }
    byteRefusal(P::RainbowIntensityNameRva);
    byteRefusal(P::RainbowMeshNameRva);
    for (const auto rva : P::Fields)
    {
        deniedImageRva = rva;
        Check(!P::Revalidate(binding), "every native scalar requires its original readable writable-image data page");
    }
    deniedImageRva = 0;
    imageReads = 0;
    Check(P::Revalidate(binding), "live qualifier healthy after independent refusals");
    const auto reads = imageReads;
    for (unsigned read = 1; read <= reads; ++read)
    {
        imageReads = 0;
        failImageReadAt = read;
        Check(!P::Revalidate(binding), "every scalar profile read failure explicitly refuses access");
    }
    failImageReadAt = 0;
    buildAllowed = false;
    P::Binding canary{11};
    Check(!P::Qualify(canary) && canary.image == 11, "wrong disk build cannot acquire scalar identity");
    buildAllowed = true;
    const Memory memory(binding, ScalarPages);
    struct AbsentTransition : mta::game_de::NativeWeatherTransitionAccess
    {
    } transition;
    OwnedAccess  access(binding);
    OwnedWeather weather(transition, access);
    CWeather&    sdk = weather;
    for (std::size_t index = 0; index < P::Fields.size(); ++index)
    {
        const auto field = static_cast<Field>(index);
        float      value = -77;
        Check(memory.Read(field, value) && value == static_cast<float>(index) / 8.0f, "native scalar reads actual correct float field");
        const auto before = access.calls;
        Check(SdkRead(sdk, field) == value && access.calls == before + 1 && access.last == field,
              "actual existing SDK getter dispatches exactly once to its correct native scalar");
        for (const float finite : {-3.5f, 7.25f, -0.0f})
        {
            std::memcpy(image.bytes + P::Fields[index], &finite, sizeof(finite));
            const auto returned = SdkRead(sdk, field);
            Check(std::memcmp(&returned, &finite, sizeof(finite)) == 0, "finite native values preserve bits without guessed0..1 clamp or stale observation");
        }
        for (const auto invalid : {0x7fc00001u, 0x7f800000u, 0xff800000u})
        {
            std::memcpy(image.bytes + P::Fields[index], &invalid, sizeof(invalid));
            value = -77;
            Check(!memory.Read(field, value) && value == -77, "nonfinite native scalar explicitly refuses and preserves output");
        }
        const float original = static_cast<float>(index) / 8.0f;
        std::memcpy(image.bytes + P::Fields[index], &original, sizeof(original));
    }
    float      value = -77;
    const auto before = scalarReads;
    Check(!memory.Read(static_cast<Field>(255), value) && value == -77 && scalarReads == before,
          "invalid field cannot supply an arbitrary pointer or run page policy");
    const Memory empty({0}, ScalarPages), overflow({(std::numeric_limits<std::uintptr_t>::max)() - 1}, ScalarPages), noPolicy(binding, nullptr);
    Check(!empty.Read(Field::Rain, value) && !overflow.Read(Field::Rain, value) && !noPolicy.Read(Field::Rain, value),
          "invalid binding or missing borrowed page policy grants no scalar reads");
    denyScalar = true;
    Check(!memory.Read(Field::Rain, value) && value == -77, "refused page cannot publish a false zero scalar");
    denyScalar = false;
    faultScalar = true;
    Check(!memory.Read(Field::Rain, value) && value == -77, "protection race is caught by actual OS read and preserves output");
    DWORD ignored = 0;
    Check(VirtualProtect(image.bytes + P::Fields[0], sizeof(float), faultProtection, &ignored) != FALSE, "restore only owned fixture data page protection");
    for (const auto result : {Result::NotInitialized, Result::WrongThread, Result::BindingChanged, Result::ReadFailure, Result::Busy, Result::Stopped})
        for (std::size_t index = 0; index < P::Fields.size(); ++index)
        {
            access.refusal = result;
            const auto calls = access.calls;
            bool       refused = false;
            try
            {
                static_cast<void>(SdkRead(sdk, static_cast<Field>(index)));
            }
            catch (const std::runtime_error& failure)
            {
                refused = std::strstr(failure.what(), std::to_string(static_cast<std::uint32_t>(result)).c_str()) != nullptr;
            }
            Check(refused && access.calls == calls + 1, "SDK refuses exactly once with actual owner status instead of publishing poisoned output");
        }
    struct AbsentScalar : mta::game_de::NativeWeatherScalarAccess
    {
    } absent;
    OwnedWeather unavailable(transition, absent);
    bool         refused = false;
    try
    {
        static_cast<void>(unavailable.GetAmountOfRain());
    }
    catch (const std::runtime_error&)
    {
        refused = true;
    }
    Check(refused, "default scalar owner cannot act as fake native getter");
    std::printf("Passed %u native weather scalar checks (64-bit; controlled-image)\n", checks);
#endif
    return 0;
}
