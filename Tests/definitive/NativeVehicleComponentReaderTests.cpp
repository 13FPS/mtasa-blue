/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the vehicle mesh association and descendant reader on x64
 *****************************************************************************/
#include "../../Client/game_de/NativeVehicleComponentReader.h"
#include <cstdio>
#include <cstdlib>
#include <map>

namespace
{
    using namespace mta::game_de;
    using namespace NativeVehicleComponents;
    namespace Names = NativeStreamingSpecialModelProfile;
    namespace Objects = NativeStartupProvidersProfile;
    namespace Vehicle = NativeModelWheelProfile;
    namespace Stream = NativeStreamingRequestProfile;

    // Only the vtable page lives in this image: Member() still exercises the
    // real Win32 image-page qualification in addition to the registry lookup.
    constexpr std::uintptr_t vtableWord = 0x18273645;
    const auto               image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    constexpr std::uintptr_t mesh = 0x120000000, skeleton = mesh + 0x1000, socket = mesh + 0x2000, boneArray = mesh + 0x3000, mapRows = mesh + 0x4000;
    constexpr std::uintptr_t poolBlock = mesh + 0x5000, socketList = mesh + 0x6000, chunks = mesh + 0x7000, items = mesh + 0x8000;
    constexpr std::uintptr_t render = mesh + 0x9000, rows = mesh + 0xA000, objectClass = mesh + 0xB000, meshClass = mesh + 0xC000, chain = mesh + 0xD000;
    std::map<std::uintptr_t, unsigned char> bytes;
    unsigned                                checks = 0;
    void                                    Check(bool pass, const char* reason)
    {
        ++checks;
        if (!pass)
        {
            std::fprintf(stderr, "Failed vehicle component check %u: %s\n", checks, reason);
            std::exit(1);
        }
    }
    bool ReadMemory(std::uintptr_t address, void* out, std::size_t length) noexcept
    {
        auto dest = static_cast<unsigned char*>(out);
        for (std::size_t i = 0; i < length; ++i)
        {
            auto it = bytes.find(address + i);
            if (it == bytes.end())
                return false;
            dest[i] = it->second;
        }
        return true;
    }
    template <typename T>
    void Put(std::uintptr_t address, const T& value)
    {
        auto src = reinterpret_cast<const unsigned char*>(&value);
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bytes[address + i] = src[i];
    }
    void Object(std::uintptr_t address, int index, std::uintptr_t cls)
    {
        Put(address, reinterpret_cast<std::uintptr_t>(&vtableWord));
        Put(address + 8, 0u);
        Put(address + 12, index);
        Put(address + 16, cls);
        Put(address + 32, std::uintptr_t{});
        Put(items + 24 * index, address);
        Put(items + 24 * index + 8, 0u);
        Put(items + 24 * index + 16, static_cast<unsigned>(100 + index));
    }
    void Name(unsigned id, const char* text, bool wide = false)
    {
        const auto size = std::strlen(text);
        auto       at = poolBlock + id * 2;
        Put(at, static_cast<std::uint16_t>((size << 6) | (wide ? 1 : 0)));
        for (std::size_t i = 0; i < size; ++i)
            if (wide)
                Put(at + 2 + i * 2, static_cast<std::uint16_t>(text[i]));
            else
                Put(at + 2 + i, text[i]);
    }
    NativeModelBankReader::Value       model{};
    NativeStreamingModelContext::Value context{};
    void                               SetUp()
    {
        bytes.clear();
        model.slotAddress = mesh + 0xe000;
        model.nativeModelInfo = image + Vehicle::PoolRva;
        context.root = mesh + 0xf000;
        context.provider = mesh + 0x10000;
        context.modelVtable = image + Vehicle::VtableRva;
        context.typeMethod = image + Vehicle::TypeGetterRva;
        context.rows = rows;
        context.count = 1;
        Put(model.slotAddress, model.nativeModelInfo);
        Put(image + Stream::RootRva, context.root);
        Put(context.root + Stream::ProviderOffset, context.provider);
        Put(context.provider + Stream::ProviderRowsOffset, context.rows);
        Put(context.provider + Stream::ProviderCountOffset, context.count);
        Put(image + Vehicle::PoolCountRva, 1);
        Put(model.nativeModelInfo + 0x40, render);
        Put(render + 0x18, model.nativeModelInfo);
        Put(model.nativeModelInfo + 0x58, rows);
        Put(rows + 0x18, 0);
        Put(rows + 0x1c, 100u);
        Put(image + Objects::objectCountRva, 5);
        Put(image + Objects::objectChunksRva, chunks);
        Put(chunks, items);
        Object(mesh, 0, meshClass);
        Object(skeleton, 1, objectClass);
        Object(socket, 2, objectClass);
        Object(objectClass, 3, objectClass);
        Object(meshClass, 4, objectClass);
        Put(image + 0x576e9d8, objectClass);
        Put(image + 0x57e6330, meshClass);
        Put(objectClass + 0x30, chain);
        Put(objectClass + 0x38, 0);
        Put(meshClass + 0x30, chain);
        Put(meshClass + 0x38, 1);
        Put(chain, objectClass + 0x30);
        Put(chain + 8, meshClass + 0x30);

        Put(image + Names::NameInitializedRva, std::uint8_t{1});
        Put(image + Names::NamePoolRva + 8, 0u);
        Put(image + Names::NamePoolRva + 12, 1024u);
        Put(image + Names::NamePoolRva + 16, poolBlock);
        Name(0, "ug_roof");  // A name on the root is not a descendant.
        Name(32, "ug_spoiler");
        Name(64, "UG_LIGHTS", true);
        Name(96, "ug_bonnet");  // A numbered FName cannot equal the bare component name.
        Name(128, "bump_front_dummy", true);
        const Bone         hierarchy[] = {{0, 0, -1}, {32, 0, 0}, {64, 0, 1}, {96, 2, 0}};
        const std::int32_t map[][5] = {{0, 0, 0, 1, 0}, {32, 0, 1, 2, 0}, {64, 0, 2, 3, 0}, {96, 2, 3, -1, 0}};
        Put(boneArray, hierarchy);
        Put(mapRows, map);
        Put(mesh + 0x1c8, ArrayHeader{boneArray, 4, 4});
        Put(mesh + 0x238, ArrayHeader{mapRows, 4, 4});
        Put(mesh + 0x238 + 52, 0);
        Put(mesh + 0x238 + 56, 0);
        Put(mesh + 0x238 + 64, std::uintptr_t{});
        Put(mesh + 0x238 + 72, 1);
        Put(mesh + 0x80, skeleton);
        Put(skeleton + 0x190, ArrayHeader{socketList, 1, 1});
        Put(socketList, socket);
        Put(socket + 0x28, std::uint64_t{128});
        Put(socket + 0x30, std::uint64_t{32});
    }
    bool Select(std::uintptr_t clump = render)
    {
        Observation    obs{image, ReadMemory};
        std::uintptr_t selected = 0;
        return Mesh(obs, model, context, clump, selected) && selected == mesh && obs.Stable();
    }
    bool Children()
    {
        Observation              obs{image, ReadMemory};
        std::vector<std::string> names;
        return Descendants(obs, mesh, names) && obs.Stable();
    }
}
int main()
{
    SetUp();
    Observation registration{image, ReadMemory};
    Check(Registered(registration, model, context) && registration.Stable(), "model slot and streaming provider match the selected context");
    Put(context.root + Stream::ProviderOffset, context.provider + 8);
    Check(!registration.Stable(), "streaming provider changed after the registered model was selected");
    Observation replacedProvider{image, ReadMemory};
    Check(!Registered(replacedProvider, model, context), "another provider cannot supply component names for the registered model");
    SetUp();
    Put(model.slotAddress, model.nativeModelInfo + 8);
    Observation reusedSlot{image, ReadMemory};
    Check(!Registered(reusedSlot, model, context), "model slot reused before selection");
    SetUp();
    Check(Select(), "registered vehicle/render with a matching weak mesh and class chain");
    Check(!Select(render + 8), "wrong supplied render");
    Put(render + 0x18, model.nativeModelInfo + 8);
    Check(!Select(), "render lost its backlink");
    SetUp();
    Put(rows + 0x1c, 99u);
    Check(!Select(), "registry serial no longer matches selected bundle");
    SetUp();
    Put(chain + 8, objectClass + 0x30);
    Check(!Select(), "bundle resolves to an unrelated object class");
    SetUp();
    Observation              obs{image, ReadMemory};
    std::vector<std::string> names;
    Check(Descendants(obs, mesh, names) && obs.Stable(), "complete stable hierarchy and socket observation");
    Check(names == std::vector<std::string>{"ug_spoiler", "UG_LIGHTS", "bump_front_dummy"},
          "unnumbered child names and parented socket retain exact case, exclude root and numbered name");
    Put(items + 2 * 24 + 16, 77u);
    Check(!obs.Stable(), "changed socket membership serial during the read");
    SetUp();
    Observation nameObservation{image, ReadMemory};
    names.clear();
    Check(Descendants(nameObservation, mesh, names), "repeat valid sample");
    Name(32, "ug_bonnet");
    Check(!nameObservation.Stable(), "changed name bytes after enumeration");
    SetUp();
    Put(socket + 0x30, std::uint64_t{999});
    Check(!Children(), "socket has no parent in this mesh");
    SetUp();
    Put(mesh + 0x80, std::uintptr_t{});
    Check(!Children(), "missing skeleton cannot qualify a complete socket list");
    SetUp();
    Put(boneArray + sizeof(Bone) + 8, 1);
    Check(!Children(), "forward/self parent does not reach root");
    SetUp();
    Put(mapRows + 12, 0);
    Check(!Children(), "cyclic map chain");
    SetUp();
    Put(image + Names::NamePoolRva + 12, 66u);
    Check(!Children(), "name storage extends beyond its published cursor");
    SetUp();
    Put(mesh + 0x1c8, ArrayHeader{boneArray, 1, 1});
    Put(mesh + 0x238, ArrayHeader{mapRows, 1, 1});
    Put(mapRows + 12, -1);
    Put(skeleton + 0x190, ArrayHeader{0, 0, 0});
    Observation rootOnly{image, ReadMemory};
    names.clear();
    Check(Descendants(rootOnly, mesh, names) && rootOnly.Stable() && names.empty(), "valid root-only mesh and empty socket list yield empty discovery");
    std::printf("Passed %u native vehicle component reader checks\n", checks);
}
