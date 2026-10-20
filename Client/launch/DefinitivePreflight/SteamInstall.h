/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Match a validated DE installation to Steam's registered library
 *****************************************************************************/

#pragma once

#include <Windows.h>
#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace DefinitivePreflight::SteamInstall
{
    namespace fs = std::filesystem;
    inline constexpr char AppId[] = "1547000";

    inline void Require(bool valid, const char* reason)
    {
        if (!valid)
            throw std::runtime_error(reason);
    }

    inline fs::path Utf8Path(std::string_view value)
    {
        Require(!value.empty() && value.size() <= 32767 && value.find('\0') == std::string_view::npos, "steam_path_invalid");
        const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
        Require(length > 0, "steam_path_invalid");
        std::wstring wide(static_cast<std::size_t>(length), L'\0');
        Require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide.data(), length) == length,
                "steam_path_invalid");
        return fs::path(wide);
    }

    inline bool SamePath(const fs::path& first, const fs::path& second)
    {
        return _wcsicmp(first.lexically_normal().c_str(), second.lexically_normal().c_str()) == 0;
    }

    struct Value
    {
        bool                                      object = false;
        std::string                               text;
        std::map<std::string, Value, std::less<>> children;

        const Value& Child(std::string_view name, bool expectedObject) const
        {
            const auto found = children.find(name);
            Require(object && found != children.end() && found->second.object == expectedObject, "steam_manifest_invalid");
            return found->second;
        }
    };

    // Steam VDF is not JSON. Parse the bounded quoted-string/object grammar
    // instead of searching text, so nested app IDs, duplicate keys and comments
    // cannot make an unrelated library look like the selected launch target.
    class Parser
    {
        std::string_view input;
        std::size_t      position = 0;
        unsigned int     entries = 0;

        void Space()
        {
            for (;;)
            {
                while (position < input.size() && (input[position] == ' ' || input[position] == '\t' || input[position] == '\r' || input[position] == '\n'))
                    ++position;
                if (input.substr(position, 2) != "//")
                    break;
                while (position < input.size() && input[position] != '\n')
                    ++position;
            }
        }

        std::string Quoted()
        {
            Space();
            Require(position < input.size() && input[position++] == '"', "steam_manifest_invalid");
            std::string value;
            while (position < input.size())
            {
                const char character = input[position++];
                if (character == '"')
                    return value;
                Require(static_cast<unsigned char>(character) >= 32, "steam_manifest_invalid");
                if (character == '\\')
                {
                    Require(position < input.size(), "steam_manifest_invalid");
                    const char escaped = input[position++];
                    Require(escaped == '\\' || escaped == '"', "steam_manifest_invalid");
                    value += escaped;
                }
                else
                    value += character;
                Require(value.size() <= 32767, "steam_manifest_invalid");
            }
            Require(false, "steam_manifest_invalid");
            return {};
        }

        Value Object(unsigned int depth)
        {
            Require(depth <= 8, "steam_manifest_invalid");
            Space();
            Require(position < input.size() && input[position++] == '{', "steam_manifest_invalid");
            Value result;
            result.object = true;
            for (;;)
            {
                Space();
                Require(position < input.size(), "steam_manifest_invalid");
                if (input[position] == '}')
                {
                    ++position;
                    return result;
                }
                const auto key = Quoted();
                Require(!key.empty() && ++entries <= 10000, "steam_manifest_invalid");
                Space();
                Require(position < input.size(), "steam_manifest_invalid");
                Value value;
                if (input[position] == '{')
                    value = Object(depth + 1);
                else
                    value.text = Quoted();
                Require(result.children.emplace(key, std::move(value)).second, "steam_manifest_invalid");
            }
        }

    public:
        explicit Parser(std::string_view text) : input(text)
        {
            Require(!text.empty() && text.size() <= 1024 * 1024 && text.find('\0') == std::string_view::npos, "steam_manifest_invalid");
            if (input.starts_with("\xef\xbb\xbf"))
                position = 3;
        }

        Value Parse(std::string_view root)
        {
            Require(Quoted() == root, "steam_manifest_invalid");
            auto value = Object(1);
            Space();
            Require(position == input.size(), "steam_manifest_invalid");
            return value;
        }
    };

    inline void ValidateRegistration(const fs::path& libraryRoot, const fs::path& installName, std::string_view librariesText, std::string_view appText)
    {
        const auto   libraries = Parser(librariesText).Parse("libraryfolders");
        unsigned int matchingApps = 0;
        bool         selected = false;
        for (const auto& [index, library] : libraries.children)
        {
            Require(!index.empty() && std::all_of(index.begin(), index.end(), [](char c) { return c >= '0' && c <= '9'; }), "steam_manifest_invalid");
            const auto path = Utf8Path(library.Child("path", false).text);
            Require(path.is_absolute(), "steam_path_invalid");
            const auto& apps = library.Child("apps", true);
            if (apps.children.contains(AppId))
            {
                Require(!apps.children.at(AppId).object, "steam_manifest_invalid");
                ++matchingApps;
                selected = SamePath(path, libraryRoot);
            }
        }
        // A copied appmanifest in a staging directory is not enough: Steam must
        // register this app in exactly the library whose files were inspected.
        Require(matchingApps == 1 && selected, "steam_library_not_registered");
        const auto app = Parser(appText).Parse("AppState");
        Require(app.Child("appid", false).text == AppId && app.Child("StateFlags", false).text == "4" &&
                    SamePath(Utf8Path(app.Child("installdir", false).text), installName),
                "steam_app_install_mismatch");
    }
}
