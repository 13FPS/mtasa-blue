/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Complete MTA file-audio reader independent of an engine ABI
 *****************************************************************************/
#pragma once
#include "CAudioContainer.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>
namespace MtaGame
{
    // The explicit root is the directory containing CONFIG, SFX and STREAMS.
    // This reads MTA's existing compatible archive formats, not native engine
    // audio objects. A future backend must qualify its actual supplied source.
    class CAudioContainerFiles final : public CAudioContainer
    {
    public:
        explicit CAudioContainerFiles(std::filesystem::path audioRoot);
        bool GetAudioData(eAudioLookupIndex lookupIndex, int bankIndex, int audioIndex, void*& memory, unsigned int& length) override;
        bool ValidateContainer(eAudioLookupIndex lookupIndex) override;
        bool GetRadioAudioData(eRadioStreamIndex streamIndex, int trackIndex, void*& memory, unsigned int& length) override;
        // This fact covers only a nonempty, well-formed loaded BankLkup.dat.
        // It grants no archive existence, full-source or DE format readiness.
        bool HasLookupData() const noexcept { return m_lookupValid; }

    private:
        struct LookupEntry
        {
            std::uint32_t offset;
            std::uint32_t length;
        };
        void                                    LoadLookup();
        std::filesystem::path                   SfxPath(eAudioLookupIndex index) const;
        std::filesystem::path                   RadioPath(eRadioStreamIndex index) const;
        std::filesystem::path                   m_audioRoot;
        std::array<std::vector<LookupEntry>, 9> m_entries;
        bool                                    m_lookupValid = false;
    };
}
