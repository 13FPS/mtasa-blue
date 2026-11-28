/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read on-foot keyboard bindings saved by the retail DE controls menu
 *****************************************************************************/
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace DefinitiveRetailMovementBindings
{
    struct Binding
    {
        const char* control;
        const char* key;
    };

    // MTA uses these bindings when the retail controls file has no applicable entry.
    inline constexpr std::array<Binding, 6> MtaDefaults = {
        {{"forwards", "w"}, {"backwards", "s"}, {"left", "a"}, {"right", "d"}, {"sprint", "lshift"}, {"walk", "rshift"}}};

    inline constexpr std::size_t MaxFileBytes = 64 * 1024;

    namespace Detail
    {
        struct Action
        {
            std::string_view mapping;
            std::string_view direction;
            const char*      control;
        };

        inline constexpr std::array<Action, 5> Actions = {{{"HID_MAPPING_PED_MOVE_Y", "1", "forwards"},
                                                           {"HID_MAPPING_PED_MOVE_Y", "0", "backwards"},
                                                           {"HID_MAPPING_PED_MOVE_X", "1", "left"},
                                                           {"HID_MAPPING_PED_MOVE_X", "0", "right"},
                                                           {"HID_MAPPING_SPRINT", "0", "sprint"}}};

        // IDs verified against named keys in the retail controls menu and its saved file.
        inline const char* KeyName(std::string_view id)
        {
            struct Key
            {
                std::string_view id;
                const char*      name;
            };
            static constexpr Key keys[] = {{"29", "w"},     {"34", "u"},       {"35", "i"},       {"42", "a"},       {"43", "s"},
                                           {"44", "d"},     {"48", "j"},       {"49", "k"},       {"50", "l"},       {"54", "lshift"},
                                           {"68", "space"}, {"79", "arrow_u"}, {"80", "arrow_d"}, {"81", "arrow_l"}, {"82", "arrow_r"}};
            for (const auto& key : keys)
                if (key.id == id)
                    return key.name;
            return nullptr;
        }

        inline std::string_view NextWord(std::string_view& line)
        {
            constexpr std::string_view whitespace = " \t\r";
            const auto                 start = line.find_first_not_of(whitespace);
            if (start == std::string_view::npos)
            {
                line = {};
                return {};
            }
            line.remove_prefix(start);
            const auto end = line.find_first_of(whitespace);
            const auto word = line.substr(0, end);
            line.remove_prefix(word.size());
            return word;
        }

        inline bool DecodeMappingText(std::string_view& text, std::string& decoded)
        {
            // The profile exporter preserves the original BOM. Mapping names,
            // numbers and separators are ASCII; non-ASCII text can occur in comments.
            struct Encoding
            {
                std::string_view bom;
                unsigned int     width;
                bool             littleEndian;
            };
            static constexpr Encoding encodings[] = {
                {{"\xFF\xFE\0\0", 4}, 4, true}, {{"\0\0\xFE\xFF", 4}, 4, false}, {"\xFF\xFE", 2, true}, {"\xFE\xFF", 2, false}, {"\xEF\xBB\xBF", 1, false}};
            for (const auto& encoding : encodings)
            {
                if (text.substr(0, encoding.bom.size()) != encoding.bom)
                    continue;
                text.remove_prefix(encoding.bom.size());
                if (encoding.width == 1)
                    break;
                if (text.size() % encoding.width)
                    return false;
                decoded.reserve(text.size() / encoding.width);
                bool needsLowSurrogate = false;
                while (!text.empty())
                {
                    std::uint32_t unit = 0;
                    for (unsigned int i = 0; i < encoding.width; ++i)
                        unit = (unit << 8) | static_cast<unsigned char>(text[encoding.littleEndian ? encoding.width - 1 - i : i]);
                    text.remove_prefix(encoding.width);
                    if (!unit || unit > 0x10ffff)
                        return false;
                    if (needsLowSurrogate)
                    {
                        if (unit < 0xdc00 || unit > 0xdfff)
                            return false;
                        needsLowSurrogate = false;
                        continue;
                    }
                    if (unit >= 0xd800 && unit <= 0xdfff)
                    {
                        if (encoding.width != 2 || unit >= 0xdc00)
                            return false;
                        needsLowSurrogate = true;
                    }
                    decoded.push_back(unit < 128 ? static_cast<char>(unit) : '?');
                }
                text = decoded;
                return !needsLowSurrogate;
            }
            return text.find('\0') == std::string_view::npos;
        }
    }

    inline std::vector<Binding> Parse(std::string_view contents)
    {
        std::string decoded;
        if (contents.size() > MaxFileBytes || !Detail::DecodeMappingText(contents, decoded))
            return {};

        struct Slot
        {
            std::string_view id;
            bool             ambiguous{};
        };
        std::array<Slot, Detail::Actions.size()> slots{};
        while (!contents.empty())
        {
            const auto end = contents.find('\n');
            auto       line = contents.substr(0, end);
            contents = end == std::string_view::npos ? std::string_view{} : contents.substr(end + 1);

            const auto id = Detail::NextWord(line);
            const auto mapping = Detail::NextWord(line);
            const auto direction = Detail::NextWord(line);
            const auto device = Detail::NextWord(line);
            if (device != "-1" || !Detail::NextWord(line).empty())
                continue;

            for (std::size_t i = 0; i < slots.size(); ++i)
            {
                const auto& action = Detail::Actions[i];
                if (mapping != action.mapping || direction != action.direction)
                    continue;
                // The file does not label primary/alternate slots. A conflicting pair has no unambiguous priority.
                auto& slot = slots[i];
                if (!slot.id.empty() && slot.id != id)
                    slot.ambiguous = true;
                slot.id = id;
                break;
            }
        }

        std::vector<Binding> bindings;
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i].ambiguous)
                continue;
            if (const char* key = Detail::KeyName(slots[i].id))
                bindings.push_back({Detail::Actions[i].control, key});
        }
        return bindings;
    }

    inline std::vector<Binding> Read(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return {};
        const std::streamoff size = file.tellg();
        if (size <= 0 || size > static_cast<std::streamoff>(MaxFileBytes))
            return {};
        std::string contents(static_cast<std::size_t>(size), '\0');
        file.seekg(0);
        if (!file.read(contents.data(), static_cast<std::streamsize>(size)) || file.peek() != std::ifstream::traits_type::eof() || file.bad())
            return {};
        return Parse(contents);
    }
}
