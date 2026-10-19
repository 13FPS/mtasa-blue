/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve Lua's numeric script handles across pointer-width changes
 *****************************************************************************/

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "../../Shared/sdk/SString.h"

// Only these unrelated function declarations need the normal client prefix.
// The fixture uses the actual Lua headers, SString and ElementID definitions;
// it does not load Lua or provide replacement implementations of its helpers.
class CLuaFunctionRef;
class CVector4D;
class CVector;
class CVector2D;
class CMatrix;
#include "../../Client/mods/deathmatch/logic/lua/LuaCommon.h"

namespace
{
    unsigned int checks = 0;

    void Check(bool condition, const char* description)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", description);
            std::exit(EXIT_FAILURE);
        }
    }
}  // namespace

int main()
{
    static_assert(sizeof(ElementID) == sizeof(std::uint32_t), "ElementID must keep its existing wire width");
    static_assert(sizeof(std::uintptr_t) == sizeof(void*), "The fixture must use the target's real pointer width");

    Check(ElementID().Value() == INVALID_ELEMENT_ID, "Default ElementID must remain invalid");

    // Include the existing element, XML and shared-ID namespaces as well as the
    // transport's full numeric range; accepting UINT32_MAX does not make it a live element.
    constexpr std::array<std::uint32_t, 7> scriptIDs = {0, 1, 0x00020000, 0x01000000, 0x02000000, 0xFFFFFFFE, 0xFFFFFFFF};
    for (const auto id : scriptIDs)
    {
        void* const payload = LuaScriptIDToUserData(id);
        Check(reinterpret_cast<std::uintptr_t>(payload) == id, "Encoding must zero-extend the unchanged numeric ID");

        std::uint32_t decoded = 0xA5A5A5A5;
        Check(LuaUserDataToScriptID(payload, decoded), "Every 32-bit numeric payload must decode");
        Check(decoded == id, "Decoding must preserve all 32 ID bits");
        Check(LuaUserDataToElementID(payload).Value() == id, "Element decoding must preserve the same numeric ID");
    }

    if constexpr (sizeof(std::uintptr_t) > sizeof(std::uint32_t))
    {
        constexpr std::uint32_t                lowID = 0x00020000;
        constexpr std::array<std::uint64_t, 3> widerPayloads = {0x100000000ULL, 0x100020000ULL, 0xFFFFFFFFFFFFFFFFULL};
        for (const auto value : widerPayloads)
        {
            const void* const payload = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(value));
            std::uint32_t     decoded = lowID;
            Check(!LuaUserDataToScriptID(payload, decoded), "Wider native addresses must not decode as numeric handles");
            Check(decoded == lowID, "A refused conversion must leave the caller's output unchanged");
            Check(LuaUserDataToElementID(payload).Value() == INVALID_ELEMENT_ID, "A refused element conversion must return invalid ElementID");
        }

        // The second wider payload has the same low bits as a valid element-ID
        // encoding, which previously allowed a native address to alias that ID.
        const auto valid = LuaScriptIDToUserData(lowID);
        const auto wider = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x100020000ULL));
        Check(LuaUserDataToElementID(valid).Value() == lowID, "The low-ID payload must remain usable");
        Check(LuaUserDataToElementID(wider) != LuaUserDataToElementID(valid), "High address bits must prevent low-ID aliasing");
    }

    std::printf("Passed %u Lua userdata ID checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
