/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep classic root/ownership semantics while reusing the full reader
 *****************************************************************************/
#include "CAudioContainerSA.h"
CAudioContainerSA::CAudioContainerSA() : m_files(std::filesystem::path{L"AUDIO"})
{
    // Preserve the actual relative classic source and missing-lookup startup
    // behavior. No file-format or path is asserted for another engine backend.
}
CAudioContainerSA::~CAudioContainerSA() = default;
bool CAudioContainerSA::GetAudioData(eAudioLookupIndex lookupIndex, int bankIndex, int audioIndex, void*& memory, unsigned int& length)
{
    return m_files.GetAudioData(lookupIndex, bankIndex, audioIndex, memory, length);
}
bool CAudioContainerSA::ValidateContainer(eAudioLookupIndex lookupIndex)
{
    return m_files.ValidateContainer(lookupIndex);
}
bool CAudioContainerSA::GetRadioAudioData(eRadioStreamIndex streamIndex, int trackIndex, void*& memory, unsigned int& length)
{
    return m_files.GetRadioAudioData(streamIndex, trackIndex, memory, length);
}
