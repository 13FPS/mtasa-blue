/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reject unqualified streaming effects without fake SDK success
 ****************************************************************************/
#include "CStreamingStartupDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    [[noreturn]] void CStreamingStartupDE::Unsupported(const char* operation)
    {
        // Raw native rows, archive management, memory and reset effects have
        // no qualified SDK implementation. Refuse before inspecting pointers,
        // touching caller state or dispatching any unrelated native intent.
        throw std::runtime_error(std::string("DE startup streaming operation is unavailable: ") + operation);
    }
    CStreamingInfo* CStreamingStartupDE::GetStreamingInfo(uint32)
    {
        Unsupported("GetStreamingInfo");
    }
    void CStreamingStartupDE::ReinitStreaming()
    {
        Unsupported("ReinitStreaming");
    }
    unsigned char CStreamingStartupDE::AddArchive(const wchar_t*)
    {
        Unsupported("AddArchive");
    }
    void CStreamingStartupDE::RemoveArchive(unsigned char)
    {
        Unsupported("RemoveArchive");
    }
    void CStreamingStartupDE::SetStreamingInfo(unsigned int, unsigned char, unsigned int, unsigned short, unsigned int)
    {
        Unsupported("SetStreamingInfo");
    }
    bool CStreamingStartupDE::SetStreamingBufferSize(uint32)
    {
        Unsupported("SetStreamingBufferSize");
    }
    uint32 CStreamingStartupDE::GetStreamingBufferSize()
    {
        Unsupported("GetStreamingBufferSize");
    }
    void CStreamingStartupDE::MakeSpaceFor(std::uint32_t)
    {
        Unsupported("MakeSpaceFor");
    }
    std::uint32_t CStreamingStartupDE::GetMemoryUsed() const
    {
        Unsupported("GetMemoryUsed");
    }
    void CStreamingStartupDE::RemoveBigBuildings()
    {
        Unsupported("RemoveBigBuildings");
    }
}
