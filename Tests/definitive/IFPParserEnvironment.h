/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Substitute only the unrelated client shell around real IFP code
 ****************************************************************************/
#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
#include <bitset>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <game/CAnimManager.h>

// Strings and entity registration are environment services here. File-format
// parsing, checked reads, transfer and cleanup come from actual client TUs.
class SString : public std::string
{
public:
    using std::string::string;
    SString() = default;
    SString(std::string value) : std::string(std::move(value)) {}
            operator const char*() const noexcept { return c_str(); }
    SString ToLower() const
    {
        SString value = *this;
        for (auto& byte : value)
            byte = static_cast<char>(std::tolower(static_cast<unsigned char>(byte)));
        return value;
    }
};
class CClientManager
{
};
using ElementID = std::uint32_t;
enum eClientEntityType
{
    CCLIENTIFP
};
struct CVector
{
    float x = 0, y = 0, z = 0;
};
class CClientEntity
{
public:
    explicit CClientEntity(ElementID) {}
    virtual ~CClientEntity() = default;
    virtual eClientEntityType GetType() const = 0;
    void                      SetSmartPointer(bool) noexcept {}
    void                      SetTypeName(const char*) noexcept {}

protected:
    CClientManager* m_pManager = nullptr;
};
class CClientIFP;
class IFPFixtureGameShell
{
public:
    CAnimManager* animations = nullptr;
    CAnimManager* GetAnimManager() const noexcept { return animations; }
};
class IFPFixtureClientGameShell
{
public:
    std::shared_ptr<CClientIFP> GetIFPPointerFromMap(unsigned int) { return {}; }
    void                        RemoveIFPPointerFromMap(unsigned int) noexcept {}
    void                        OnClientIFPUnload(const std::shared_ptr<CClientIFP>&) noexcept {}
};
inline IFPFixtureGameShell*       g_pGame = nullptr;
inline IFPFixtureClientGameShell* g_pClientGame = nullptr;
inline bool                       FileLoad(const std::nothrow_t&, const SString&, SString&) noexcept
{
    std::fputs("The IFP fixture only supplies raw buffers, not a file service\n", stderr);
    std::abort();
}
