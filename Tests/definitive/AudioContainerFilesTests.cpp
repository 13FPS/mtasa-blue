/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Full actual file-audio SDK over small independent synthetic archives
 *****************************************************************************/
#include "../../Client/sdk/game/CAudioContainerFiles.h"
#include "AudioContainerSyntheticArchives.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>
namespace
{
    using namespace audio_files_fixture;
    using MtaGame::CAudioContainerFiles;
    unsigned checks = 0, cases = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    std::filesystem::path Fresh(const std::filesystem::path& root)
    {
        const auto path = root / ("case-" + std::to_string(++cases));
        if (std::filesystem::exists(path))
            throw std::runtime_error("synthetic case already exists");
        std::filesystem::create_directories(path);
        return path;
    }
    Bytes Ogg(std::size_t size, unsigned char value)
    {
        Bytes bytes(size, value);
        if (size >= 4)
            std::memcpy(bytes.data(), "OggS", 4);
        return bytes;
    }
    void Xor(Bytes& bytes)
    {
        for (std::size_t at = 0; at < bytes.size(); ++at)
            bytes[at] ^= RadioXorKey[at % RadioXorKey.size()];
    }
    void SfxFails(CAudioContainer& sdk, eAudioLookupIndex index, int bank, int sound)
    {
        unsigned char sentinel = 0x5a;
        void*         memory = &sentinel;
        unsigned int  length = 0xdeadbeef;
        Check(!sdk.GetAudioData(index, bank, sound, memory, length), "malformed/missing SFX returns false");
        Check(memory == &sentinel && length == 0xdeadbeef && sentinel == 0x5a, "SFX false preserves both caller outputs and ownership");
    }
    void RadioFails(CAudioContainer& sdk, eRadioStreamIndex index, int track)
    {
        unsigned char sentinel = 0x5a;
        void*         memory = &sentinel;
        unsigned int  length = 0xdeadbeef;
        Check(!sdk.GetRadioAudioData(index, track, memory, length), "malformed/missing radio returns false");
        Check(memory == &sentinel && length == 0xdeadbeef && sentinel == 0x5a, "radio false preserves caller outputs and ownership");
    }
    void Wave(CAudioContainer& sdk, eAudioLookupIndex index, int bank, int sound, const Bytes& pcm, std::uint16_t rate)
    {
        void*        memory = nullptr;
        unsigned int length = 0;
        Check(sdk.GetAudioData(index, bank, sound, memory, length), "actual full SFX SDK succeeds");
        std::unique_ptr<unsigned char[]> owner(static_cast<unsigned char*>(memory));
        Check(memory && length == 44 + pcm.size(), "caller owns exact new[] RIFF+PCM extent");
        Bytes expected(44 + pcm.size());
        std::memcpy(expected.data(), "RIFF", 4);
        SetU32(expected, 4, static_cast<std::uint32_t>(36 + pcm.size()));
        std::memcpy(expected.data() + 8, "WAVEfmt ", 8);
        SetU32(expected, 16, 16);
        SetU16(expected, 20, 1);
        SetU16(expected, 22, 1);
        SetU32(expected, 24, rate);
        SetU32(expected, 28, static_cast<std::uint32_t>(rate) * 2);
        SetU16(expected, 32, 2);
        SetU16(expected, 34, 16);
        std::memcpy(expected.data() + 36, "data", 4);
        SetU32(expected, 40, static_cast<std::uint32_t>(pcm.size()));
        std::copy(pcm.begin(), pcm.end(), expected.begin() + 44);
        Check(std::memcmp(memory, expected.data(), expected.size()) == 0, "independent little-endian44-byte mono16PCM header and payload match");
    }
    void Radio(CAudioContainer& sdk, eRadioStreamIndex index, int track, const Bytes& expected)
    {
        void*        memory = nullptr;
        unsigned int length = 0;
        Check(sdk.GetRadioAudioData(index, track, memory, length), "actual full radio SDK succeeds");
        std::unique_ptr<unsigned char[]> owner(static_cast<unsigned char*>(memory));
        Check(memory && length == expected.size() && std::memcmp(memory, expected.data(), expected.size()) == 0,
              "absolute-position XOR and selected Ogg payload match");
    }
    void LookupAndSfx(const std::filesystem::path& root)
    {
        static_assert(!std::is_abstract_v<CAudioContainerFiles> && std::is_base_of_v<CAudioContainer, CAudioContainerFiles>);
        const std::vector<Bytes>         sounds{{0x00, 0x7f, 0x80, 0xff}, {0x11, 0x22, 0x33}, {}};
        const std::vector<std::uint16_t> rates{22050, 65535, 0};
        for (unsigned index = 0; index < 9; ++index)
        {
            auto path = Fresh(root);
            CreateSfx(path, sounds, rates, static_cast<std::uint8_t>(index), 23);
            auto                 ownedPath = path;
            CAudioContainerFiles files(ownedPath);
            ownedPath /= L"unrelated";
            CAudioContainer& sdk = files;
            Check(files.HasLookupData(), "well-formed nonempty lookup fact, independent owned root");
            for (int sound = 0; sound < 3; ++sound)
                Wave(sdk, static_cast<eAudioLookupIndex>(index), 0, sound, sounds[static_cast<std::size_t>(sound)], rates[static_cast<std::size_t>(sound)]);
            for (const int bank : {std::numeric_limits<int>::min(), -1, 1, 256, std::numeric_limits<int>::max()})
                SfxFails(sdk, static_cast<eAudioLookupIndex>(index), bank, 0);
            for (const int sound : {std::numeric_limits<int>::min(), -1, 3, 400, std::numeric_limits<int>::max()})
                SfxFails(sdk, static_cast<eAudioLookupIndex>(index), 0, sound);
            SfxFails(sdk, AUDIO_LOOKUP_RADIO, 0, 0);
        }
        auto                 missing = Fresh(root);
        CAudioContainerFiles unavailable(missing);
        Check(!unavailable.HasLookupData(), "missing lookup preserves nonthrowing classic invalid-install construction");
        SfxFails(unavailable, AUDIO_LOOKUP_FEET, 0, 0);
        for (const unsigned malformed : {0, 1, 2, 3})
        {
            auto path = Fresh(root);
            CreateSfx(path, {{1, 2, 3}}, {8000});
            auto lookup = ReadBytes(path / "CONFIG" / "BankLkup.dat");
            if (malformed == 0)
                lookup.clear();
            else if (malformed == 1)
                lookup[0] = 9;
            else if (malformed == 2)
                lookup.push_back(0xa5);
            else
                lookup.resize(11);
            WriteBytes(path / "CONFIG" / "BankLkup.dat", lookup);
            CAudioContainerFiles files(path);
            Check(!files.HasLookupData(), "empty/invalid-index/partial lookup never publishes a usable lookup fact");
            SfxFails(files, AUDIO_LOOKUP_FEET, 0, 0);
        }
        // Interleaving is filtered encounter order, not a global bank index.
        auto first = Fresh(root), second = Fresh(root), other = Fresh(root), combined = Fresh(root);
        CreateSfx(first, {{1, 2}}, {11025}, 0, 7);
        CreateSfx(second, {{3, 4, 5}}, {16000});
        CreateSfx(other, {{6, 7}}, {32000}, 1);
        auto firstData = ReadBytes(first / "SFX" / "FEET"), secondData = ReadBytes(second / "SFX" / "FEET");
        auto firstLookup = ReadBytes(first / "CONFIG" / "BankLkup.dat"), secondLookup = ReadBytes(second / "CONFIG" / "BankLkup.dat"),
             otherLookup = ReadBytes(other / "CONFIG" / "BankLkup.dat");
        SetU32(secondLookup, 4, static_cast<std::uint32_t>(firstData.size()));
        firstData.insert(firstData.end(), secondData.begin(), secondData.end());
        firstLookup[1] = 0xff;
        firstLookup[2] = 0x80;
        firstLookup[3] = 0x01;
        firstLookup.insert(firstLookup.end(), otherLookup.begin(), otherLookup.end());
        firstLookup.insert(firstLookup.end(), secondLookup.begin(), secondLookup.end());
        WriteBytes(combined / "SFX" / "FEET", firstData);
        WriteBytes(combined / "SFX" / "GENRL", ReadBytes(other / "SFX" / "GENRL"));
        WriteBytes(combined / "CONFIG" / "BankLkup.dat", firstLookup);
        CAudioContainerFiles interleaved(combined);
        Wave(interleaved, AUDIO_LOOKUP_FEET, 0, 0, {1, 2}, 11025);
        Wave(interleaved, AUDIO_LOOKUP_FEET, 1, 0, {3, 4, 5}, 16000);
        Wave(interleaved, AUDIO_LOOKUP_GENRL, 0, 0, {6, 7}, 32000);
        // The lookup is loaded once, but sample data is read from the actual
        // archive on each call; neither behavior is a native lifetime claim.
        WriteBytes(combined / "CONFIG" / "BankLkup.dat", {});
        Wave(interleaved, AUDIO_LOOKUP_FEET, 1, 0, {3, 4, 5}, 16000);
        auto archive = ReadBytes(combined / "SFX" / "FEET");
        archive[7 + SfxHeaderBytes] = 9;
        WriteBytes(combined / "SFX" / "FEET", archive);
        Wave(interleaved, AUDIO_LOOKUP_FEET, 0, 0, {9, 2}, 11025);
        for (unsigned malformed = 0; malformed < 10; ++malformed)
        {
            auto path = Fresh(root);
            CreateSfx(path, {{1, 2}, {3, 4}}, {22050, 44100});
            auto data = ReadBytes(path / "SFX" / "FEET");
            auto lookup = ReadBytes(path / "CONFIG" / "BankLkup.dat");
            if (malformed == 0)
                SetU32(data, 0, 401);
            else if (malformed == 1)
                SetU32(data, 0, 0xffffffff);
            else if (malformed == 2)
                SetU32(data, 4, 5);
            else if (malformed == 3)
            {
                SetU32(data, 4, 2);
                SetU32(data, 16, 1);
            }
            else if (malformed == 4)
                SetU32(data, 16, 5);
            else if (malformed == 5)
                data.resize(SfxHeaderBytes - 1);
            else if (malformed == 6)
                data.resize(data.size() - 1);
            else if (malformed == 7)
                SetU32(lookup, 4, 0xffffffff);
            else if (malformed == 8)
            {
                SetU32(data, 0, 1);
                SetU32(lookup, 8, 2 * 1024 * 1024 + 1);
            }
            else
                SetU32(data, 0, 0);
            WriteBytes(path / "SFX" / "FEET", data);
            WriteBytes(path / "CONFIG" / "BankLkup.dat", lookup);
            CAudioContainerFiles files(path);
            SfxFails(files, AUDIO_LOOKUP_FEET, 0, malformed == 6 ? 1 : 0);
        }
        std::puts("Actual complete MTA file-audio lookup/PCM/RIFF SDK fixture completed");
    }
    void RadioChecks(const std::filesystem::path& root)
    {
        const Bytes                       first = Ogg(4513, 0xa1), second = Ogg(4522, 0x72);
        const std::array<const char*, 16> names{"AA", "ADVERTS", "AMBIENCE", "BEATS", "CH", "CO", "CR", "CUTSCENE",
                                                "DS", "HC",      "MH",       "MR",    "NJ", "RE", "RG", "TK"};
        for (unsigned index = 0; index < names.size(); ++index)
        {
            auto path = Fresh(root);
            CreateRadio(path, {first, second}, names[index]);
            CAudioContainerFiles files(path);
            CAudioContainer&     sdk = files;
            Check(!files.HasLookupData(), "radio success does not manufacture SFX lookup/source readiness");
            for (const int track : {std::numeric_limits<int>::min(), -1, 0, 1})
                Radio(sdk, static_cast<eRadioStreamIndex>(index), track, first);
            Radio(sdk, static_cast<eRadioStreamIndex>(index), 2, second);
            RadioFails(sdk, static_cast<eRadioStreamIndex>(index), 3);
            RadioFails(sdk, static_cast<eRadioStreamIndex>(index), std::numeric_limits<int>::max());
        }
        for (unsigned malformed = 0; malformed < 8; ++malformed)
        {
            auto path = Fresh(root);
            CreateRadio(path, {first, second});
            auto data = ReadBytes(path / "STREAMS" / "AA");
            Xor(data);
            const auto start = RadioHeaderBytes + first.size();
            int        requested = 1;
            if (malformed == 0)
                for (unsigned slot = 0; slot < 8; ++slot)
                    SetU32(data, RadioLengthsOffset + slot * 8, 0xcdcdcdcd);
            else if (malformed == 1)
                SetU32(data, RadioLengthsOffset, 4500);
            else if (malformed == 2)
                data[RadioHeaderBytes] = 'X';
            else if (malformed == 3)
                data.resize(RadioHeaderBytes - 1);
            else if (malformed == 4)
            {
                data.resize(data.size() - 1);
                requested = 2;
            }
            else if (malformed == 5)
                SetU32(data, RadioLengthsOffset, 0xffffffff);
            else if (malformed == 6)
            {
                for (unsigned slot = 0; slot < 8; ++slot)
                    SetU32(data, start + RadioLengthsOffset + slot * 8, 0xcdcdcdcd);
                requested = 2;
            }
            else
                SetU32(data, RadioLengthsOffset, 0);
            Xor(data);
            WriteBytes(path / "STREAMS" / "AA", data);
            CAudioContainerFiles files(path);
            RadioFails(files, RADIO_STREAM_POLICE, requested);
        }
        auto path = Fresh(root);
        CreateRadio(path, {first});
        auto data = ReadBytes(path / "STREAMS" / "AA");
        Xor(data);
        SetU32(data, RadioLengthsOffset, 1);
        SetU32(data, RadioLengthsOffset + 7 * 8, static_cast<std::uint32_t>(first.size()));
        Xor(data);
        WriteBytes(path / "STREAMS" / "AA", data);
        CAudioContainerFiles lastLength(path);
        Radio(lastLength, RADIO_STREAM_POLICE, 1, first);
        auto                 missing = Fresh(root);
        CAudioContainerFiles absent(missing);
        RadioFails(absent, RADIO_STREAM_POLICE, 1);
        std::puts("Actual complete MTA file-audio radio SDK fixture completed");
    }
    void Validation(const std::filesystem::path& root)
    {
        for (const unsigned char fill : {static_cast<unsigned char>(0), static_cast<unsigned char>(0x7f)})
        {
            auto path = Fresh(root);
            WriteBytes(path / "SFX" / "FEET", Bytes(1024 * 1024, fill));
            CAudioContainerFiles files(path);
            Check(files.ValidateContainer(AUDIO_LOOKUP_FEET) == (fill != 0), "existing ten-block zero heuristic retained");
            Check(!files.HasLookupData(), "heuristic validation is independent and does not claim source/lookup readiness");
        }
        auto path = Fresh(root);
        WriteBytes(path / "SFX" / "FEET", {});
        CAudioContainerFiles small(path);
        Check(small.ValidateContainer(AUDIO_LOOKUP_FEET), "existing small-file heuristic behavior retained; not a format assertion");
        Check(!small.ValidateContainer(AUDIO_LOOKUP_GENRL) && !small.ValidateContainer(AUDIO_LOOKUP_RADIO), "missing/unsupported SFX archive returns false");
        std::puts("Actual complete MTA file-audio validation SDK fixture completed");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("owned synthetic fixture directory argument required");
        const auto root = std::filesystem::u8path(argv[1]);
        if (std::filesystem::exists(root))
            throw std::runtime_error("synthetic fixture root already exists");
        std::filesystem::create_directories(root);
        LookupAndSfx(root);
        RadioChecks(root);
        Validation(root);
        std::printf("Passed %u complete file audio SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL %u: %s\n", checks, error.what());
        return 1;
    }
}
