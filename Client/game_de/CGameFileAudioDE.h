/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Supply file-based sound effects and radio to the DE game SDK
 ****************************************************************************/
#pragma once
#include <game/CAudioContainerFiles.h>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>

namespace mta::game_de
{
    template <class GameBase>
    class CGameFileAudioDE : public GameBase
    {
    public:
        CAudioContainer* GetAudioContainer() override
        {
            // The sound manager can borrow this pointer for the game lifetime.
            // Bind once so a later launch setting cannot mix two archives in one session.
            std::call_once(bindAudio, [this] { audio.emplace(ConfiguredAudioRoot()); });
            return &*audio;
        }

    protected:
        using GameBase::GameBase;

    private:
        static std::filesystem::path ConfiguredAudioRoot()
        {
            wchar_t* value = nullptr;
            size_t   size = 0;
            if (_wdupenv_s(&value, &size, L"MTA_DE_AUDIO_ROOT") != 0)
                throw std::bad_alloc();
            const std::unique_ptr<wchar_t, decltype(&std::free)> storage(value, &std::free);
            const std::filesystem::path                          root = value ? value : L"";
            // DE's packaged audio uses a different layout. Only open compatible
            // loose archives if the launcher explicitly selected an absolute directory.
            return root.is_absolute() ? root : std::filesystem::path{};
        }

        std::once_flag                               bindAudio;
        std::optional<MtaGame::CAudioContainerFiles> audio;
    };
}
