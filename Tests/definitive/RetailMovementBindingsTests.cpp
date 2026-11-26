// Verify the retail keyboard importer using synthetic excerpts and an isolated temporary directory.
#include "../../Client/core/DefinitiveRetailMovementBindings.h"

#include <iostream>
#include <iterator>
#include <utility>

namespace Retail = DefinitiveRetailMovementBindings;
using Expected = std::vector<std::pair<std::string, std::string>>;

bool Check(std::string_view name, const std::vector<Retail::Binding>& bindings, const Expected& expected)
{
    Expected actual;
    for (const auto& bind : bindings)
        actual.emplace_back(bind.control, bind.key);
    if (actual == expected)
        return true;
    std::cerr << name << " failed\n";
    return false;
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const auto file = std::filesystem::path(argv[1]) / "KeyboardMappings.cfg";
    if (std::filesystem::exists(file))
        return 2;

    bool ok = Check("missing file", Retail::Read(file), {});
    ok &= Check("empty", Retail::Parse(" \r\n\t"), {});
    ok &= Check("default rows",
                Retail::Parse("44 HID_MAPPING_PED_MOVE_X 0 -1\n29 HID_MAPPING_PED_MOVE_Y 1 -1\n54 HID_MAPPING_SPRINT 0 -1\n"
                              "43 HID_MAPPING_PED_MOVE_Y 0 -1\n42 HID_MAPPING_PED_MOVE_X 1 -1\n"),
                {{"forwards", "w"}, {"backwards", "s"}, {"left", "a"}, {"right", "d"}, {"sprint", "lshift"}});
    ok &= Check("arrow rows",
                Retail::Parse(" \t81 HID_MAPPING_PED_MOVE_X 1 -1\r\n82\tHID_MAPPING_PED_MOVE_X\t0 -1\r\n"
                              "79 HID_MAPPING_PED_MOVE_Y 1 -1\r\n80 HID_MAPPING_PED_MOVE_Y 0 -1"),
                {{"forwards", "arrow_u"}, {"backwards", "arrow_d"}, {"left", "arrow_l"}, {"right", "arrow_r"}});
    ok &= Check("custom keys and repeated row", Retail::Parse("49 HID_MAPPING_PED_MOVE_Y 1 -1\n49 HID_MAPPING_PED_MOVE_Y 1 -1\n68 HID_MAPPING_SPRINT 0 -1\n"),
                {{"forwards", "k"}, {"sprint", "space"}});
    ok &= Check("retail custom movement layout",
                Retail::Parse("34 HID_MAPPING_SPRINT 0 -1\n48 HID_MAPPING_PED_MOVE_X 1 -1\n50 HID_MAPPING_PED_MOVE_X 0 -1\n"
                              "49 HID_MAPPING_PED_MOVE_Y 0 -1\n35 HID_MAPPING_PED_MOVE_Y 1 -1\n"),
                {{"forwards", "i"}, {"backwards", "k"}, {"left", "j"}, {"right", "l"}, {"sprint", "u"}});
    ok &= Check("unknown keys, directions, actions and devices",
                Retail::Parse("9999 HID_MAPPING_PED_MOVE_Y 1 -1\n42 HID_MAPPING_PED_MOVE_X 2 -1\n54 HID_MAPPING_SPRINT 1 -1\n"
                              "42 HID_MAPPING_PED_MOVE_X 1 0\n127 HID_MAPPING_SPRINT 0 2\n43 HID_MAPPING_WALK 0 -1\n"),
                {});
    ok &= Check("malformed rows",
                Retail::Parse("44 HID_MAPPING_PED_MOVE_X 0\n42 HID_MAPPING_PED_MOVE_X 1 -1 tail\n44bad HID_MAPPING_PED_MOVE_X 0 -1\n"
                              "x49 HID_MAPPING_PED_MOVE_Y 1 -1\n49 HID_MAPPING_PED_MOVE_Y 1 -1bad\n"),
                {});
    ok &= Check("conflicting rows",
                Retail::Parse("49 HID_MAPPING_PED_MOVE_Y 1 -1\n29 HID_MAPPING_PED_MOVE_Y 1 -1\n29 HID_MAPPING_PED_MOVE_Y 1 -1\n"
                              "42 HID_MAPPING_PED_MOVE_X 1 -1\n9999 HID_MAPPING_PED_MOVE_X 1 -1\n44 HID_MAPPING_PED_MOVE_X 0 -1\n"),
                {{"right", "d"}});

    std::string sample = "49 HID_MAPPING_PED_MOVE_Y 1 -1\n";
    std::string binary = sample;
    binary.push_back('\0');
    ok &= Check("embedded NUL", Retail::Parse(binary), {});
    sample.resize(Retail::MaxFileBytes, ' ');
    {
        std::ofstream output(file, std::ios::binary);
        if (!output.write(sample.data(), static_cast<std::streamsize>(sample.size())))
            return 2;
    }
    ok &= Check("boundary file", Retail::Read(file), {{"forwards", "k"}});
    std::ifstream     readBack(file, std::ios::binary);
    const std::string original(std::istreambuf_iterator<char>{readBack}, {});
    if (original != sample)
        return 1;
    readBack.close();
    sample.push_back(' ');
    ok &= Check("oversize buffer", Retail::Parse(sample), {});
    {
        std::ofstream output(file, std::ios::binary | std::ios::app);
        if (!output.put(' '))
            return 2;
    }
    ok &= Check("oversize file", Retail::Read(file), {});
    return ok ? 0 : 1;
}
