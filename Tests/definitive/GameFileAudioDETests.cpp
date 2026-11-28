// Exercise the same file-audio override used in the DE startup game through CGame.
#include "../../Client/game_de/CGameModelLodResetDE.h"
#include "AudioContainerSyntheticArchives.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#pragma warning(push)
#pragma warning(disable : 4100)  // Unrelated SDK fixture methods cannot consume their parameters.
#include "OtherGameMethods.generated.h"
#pragma warning(pop)

using Game = mta::game_de::CGameFileAudioDE<OtherGameMethods>;
template <class Base>
constexpr bool UsesFileAudio(CAudioContainer* (mta::game_de::CGameFileAudioDE<Base>::*)())
{
    return true;
}
static_assert(UsesFileAudio(&mta::game_de::CGameModelLodResetDE::GetAudioContainer));

void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void SelectRoot(const std::filesystem::path& root)
{
    Check(_wputenv_s(L"MTA_DE_AUDIO_ROOT", root.c_str()) == 0, "select fixture archive root");
}

#include "GTASfxPlaybackFixture.h"

void CheckUnavailable(CGame& game)
{
    CAudioContainer* container = game.GetAudioContainer();
    int              sentinel = 0;
    void*            memory = &sentinel;
    unsigned         length = 47;
    Check(container && !container->ValidateContainer(AUDIO_LOOKUP_FEET), "unconfigured SFX unavailable");
    Check(!container->GetAudioData(AUDIO_LOOKUP_FEET, 0, 0, memory, length) && memory == &sentinel && length == 47, "unconfigured SFX preserves outputs");
    Check(!container->GetRadioAudioData(RADIO_STREAM_POLICE, 1, memory, length) && memory == &sentinel && length == 47, "unconfigured radio preserves outputs");
}

void CheckAvailable(CGame& game, bool fixture)
{
    auto* container = game.GetAudioContainer();
    Check(container && container == game.GetAudioContainer(), "one stable audio container per game");
    Check(container->ValidateContainer(AUDIO_LOOKUP_FEET), "validate the selected archive");
    void*    memory = nullptr;
    unsigned length = 0;
    Check(container->GetAudioData(AUDIO_LOOKUP_FEET, 0, 0, memory, length), "read first FEET SFX via game SDK");
    std::unique_ptr<unsigned char[]> wave(static_cast<unsigned char*>(memory));
    Check(length > 44 && !std::memcmp(wave.get(), "RIFF", 4) && !std::memcmp(wave.get() + 8, "WAVE", 4), "valid nonempty RIFF/WAVE");
    if (fixture)
        Check(length == 48 && !std::memcmp(wave.get() + 44, "SFX!", 4), "exact synthetic PCM payload");
    std::printf("SFX RIFF/WAVE: %u bytes\n", length);
    Check(container->GetRadioAudioData(RADIO_STREAM_POLICE, 1, memory, length), "read first police radio track via game SDK");
    std::unique_ptr<unsigned char[]> radio(static_cast<unsigned char*>(memory));
    Check(length > 4500 && !std::memcmp(radio.get(), "OggS", 4), "nonempty Ogg stream");
    if (fixture)
        Check(length == 5000 && radio[4999] == 17, "exact synthetic radio tail");
    std::printf("Police radio Ogg: %u bytes\n", length);
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc == 2)
        {
            SelectRoot(argv[1]);
            Game installed;
            CheckAvailable(installed, false);
#ifdef MTA_TEST_BASS_DECODE
            // Device zero is the BASS no-sound device. Decoding stays in memory.
            Check(BASS_Init(0, 44100, 0, nullptr, nullptr) != 0, "start BASS without an output device");
#endif
            sfx_playback_fixture::Run(installed, true);
#ifdef MTA_TEST_BASS_DECODE
            Check(BASS_Free() != 0, "release BASS");
#endif
            return 0;
        }
        const auto path = std::filesystem::current_path();
        audio_files_fixture::CreateSfx(path, {{'S', 'F', 'X', '!'}}, {22050});
        audio_files_fixture::Bytes track(5000, 17);
        std::memcpy(track.data(), "OggS", 4);
        audio_files_fixture::CreateRadio(path, {track});
        SelectRoot({});
        Game missing;
        CheckUnavailable(missing);
        SelectRoot(L".");
        Game relative;
        CheckUnavailable(relative);
        SelectRoot(path);
        Game   selected;
        CGame& sdk = selected;
        auto*  original = sdk.GetAudioContainer();
        SelectRoot({});
        Check(original == sdk.GetAudioContainer(), "later environment changes preserve a live game binding");
        CheckAvailable(sdk, true);
        sfx_playback_fixture::Run(sdk, false);
        CheckUnavailable(missing);
        Game next;
        CheckUnavailable(next);
        return 0;
    }
    catch (const std::exception& failure)
    {
        std::fprintf(stderr, "%s\n", failure.what());
        return 1;
    }
}
