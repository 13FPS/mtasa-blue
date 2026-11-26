// Exercise the saved profile, the public Lua size limit and the gameplay pulse
// with a recording limits receiver. Each scenario runs in a fresh process.
#include "CCVarsInterface.h"
#if __has_include("CStreamingMemorySettings.h")
    #include "CStreamingMemorySettings.h"
#endif
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string_view>

using uint = unsigned int;
namespace SharedUtil
{
    template <typename T>
    T Clamp(T low, T value, T high)
    {
        return std::clamp(value, low, high);
    }
}

struct TextNode
{
    std::optional<std::string> text;
    const char*                GetText() const { return text ? text->c_str() : nullptr; }
};
struct CXMLNodeImpl
{
    TextNode*   m_pNode;
    static bool StringToLong(const char*, long&);
    bool        GetTagContent(unsigned int&);
};
#include "Xml.generated.inl"

struct Variables : CCVarsInterface
{
    TextNode node;
    int      reads{}, writes{};
    bool     Exists(const std::string&) override
    {
        ++reads;
        return node.text.has_value();
    }
    bool Get(const std::string&, unsigned int& value) override
    {
        ++reads;
        return CXMLNodeImpl{&node}.GetTagContent(value);
    }
    bool Get(const std::string&, std::string& value) override
    {
        ++reads;
        value = node.text.value_or("");
        return !value.empty();
    }
    void Set(const std::string&, unsigned int value) override
    {
        ++writes;
        node.text = std::to_string(value);
    }
    bool Get(const std::string&, int&) override { std::abort(); }
    bool Get(const std::string&, float&) override { std::abort(); }
    bool Get(const std::string&, bool&) override { std::abort(); }
    void Set(const std::string&, int) override { std::abort(); }
    void Set(const std::string&, float) override { std::abort(); }
    void Set(const std::string&, bool) override { std::abort(); }
    void Set(const std::string&, const std::string&) override { std::abort(); }
};
struct CCore
{
    Variables  m_ClientVariables;
    size_t     m_CustomStreamingMemoryLimitBytes = 0;
    uint       min = 64, max = 256;
    Variables* GetCVars() { return &m_ClientVariables; }
    bool       IsUsingCustomStreamingMemorySize();
    void       SetCustomStreamingMemory(size_t);
    size_t     GetStreamingMemory();
    uint       GetMinStreamingMemory() { return min; }
    uint       GetMaxStreamingMemory() { return max; }
};
CCore  core;
CCore* g_pCore = &core;
#define CVARS_GET_VALUE m_ClientVariables.GetValue
#include "Core.generated.inl"
#include "Lua.generated.inl"

struct Limits
{
    int  bytes = 17, writes{};
    int  GetStreamingMemory() const { return bytes; }
    bool SetStreamingMemory(int value)
    {
        ++writes;
        bytes = value;
        return true;
    }
};
struct Multiplayer
{
    Limits* limits;
    Limits* GetLimits() { return limits; }
};
Limits       limits;
Multiplayer  multiplayer{&limits};
Multiplayer* g_pMultiplayer = &multiplayer;

void Pulse()
{
#include "Pulse.generated.inl"
}

bool Check(std::string_view scenario)
{
    auto&         vars = core.m_ClientVariables;
    constexpr int mib = 1024 * 1024;
    if (scenario == "override")
    {
        vars.node.text = "bad";
        EngineStreamingSetMemorySize(100001);
        Pulse();
        Pulse();
        if (EngineStreamingGetMemorySize() != 100001 || limits.bytes != 100001 || limits.writes != 1 || vars.reads || vars.writes)
            return false;
        EngineStreamingRestoreMemorySize();
        Pulse();
        return limits.bytes == 256 * mib && vars.node.text == "256" && vars.writes == 1;
    }
    if (scenario == "size-boundaries")
    {
        EngineStreamingSetMemorySize(INT_MAX);
        Pulse();
        if (limits.bytes != INT_MAX)
            return false;
        for (size_t rejected : {size_t(0), size_t(INT_MAX) + 1, SIZE_MAX})
        {
            try
            {
                EngineStreamingSetMemorySize(rejected);
                return false;
            }
            catch (const std::invalid_argument&)
            {
            }
            if (EngineStreamingGetMemorySize() != INT_MAX)
                return false;
        }
        core.SetCustomStreamingMemory(size_t(INT_MAX) + 1);  // Another SDK caller supplies an invalid budget.
        Pulse();
        return limits.writes == 1 && limits.bytes == INT_MAX;
    }
    if (scenario == "late-limits")
    {
        vars.node.text = "128";
        g_pMultiplayer = nullptr;
        Pulse();
        g_pMultiplayer = &multiplayer;
        multiplayer.limits = nullptr;
        Pulse();
        multiplayer.limits = &limits;
        Pulse();
        return limits.bytes == 128 * mib && limits.writes == 1 && vars.writes == 0;
    }
    if (scenario == "late-range")
    {
        core.min = core.max = 0;
        if (EngineStreamingGetMemorySize() != 0 || vars.node.text)
            return false;
        Pulse();
        if (limits.writes || vars.writes)
            return false;
        vars.node.text = "80";
        if (EngineStreamingGetMemorySize() != 80 * mib)
            return false;
        vars.node.text.reset();
        core.min = 64;
        core.max = 256;
        Pulse();
        return limits.bytes == 256 * mib && vars.node.text == "256";
    }
    int  expected = 256;
    bool repair = true;
    if (scenario == "junk")
        vars.node.text = "junk";
    else if (scenario == "suffix")
        vars.node.text = "128junk";
    else if (scenario == "zero")
    {
        vars.node.text = "0";
        expected = 64;
    }
    else if (scenario == "high")
        vars.node.text = "4096";
    else if (scenario == "valid")
    {
        vars.node.text = "128";
        expected = 128;
        repair = false;
    }
    else if (scenario == "legacy-hex")
    {
        vars.node.text = " +0x60 \t";
        expected = 96;
        repair = false;
    }
    else if (scenario != "missing")
        return false;
    const auto original = vars.node.text;
    if (EngineStreamingGetMemorySize() != size_t(expected) * mib || vars.writes || vars.node.text != original)
        return false;
    Pulse();
    Pulse();
    return limits.bytes == expected * mib && limits.writes == 1 && vars.writes == int(repair) &&
           vars.node.text == (repair ? std::to_string(expected) : *original);
}
int main(int argc, char** argv)
{
    return argc == 2 && Check(argv[1]) ? 0 : 1;
}
