/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Validate a DE executable and owned bundle without starting either
 *****************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include "Preflight.h"
#include "SteamInstall.h"
#include <bcrypt.h>
#include <tlhelp32.h>
#include <json-c/json_object.h>
#include <json-c/json_object_iterator.h>
#include <json-c/json_tokener.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    constexpr std::string_view KnownGameHash = "63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f";
    struct Dependency
    {
        std::string_view name, url, sha256;
    };
    constexpr std::array<Dependency, 7> Dependencies{
        {{"cleo_redux_1.5.1.x64.zip", "https://github.com/cleolibrary/CLEO-Redux/releases/download/1.5.1/cleo_redux_1.5.1.x64.zip",
          "4205147c6b62a0393e4ed77f0e59cbbf52715601c5a1ec09c755b0a1320185dd"},
         {"Ultimate-ASI-Loader-NoPDB_x64.zip", "https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader-NoPDB_x64.zip",
          "e5860e7d9a1805267535b65749575b5e406cc6ea3325c7392189c578815045d1"},
         {"CLEO_REDUX_LICENSE.txt", "https://raw.githubusercontent.com/cleolibrary/CLEO-Redux/1.5.1/LICENSE.txt",
          "baec1109f27ba9165e84f9fcd8000029a54fd108895c53cc4fff4ef76799c7c1"},
         {"ULTIMATE_ASI_LOADER_LICENSE.txt", "https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/v9.7.4/license",
          "ceca73c504e39084b0b80d2a437aea9988313d1915418d3c629c3e4a8313f5a1"},
         {"sa_unreal.json", "https://raw.githubusercontent.com/sannybuilder/library/34bedee0be2f57047ed8f0705c81f137ede3d3ce/sa_unreal/sa_unreal.json",
          "cab1b9d6bd25a4fff41fef63aef0fd771a923c0d39cb018f030f44eceb7713f7"},
         {"unknown_x64.json", "https://raw.githubusercontent.com/sannybuilder/library/34bedee0be2f57047ed8f0705c81f137ede3d3ce/unknown_x64/unknown_x64.json",
          "5206809f78515bd0dca716f945f867056f4d5b0da2f9d079caaadf55c0855676"},
         {"SANNY_BUILDER_LIBRARY_README.md", "https://raw.githubusercontent.com/sannybuilder/library/34bedee0be2f57047ed8f0705c81f137ede3d3ce/README.md",
          "359d7c71d42f14bc4277616d6058a6715d13ef3bbdd7567216448d45f4fd721d"}}};
    constexpr std::array<std::string_view, 23> OwnedPaths{"version.dll",
                                                          "cleo_redux64.asi",
                                                          "CLEO/CLEO_PLUGINS/dylib64.cleo",
                                                          "CLEO/CLEO_PLUGINS/Input64.cleo",
                                                          "CLEO/.config/cleo.ini",
                                                          "CLEO/.config/sa_unreal.json",
                                                          "CLEO/.config/unknown_x64.json",
                                                          "CLEO/de-online/index.js",
                                                          "CLEO/de-online/controller.mjs",
                                                          "CLEO/de-online/config.mjs",
                                                          "CLEO/de-online/mod.json",
                                                          "CLEO/de-online/de_client_bridge.dll",
                                                          "CLEO/de-online/native-pad-reference.mjs",
                                                          "CLEO/de-online/native-movement-trial.mjs",
                                                          "CLEO/de-online/game_de.dll",
                                                          "CLEO/de-online/core.dll",
                                                          "CLEO/de-online/pthread.dll",
                                                          "CLEO/de-online/xmll.dll",
                                                          "CLEO/de-online/mta/config/coreconfig.xml",
                                                          "DE_ONLINE_LICENSES/CLEO_REDUX_LICENSE.txt",
                                                          "DE_ONLINE_LICENSES/ULTIMATE_ASI_LOADER_LICENSE.txt",
                                                          "DE_ONLINE_LICENSES/MTA_GPLv3.txt",
                                                          "DE_ONLINE_LICENSES/SANNY_BUILDER_LIBRARY_README.md"};

    constexpr std::array<std::string_view, 13> GuiPaths{"CLEO/de-online/cgui.dll",
                                                        "CLEO/de-online/mta/cgui/sans.ttf",
                                                        "CLEO/de-online/mta/cgui/saheader.ttf",
                                                        "CLEO/de-online/mta/cgui/sagothic.ttf",
                                                        "CLEO/de-online/mta/cgui/Imageset.xsd",
                                                        "CLEO/de-online/mta/cgui/GUIScheme.xsd",
                                                        "CLEO/de-online/mta/cgui/Font.xsd",
                                                        "CLEO/de-online/mta/cgui/Falagard.xsd",
                                                        "CLEO/de-online/mta/cgui/unifont.ttf",
                                                        "CLEO/de-online/skins/Default/CGUI.xml",
                                                        "CLEO/de-online/skins/Default/CGUI.is.xml",
                                                        "CLEO/de-online/skins/Default/CGUI.lnf.xml",
                                                        "CLEO/de-online/skins/Default/CGUI.png"};
    constexpr std::array<std::string_view, 22> FrontendPaths{"CLEO/de-online/mta/cgui/images/background.png",
                                                             "CLEO/de-online/mta/cgui/images/background_logo.png",
                                                             "CLEO/de-online/mta/cgui/images/mta_filler.png",
                                                             "CLEO/de-online/mta/cgui/images/version.png",
                                                             "CLEO/de-online/mta/cgui/images/the_language_icon.png",
                                                             "CLEO/de-online/mta/cgui/images/serverbrowser/locked.png",
                                                             "CLEO/de-online/mta/cgui/images/serverbrowser/search-servers.png",
                                                             "CLEO/de-online/mta/cgui/images/serverbrowser/search-players.png",
                                                             "CLEO/de-online/mta/cgui/images/serverbrowser/refresh.png",
                                                             "CLEO/de-online/mta/cgui/images/serverbrowser/favorite.png",
                                                             "CLEO/de-online/mta/cgui/images/serverbrowser/connect.png",
                                                             "CLEO/de-online/mta/cgui/images/serverbrowser/info.png",
                                                             "CLEO/de-online/mta/cgui/images/serverbrowser/search.png",
                                                             "CLEO/de-online/mta/cgui/images/serverbrowser/trashcan.png",
                                                             "CLEO/de-online/mta/locale/en_US/menu_disconnect.png",
                                                             "CLEO/de-online/mta/locale/en_US/menu_quick_connect.png",
                                                             "CLEO/de-online/mta/locale/en_US/menu_browse_servers.png",
                                                             "CLEO/de-online/mta/locale/en_US/menu_settings.png",
                                                             "CLEO/de-online/mta/locale/en_US/menu_about.png",
                                                             "CLEO/de-online/mta/locale/en_US/menu_quit.png",
                                                             "CLEO/de-online/mta/locale/en_US/latest_news.png",
                                                             "CLEO/de-online/mta/locale/en_US/client.pot"};
    constexpr std::array<std::string_view, 2>  NativeFrontendPaths{"MtaDefinitive.asi", "MtaDefinitive.ini"};
    constexpr std::array<std::string_view, 4>  MessageBoxPaths{"CLEO/de-online/mta/cgui/images/question.png", "CLEO/de-online/mta/cgui/images/info.png",
                                                              "CLEO/de-online/mta/cgui/images/warning.png", "CLEO/de-online/mta/cgui/images/error.png"};

    void Require(bool valid, const char* reason)
    {
        if (!valid)
            throw std::runtime_error(reason);
    }

    struct Handle
    {
        HANDLE value = INVALID_HANDLE_VALUE;
        ~Handle()
        {
            if (value != INVALID_HANDLE_VALUE)
                CloseHandle(value);
        }
    };

    struct Hash
    {
        BCRYPT_ALG_HANDLE  algorithm = nullptr;
        BCRYPT_HASH_HANDLE value = nullptr;
        ~Hash()
        {
            if (value)
                BCryptDestroyHash(value);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    };

    void NoLinks(const fs::path& path)
    {
        Require(path.is_absolute(), "absolute_paths_required");
        for (auto current = path; !current.empty();)
        {
            const DWORD attributes = GetFileAttributesW(current.c_str());
            Require(attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT), "missing_or_linked_path");
            const auto parent = current.parent_path();
            if (parent == current)
                break;
            current = parent;
        }
    }

    template <typename Value>
    Value ReadValue(const std::vector<unsigned char>& bytes, std::size_t offset)
    {
        Require(offset <= bytes.size() && sizeof(Value) <= bytes.size() - offset, "truncated_pe_headers");
        Value value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }

    void Pe64(const std::vector<unsigned char>& prefix, std::uint64_t fileSize, bool dll)
    {
        Require(ReadValue<std::uint16_t>(prefix, 0) == IMAGE_DOS_SIGNATURE, "invalid_pe_signature");
        const auto nt = ReadValue<std::uint32_t>(prefix, 0x3c);
        Require(nt <= 65536 - sizeof(IMAGE_NT_HEADERS64), "pe_headers_out_of_bounds");
        Require(ReadValue<std::uint32_t>(prefix, nt) == IMAGE_NT_SIGNATURE, "invalid_pe_signature");
        const auto header = ReadValue<IMAGE_FILE_HEADER>(prefix, nt + 4);
        Require(header.Machine == IMAGE_FILE_MACHINE_AMD64 && header.NumberOfSections > 0 && header.NumberOfSections <= 96 &&
                    header.SizeOfOptionalHeader >= sizeof(IMAGE_OPTIONAL_HEADER64),
                "amd64_pe32plus_required");
        Require((header.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) != 0 && ((header.Characteristics & IMAGE_FILE_DLL) != 0) == dll, "wrong_pe_image_kind");
        const auto optional = ReadValue<IMAGE_OPTIONAL_HEADER64>(prefix, nt + 24);
        Require(optional.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC && optional.SizeOfImage && optional.SizeOfHeaders <= fileSize, "amd64_pe32plus_required");
        const std::size_t sections = static_cast<std::size_t>(nt) + 24 + header.SizeOfOptionalHeader;
        for (std::size_t index = 0; index < header.NumberOfSections; ++index)
        {
            const auto section = ReadValue<IMAGE_SECTION_HEADER>(prefix, sections + index * sizeof(IMAGE_SECTION_HEADER));
            Require(section.PointerToRawData <= fileSize && section.SizeOfRawData <= fileSize - section.PointerToRawData, "pe_section_file_bounds_invalid");
            const auto mapped = std::max(section.Misc.VirtualSize, section.SizeOfRawData);
            Require(section.VirtualAddress <= optional.SizeOfImage && mapped <= optional.SizeOfImage - section.VirtualAddress,
                    "pe_section_image_bounds_invalid");
        }
    }

    struct FileResult
    {
        std::string                sha256;
        std::vector<unsigned char> prefix;
        std::uint64_t              size = 0;
    };

    FileResult ReadFile(const fs::path& path, std::uint64_t maximum, bool retainAll = false)
    {
        NoLinks(path);
        Handle file{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        Require(file.value != INVALID_HANDLE_VALUE, "file_read_failed");
        BY_HANDLE_FILE_INFORMATION before{};
        Require(GetFileInformationByHandle(file.value, &before) != FALSE && !(before.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), "regular_file_required");
        const auto size = (static_cast<std::uint64_t>(before.nFileSizeHigh) << 32) | before.nFileSizeLow;
        Require(size > 0 && size <= maximum, "file_size_out_of_bounds");
        Hash hash;
        Require(BCryptOpenAlgorithmProvider(&hash.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
                    BCryptCreateHash(hash.algorithm, &hash.value, nullptr, 0, nullptr, 0, 0) >= 0,
                "sha256_unavailable");
        FileResult result;
        result.size = size;
        std::array<unsigned char, 65536> buffer{};
        std::uint64_t                    total = 0;
        for (;;)
        {
            DWORD count = 0;
            Require(::ReadFile(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) != FALSE, "file_read_failed");
            if (!count)
                break;
            total += count;
            Require(total <= maximum && BCryptHashData(hash.value, buffer.data(), count, 0) >= 0, "file_changed_or_hash_failed");
            if (retainAll || result.prefix.empty())
                result.prefix.insert(result.prefix.end(), buffer.begin(), buffer.begin() + count);
        }
        std::array<unsigned char, 32> bytes{};
        BY_HANDLE_FILE_INFORMATION    after{};
        Require(total == size && GetFileInformationByHandle(file.value, &after) != FALSE && before.nFileSizeHigh == after.nFileSizeHigh &&
                    before.nFileSizeLow == after.nFileSizeLow && CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime) == 0 &&
                    BCryptFinishHash(hash.value, bytes.data(), static_cast<ULONG>(bytes.size()), 0) >= 0,
                "file_changed_or_hash_failed");
        constexpr char hex[] = "0123456789abcdef";
        for (const auto byte : bytes)
        {
            result.sha256 += hex[byte >> 4];
            result.sha256 += hex[byte & 15];
        }
        return result;
    }

    json_object* Field(json_object* object, const char* name, json_type type)
    {
        json_object* value = nullptr;
        Require(json_object_object_get_ex(object, name, &value) && json_object_is_type(value, type), "manifest_field_invalid");
        return value;
    }

    std::string_view String(json_object* value)
    {
        const auto text = json_object_get_string(value);
        const auto length = json_object_get_string_len(value);
        Require(text && length >= 0 && static_cast<std::size_t>(length) == std::strlen(text), "manifest_string_invalid");
        return {text, static_cast<std::size_t>(length)};
    }

    void Keys(json_object* object, std::initializer_list<std::string_view> allowed)
    {
        Require(json_object_is_type(object, json_type_object), "manifest_object_required");
        auto       iterator = json_object_iter_begin(object);
        const auto end = json_object_iter_end(object);
        while (!json_object_iter_equal(&iterator, &end))
        {
            const std::string_view key = json_object_iter_peek_name(&iterator);
            Require(std::find(allowed.begin(), allowed.end(), key) != allowed.end(), "manifest_unknown_field");
            json_object_iter_next(&iterator);
        }
    }

    struct Request
    {
        fs::path game, bundle, manifest;
        bool     launch = false;
    };

    Request Parse(bool launch = false)
    {
        int  count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        Require(arguments != nullptr, "command_line_invalid");
        const auto release = std::unique_ptr<wchar_t*, decltype(&LocalFree)>(arguments, &LocalFree);
        Require(count == 8 && std::wstring_view(arguments[1]) == (launch ? L"--de-launch" : L"--de-preflight"), "preflight_arguments_required");
        Request request;
        request.launch = launch;
        std::set<std::wstring_view> seen;
        for (int index = 2; index < count; index += 2)
        {
            const std::wstring_view key = arguments[index];
            Require(seen.insert(key).second, "duplicate_argument");
            const fs::path value = arguments[index + 1];
            Require(value.is_absolute() && value == value.lexically_normal(), "normalized_absolute_paths_required");
            if (key == L"--game-exe")
                request.game = value;
            else if (key == L"--bundle")
                request.bundle = value;
            else if (key == L"--manifest")
                request.manifest = value;
            else
                Require(false, "unknown_argument");
        }
        Require(!request.game.empty() && !request.bundle.empty() && !request.manifest.empty(), "preflight_arguments_required");
        Require(_wcsicmp(request.game.filename().c_str(), L"SanAndreas.exe") == 0, "game_filename_invalid");
        return request;
    }

    std::unique_ptr<json_object, decltype(&json_object_put)> Json(const FileResult& file)
    {
        const auto tokener = std::unique_ptr<json_tokener, decltype(&json_tokener_free)>(json_tokener_new_ex(16), &json_tokener_free);
        Require(tokener != nullptr, "json_parser_unavailable");
        json_tokener_set_flags(tokener.get(), JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
        auto object = std::unique_ptr<json_object, decltype(&json_object_put)>(
            json_tokener_parse_ex(tokener.get(), reinterpret_cast<const char*>(file.prefix.data()), static_cast<int>(file.prefix.size())), &json_object_put);
        Require(object && json_tokener_get_error(tokener.get()) == json_tokener_success && json_tokener_get_parse_end(tokener.get()) == file.prefix.size(),
                "manifest_json_invalid");
        return object;
    }

    void InstalledOwnership(const Request& request, json_object* packageFiles, std::string_view mode)
    {
        Require(DefinitivePreflight::SteamInstall::SamePath(request.bundle, request.game.parent_path()), "installed_bundle_required");
        const auto receipt = Json(ReadFile(request.bundle / L"de-online-install-manifest.json", 65536, true));
        Keys(receipt.get(), {"packageId", "formatVersion", "mode", "gameDirectory", "gameSha256", "files"});
        Require(json_object_object_length(receipt.get()) == 6 && String(Field(receipt.get(), "packageId", json_type_string)) == "de-online-prototype" &&
                    json_object_get_int64(Field(receipt.get(), "formatVersion", json_type_int)) == 1 &&
                    String(Field(receipt.get(), "gameSha256", json_type_string)) == KnownGameHash &&
                    String(Field(receipt.get(), "mode", json_type_string)) == mode &&
                    DefinitivePreflight::SteamInstall::SamePath(
                        DefinitivePreflight::SteamInstall::Utf8Path(String(Field(receipt.get(), "gameDirectory", json_type_string))), request.bundle),
                "installed_manifest_identity_invalid");
        const auto files = Field(receipt.get(), "files", json_type_array);
        Require(json_object_array_length(files) == json_object_array_length(packageFiles), "installed_owned_group_mismatch");
        std::set<std::string_view> seen;
        for (std::size_t index = 0; index < json_object_array_length(files); ++index)
        {
            const auto entry = json_object_array_get_idx(files, index);
            Keys(entry, {"path", "destination", "sha256"});
            Require(json_object_object_length(entry) == 3, "installed_manifest_entry_invalid");
            const auto path = String(Field(entry, "path", json_type_string));
            Require(seen.insert(path).second, "installed_owned_path_mismatch");
            json_object* expected = nullptr;
            for (std::size_t candidate = 0; candidate < json_object_array_length(packageFiles); ++candidate)
            {
                const auto item = json_object_array_get_idx(packageFiles, candidate);
                if (String(Field(item, "path", json_type_string)) == path)
                    expected = item;
            }
            Require(expected && String(Field(entry, "sha256", json_type_string)) == String(Field(expected, "sha256", json_type_string)) &&
                        DefinitivePreflight::SteamInstall::SamePath(
                            DefinitivePreflight::SteamInstall::Utf8Path(String(Field(entry, "destination", json_type_string))),
                            request.bundle / fs::path(std::string(path))),
                    "installed_owned_path_mismatch");
        }
    }

    void SteamRegistration(const Request& request)
    {
        namespace Steam = DefinitivePreflight::SteamInstall;
        const auto win64 = request.game.parent_path();
        const auto binaries = win64.parent_path();
        const auto gameface = binaries.parent_path();
        const auto install = gameface.parent_path();
        const auto common = install.parent_path();
        const auto steamapps = common.parent_path();
        Require(Steam::SamePath(win64.filename(), L"Win64") && Steam::SamePath(binaries.filename(), L"Binaries") &&
                    Steam::SamePath(gameface.filename(), L"Gameface") && Steam::SamePath(common.filename(), L"common") &&
                    Steam::SamePath(steamapps.filename(), L"steamapps"),
                "steam_install_layout_invalid");
        std::array<wchar_t, 32768> steamPath{};
        DWORD                      bytes = static_cast<DWORD>(sizeof(steamPath));
        Require(RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, steamPath.data(), &bytes) == ERROR_SUCCESS &&
                    bytes >= sizeof(wchar_t) && steamPath.back() == L'\0',
                "steam_registration_unavailable");
        const fs::path steamRoot(steamPath.data());
        Require(steamRoot.is_absolute(), "steam_path_invalid");
        const auto libraryFile = ReadFile(steamRoot / L"steamapps" / L"libraryfolders.vdf", 1024 * 1024, true);
        const auto appFile = ReadFile(steamapps / L"appmanifest_1547000.acf", 65536, true);
        Steam::ValidateRegistration(steamapps.parent_path(), install.filename(),
                                    {reinterpret_cast<const char*>(libraryFile.prefix.data()), libraryFile.prefix.size()},
                                    {reinterpret_cast<const char*>(appFile.prefix.data()), appFile.prefix.size()});
    }

    void GameNotRunning()
    {
        Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
        Require(snapshot.value != INVALID_HANDLE_VALUE, "process_snapshot_failed");
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        Require(Process32FirstW(snapshot.value, &entry) != FALSE, "process_snapshot_failed");
        do
        {
            Require(_wcsicmp(entry.szExeFile, L"SanAndreas.exe") != 0, "game_already_running");
        } while (Process32NextW(snapshot.value, &entry));
        Require(GetLastError() == ERROR_NO_MORE_FILES, "process_snapshot_failed");
    }

    unsigned int Validate(const Request& request)
    {
        NoLinks(request.bundle);
        Require(fs::is_directory(request.bundle), "bundle_directory_required");
        const auto game = ReadFile(request.game, 128 * 1024 * 1024);
        Pe64(game.prefix, game.size, false);
        Require(game.sha256 == KnownGameHash, "game_fingerprint_unsupported");
        const auto manifest = ReadFile(request.manifest, 65536, true);
        const auto object = Json(manifest);
        Keys(object.get(), {"packageId", "formatVersion", "mode", "gameSha256", "gameFixedVersion", "apiCommit", "dependencies", "files"});
        Require(json_object_object_length(object.get()) == 8 && String(Field(object.get(), "packageId", json_type_string)) == "de-online-prototype" &&
                    json_object_get_int64(Field(object.get(), "formatVersion", json_type_int)) == 1 &&
                    String(Field(object.get(), "gameSha256", json_type_string)) == KnownGameHash &&
                    String(Field(object.get(), "gameFixedVersion", json_type_string)) == "1.0.113.21181" &&
                    String(Field(object.get(), "apiCommit", json_type_string)) == "34bedee0be2f57047ed8f0705c81f137ede3d3ce",
                "manifest_identity_invalid");
        const auto mode = String(Field(object.get(), "mode", json_type_string));
        Require(mode == "Diagnostic" || mode == "Smoke" || mode == "Online", "manifest_mode_invalid");
        const auto dependencies = Field(object.get(), "dependencies", json_type_array);
        Require(json_object_array_length(dependencies) == Dependencies.size(), "manifest_dependencies_invalid");
        std::set<std::string_view> dependencyNames;
        for (std::size_t index = 0; index < Dependencies.size(); ++index)
        {
            auto dependency = json_object_array_get_idx(dependencies, index);
            Keys(dependency, {"name", "url", "sha256"});
            Require(json_object_object_length(dependency) == 3, "manifest_dependencies_invalid");
            const auto name = String(Field(dependency, "name", json_type_string));
            const auto known = std::find_if(Dependencies.begin(), Dependencies.end(), [name](const Dependency& item) { return item.name == name; });
            Require(known != Dependencies.end() && dependencyNames.insert(name).second && String(Field(dependency, "url", json_type_string)) == known->url &&
                        String(Field(dependency, "sha256", json_type_string)) == known->sha256,
                    "manifest_dependencies_invalid");
        }
        const auto files = Field(object.get(), "files", json_type_array);
        const auto fileCount = json_object_array_length(files);
        Require(fileCount == 23 || fileCount == 36 || fileCount == 58 || fileCount == 60 || fileCount == 62 || fileCount == 64,
                "exact_owned_group_count_required");
        const auto groupCount = [&](const auto& paths)
        {
            std::size_t count = 0;
            for (std::size_t index = 0; index < fileCount; ++index)
            {
                const auto relative = String(Field(json_object_array_get_idx(files, index), "path", json_type_string));
                count += std::find(paths.begin(), paths.end(), relative) != paths.end();
            }
            return count;
        };
        const auto nativeCount = groupCount(NativeFrontendPaths);
        const auto messageBoxCount = groupCount(MessageBoxPaths);
        const bool hasFrontend = fileCount >= 58;
        // Native startup and genuine MTA dialogs add independent complete groups.
        // Their file counts cannot stand in for one another or relax ownership.
        Require((nativeCount == 0 || nativeCount == NativeFrontendPaths.size()) && (messageBoxCount == 0 || messageBoxCount == MessageBoxPaths.size()) &&
                    (!(nativeCount || messageBoxCount) || hasFrontend),
                "exact_owned_group_count_required");
        std::vector<std::string_view> ownedPaths(OwnedPaths.begin(), OwnedPaths.end());
        if (fileCount >= 36)
            ownedPaths.insert(ownedPaths.end(), GuiPaths.begin(), GuiPaths.end());
        if (hasFrontend)
            ownedPaths.insert(ownedPaths.end(), FrontendPaths.begin(), FrontendPaths.end());
        if (nativeCount)
            ownedPaths.insert(ownedPaths.end(), NativeFrontendPaths.begin(), NativeFrontendPaths.end());
        if (messageBoxCount)
            ownedPaths.insert(ownedPaths.end(), MessageBoxPaths.begin(), MessageBoxPaths.end());
        Require(fileCount == ownedPaths.size(), "exact_owned_group_count_required");
        std::string                configuration;
        std::string                nativeIni;
        std::set<std::string_view> seen;
        std::uint64_t              total = 0;
        for (std::size_t index = 0; index < ownedPaths.size(); ++index)
        {
            auto entry = json_object_array_get_idx(files, index);
            Keys(entry, {"path", "sha256"});
            Require(json_object_object_length(entry) == 2, "manifest_file_fields_invalid");
            const auto relative = String(Field(entry, "path", json_type_string));
            const auto expected = String(Field(entry, "sha256", json_type_string));
            Require(std::find(ownedPaths.begin(), ownedPaths.end(), relative) != ownedPaths.end() && seen.insert(relative).second,
                    "owned_path_not_allowed_or_duplicate");
            Require(expected.size() == 64 && std::all_of(expected.begin(), expected.end(),
                                                         [](char value) { return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'); }),
                    "owned_sha256_invalid");
            // Only fixed ASCII ownership names reach this conversion. Unicode
            // comes from the already-wide absolute bundle/command-line paths.
            const auto path = request.bundle / fs::path(std::string(relative));
            // MODE coherence must inspect the whole configuration, including
            // any appended assignment beyond the retained header-sized buffer.
            const auto file = ReadFile(path, relative == "CLEO/de-online/config.mjs" ? 65536 : relative == "MtaDefinitive.ini" ? 1024 : 64 * 1024 * 1024);
            total += file.size;
            Require(total <= 256 * 1024 * 1024 && file.sha256 == expected, "owned_file_hash_mismatch");
            const auto extension = path.extension().wstring();
            if (extension == L".dll" || extension == L".asi" || extension == L".cleo")
                Pe64(file.prefix, file.size, true);
            if (relative == "CLEO/de-online/config.mjs")
            {
                configuration.assign(reinterpret_cast<const char*>(file.prefix.data()), file.prefix.size());
                const auto expectedMode = mode == "Smoke" ? "smoke" : mode == "Online" ? "online" : "diagnostic";
                const auto marker = std::string("export const MODE = '") + expectedMode + "';";
                const auto first = configuration.find("export const MODE =");
                Require(first != std::string_view::npos && std::string_view(configuration).substr(first).starts_with(marker) &&
                            configuration.find("export const MODE =", first + 1) == std::string_view::npos,
                        "manifest_config_mode_mismatch");
            }
            if (relative == "MtaDefinitive.ini")
            {
                nativeIni.assign(reinterpret_cast<const char*>(file.prefix.data()), file.prefix.size());
                // Match the shared package contract while allowing Windows line
                // endings; extra sections or duplicate settings remain refused.
                for (auto position = nativeIni.find("\r\n"); position != std::string::npos; position = nativeIni.find("\r\n", position))
                    nativeIni.erase(position, 1);
            }
        }
        // Owning menu assets cannot arm interactive engine controls. Require one
        // explicit marker and the whole group before accepting a GUI opt-in.
        const auto flag = [&](std::string_view name, bool required)
        {
            const auto prefix = std::string("export const ") + std::string(name) + " = ";
            const auto declaration = std::string("export const ") + std::string(name);
            const auto declarationPosition = [&]()
            {
                auto position = configuration.find(declaration);
                while (position != std::string::npos)
                {
                    const auto after = position + declaration.size();
                    if (after == configuration.size() ||
                        !((configuration[after] >= 'A' && configuration[after] <= 'Z') || (configuration[after] >= 'a' && configuration[after] <= 'z') ||
                          (configuration[after] >= '0' && configuration[after] <= '9') || configuration[after] == '_'))
                        return position;
                    position = configuration.find(declaration, after);
                }
                return std::string::npos;
            }();
            const auto position = configuration.find(prefix);
            Require(position != std::string::npos || !required, "gui_configuration_marker_invalid");
            Require(declarationPosition == position, "gui_configuration_marker_invalid");
            if (position == std::string::npos)
                return false;
            Require(configuration.find(declaration, position + declaration.size()) == std::string::npos, "gui_configuration_marker_invalid");
            const auto value = std::string_view(configuration).substr(position + prefix.size());
            Require(value.starts_with("true;") || value.starts_with("false;"), "gui_configuration_marker_invalid");
            return value.starts_with("true;");
        };
        const bool startup = flag("MTA_CORE_GUI_STARTUP_REFERENCE", fileCount >= 36);
        const bool surface = flag("MTA_CORE_GUI_SURFACE_REFERENCE", false);
        const bool frontend = flag("MTA_CORE_GUI_FRONTEND_REFERENCE", hasFrontend);
        // The Core-owned scoped input trial shares the real frontend assets,
        // but cannot inherit the older script-owned player-control lease.
        const bool frontendInput = flag("MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE", false);
        const bool frontendStartup = flag("MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE", false);
        const bool persistentFrontend = flag("MTA_CORE_PERSISTENT_FRONTEND", false);
        const bool runtime = flag("MTA_CORE_FRONTEND_RUNTIME", false);
        const bool bindingPersistence = flag("MTA_CORE_FRONTEND_BINDING_PERSISTENCE", false);
        const bool nativeHost = flag("MTA_CORE_FRONTEND_NATIVE_HOST", nativeCount != 0);
        Require(!request.launch || nativeHost, "enabled_native_frontend_required");
        Require(!bindingPersistence || runtime, "gui_mode_conflict");
        Require(!nativeHost || (nativeCount == NativeFrontendPaths.size() && runtime && mode == "Smoke"), "native_frontend_configuration_invalid");
        if (nativeCount)
        {
            const auto expectedIni =
                std::string("[MTA]\nEnabled=") + (nativeHost ? "1" : "0") + "\nLocalInput=1\nBindingPersistence=" + (bindingPersistence ? "1" : "0") + "\n";
            Require(nativeIni == expectedIni, "native_frontend_configuration_invalid");
        }
        if (startup || surface || frontend || frontendInput || frontendStartup || persistentFrontend || runtime)
        {
            Require(mode == "Smoke" && fileCount >= 36 && (!(frontend || frontendInput || frontendStartup || persistentFrontend || runtime) || hasFrontend) &&
                        static_cast<unsigned>(startup) + static_cast<unsigned>(surface) + static_cast<unsigned>(frontend) +
                                static_cast<unsigned>(frontendInput) + static_cast<unsigned>(frontendStartup) + static_cast<unsigned>(persistentFrontend) +
                                static_cast<unsigned>(runtime) ==
                            1,
                    "gui_mode_conflict");
            for (const auto name : {"GAIT_REFERENCE", "GAIT_RATE", "GAIT_ROOT_LOCK", "INPUT_REFERENCE", "MTA_NATIVE_PAD_REFERENCE", "MTA_NATIVE_MOVEMENT_TRIAL",
                                    "MTA_CORE_STARTUP_REFERENCE", "MTA_CORE_INPUT_REFERENCE", "MTA_CORE_MOVEMENT_REFERENCE", "MTA_CORE_RENDER_REFERENCE",
                                    "MTA_CORE_RENDER_GPU_REFERENCE", "MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE", "MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE",
                                    "MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE"})
                Require(!flag(name, false), "gui_mode_conflict");
        }
        if (request.launch)
        {
            // A Steam request must inspect the installed native owner, not a
            // valid staging package whose files Steam would never execute.
            InstalledOwnership(request, files, mode);
            SteamRegistration(request);
            GameNotRunning();
        }
        return static_cast<unsigned int>(seen.size());
    }
}

extern "C" __declspec(dllexport) int DE_PrepareHostLaunchV1(DefinitivePreflight::HostLaunchResultV1* result)
{
    if (!result || result->size != sizeof(*result) || result->version != 1)
        return 4;
    *result = {};
    try
    {
        Require(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"xmll.dll"), "unexpected_engine_module");
        result->verifiedFiles = Validate(Parse(true));
        Require(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"xmll.dll"), "unexpected_engine_module");
        strcpy_s(result->status, "host_launch_prepared");
        return 0;
    }
    catch (const std::runtime_error& error)
    {
        result->verifiedFiles = 0;
        strcpy_s(result->status, sizeof(result->status), std::strlen(error.what()) < sizeof(result->status) ? error.what() : "preflight_exception");
    }
    catch (...)
    {
        strcpy_s(result->status, "preflight_exception");
    }
    return 4;
}

extern "C" __declspec(dllexport) int DoWinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    try
    {
        // File coherence is useful before startup precisely because none of the
        // inspected engine/backend DLLs has executed in this external launcher.
        Require(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"xmll.dll"), "unexpected_engine_module");
        const auto verified = Validate(Parse());
        Require(!GetModuleHandleW(L"core.dll") && !GetModuleHandleW(L"game_de.dll") && !GetModuleHandleW(L"xmll.dll"), "unexpected_engine_module");
        return DefinitivePreflight::Status("preflight_valid", 0, 0, verified);
    }
    catch (const std::runtime_error& error)
    {
        return DefinitivePreflight::Status(error.what(), 4);
    }
    catch (...)
    {
        return DefinitivePreflight::Status("preflight_exception", 4);
    }
}
