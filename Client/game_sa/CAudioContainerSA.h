/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the classic audio source over MTA's common file reader
 *****************************************************************************/
#pragma once
#include <game/CAudioContainerFiles.h>
class CAudioContainerSA : public CAudioContainer
{
public:
    CAudioContainerSA();
    ~CAudioContainerSA();
    bool GetAudioData(eAudioLookupIndex lookupIndex, int bankIndex, int audioIndex, void*& memory, unsigned int& length) override;
    bool ValidateContainer(eAudioLookupIndex lookupIndex) override;
    bool GetRadioAudioData(eRadioStreamIndex streamIndex, int trackIndex, void*& memory, unsigned int& length) override;

private:
    MtaGame::CAudioContainerFiles m_files;
};
