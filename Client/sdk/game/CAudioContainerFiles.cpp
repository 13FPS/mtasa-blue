/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual MTA lookup/PCM/RIFF/radio file behavior in both backends
 *****************************************************************************/
#include "CAudioContainerFiles.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <utility>
namespace MtaGame
{
    namespace
    {
        constexpr std::size_t                   LookupBytes = 12;
        constexpr std::size_t                   SoundBytes = 12;
        constexpr std::size_t                   MaximumSounds = 400;
        constexpr std::size_t                   BankHeaderBytes = 4 + MaximumSounds * SoundBytes;
        constexpr std::size_t                   WaveHeaderBytes = 44;
        constexpr std::size_t                   RadioHeaderBytes = 8068;
        constexpr std::size_t                   RadioLengthsOffset = 8000;
        constexpr std::size_t                   RadioLengthCount = 8;
        constexpr std::uint32_t                 NoRadioLength = 0xcdcdcdcd;
        constexpr std::size_t                   ValidateBufferBytes = 4096;
        constexpr std::array<unsigned char, 16> RadioXor{0xea, 0x3a, 0xc4, 0xa1, 0x9a, 0xa8, 0x14, 0xf3, 0x48, 0xb0, 0xd7, 0x23, 0x9d, 0xe8, 0xff, 0xf1};

        std::uint16_t Read16(const unsigned char* bytes) noexcept
        {
            return static_cast<std::uint16_t>(bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8));
        }
        std::uint32_t Read32(const unsigned char* bytes) noexcept
        {
            return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) | (static_cast<std::uint32_t>(bytes[2]) << 16) |
                   (static_cast<std::uint32_t>(bytes[3]) << 24);
        }
        void Write16(unsigned char* bytes, std::uint16_t value) noexcept
        {
            bytes[0] = static_cast<unsigned char>(value);
            bytes[1] = static_cast<unsigned char>(value >> 8);
        }
        void Write32(unsigned char* bytes, std::uint32_t value) noexcept
        {
            for (unsigned index = 0; index < 4; ++index)
                bytes[index] = static_cast<unsigned char>(value >> (index * 8));
        }
        bool ReadAt(std::ifstream& stream, std::uint64_t offset, unsigned char* output, std::size_t length)
        {
            if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) ||
                length > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
                return false;
            stream.clear();
            stream.seekg(static_cast<std::streamoff>(offset));
            return stream && static_cast<bool>(stream.read(reinterpret_cast<char*>(output), static_cast<std::streamsize>(length)));
        }
        bool HasRemaining(std::ifstream& stream, std::uint32_t length)
        {
            const auto position = stream.tellg();
            if (position < 0)
                return false;
            stream.seekg(0, std::ios::end);
            const auto end = stream.tellg();
            stream.seekg(position);
            return stream && end >= position && static_cast<std::uint64_t>(end - position) >= length;
        }
        bool ReadRadio(std::ifstream& stream, unsigned char* output, std::size_t length)
        {
            const auto position = stream.tellg();
            if (position < 0 || length > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()) ||
                !stream.read(reinterpret_cast<char*>(output), static_cast<std::streamsize>(length)))
                return false;
            auto keyPosition = static_cast<std::uint64_t>(position) % RadioXor.size();
            for (std::size_t index = 0; index < length; ++index)
            {
                output[index] ^= RadioXor[static_cast<std::size_t>(keyPosition)];
                keyPosition = (keyPosition + 1) % RadioXor.size();
            }
            return true;
        }
        bool RadioSize(const std::array<unsigned char, RadioHeaderBytes>& header, std::uint32_t& output) noexcept
        {
            std::uint32_t selected = 0;
            bool          found = false;
            for (std::size_t index = 0; index < RadioLengthCount; ++index)
            {
                const auto value = Read32(header.data() + RadioLengthsOffset + index * 8);
                if (value != NoRadioLength)
                {
                    selected = value;
                    found = true;
                }
            }
            // Keep the last non-sentinel slot. A malformed subsequent header
            // must not borrow the previous track's length or allocate a signed
            // negative size, both outside the original successful format.
            if (!found || selected == 0 || selected > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
                return false;
            output = selected;
            return true;
        }
    }
    CAudioContainerFiles::CAudioContainerFiles(std::filesystem::path audioRoot) : m_audioRoot(std::move(audioRoot))
    {
        LoadLookup();
    }
    void CAudioContainerFiles::LoadLookup()
    {
        std::ifstream stream(m_audioRoot / "CONFIG" / "BankLkup.dat", std::ios::binary);
        if (!stream)
            return;
        std::array<unsigned char, LookupBytes> record{};
        std::size_t                            count = 0;
        while (stream.read(reinterpret_cast<char*>(record.data()), static_cast<std::streamsize>(record.size())))
        {
            if (record[0] >= m_entries.size())
            {
                for (auto& entries : m_entries)
                    entries.clear();
                return;
            }
            m_entries[record[0]].push_back({Read32(record.data() + 4), Read32(record.data() + 8)});
            ++count;
        }
        if (!stream.eof() || stream.gcount() != 0)
        {
            for (auto& entries : m_entries)
                entries.clear();
            return;
        }
        m_lookupValid = count != 0;
    }
    std::filesystem::path CAudioContainerFiles::SfxPath(eAudioLookupIndex index) const
    {
        constexpr std::array<const char*, 9> names{"FEET", "GENRL", "PAIN_A", "SCRIPT", "SPC_EA", "SPC_FA", "SPC_GA", "SPC_NA", "SPC_PA"};
        const auto                           numeric = static_cast<int>(index);
        if (numeric < 0 || static_cast<std::size_t>(numeric) >= names.size())
            return {};
        return m_audioRoot / "SFX" / names[static_cast<std::size_t>(numeric)];
    }
    std::filesystem::path CAudioContainerFiles::RadioPath(eRadioStreamIndex index) const
    {
        constexpr std::array<const char*, 16> names{"AA", "ADVERTS", "AMBIENCE", "BEATS", "CH", "CO", "CR", "CUTSCENE",
                                                    "DS", "HC",      "MH",       "MR",    "NJ", "RE", "RG", "TK"};
        const auto                            numeric = static_cast<int>(index);
        if (numeric < 0 || static_cast<std::size_t>(numeric) >= names.size())
            return {};
        return m_audioRoot / "STREAMS" / names[static_cast<std::size_t>(numeric)];
    }
    bool CAudioContainerFiles::GetAudioData(eAudioLookupIndex lookupIndex, int bankIndex, int audioIndex, void*& memory, unsigned int& length)
    {
        const auto numeric = static_cast<int>(lookupIndex);
        if (!m_lookupValid || numeric < 0 || static_cast<std::size_t>(numeric) >= m_entries.size() || bankIndex < 0 || bankIndex > 0xff)
            return false;
        const auto& entries = m_entries[static_cast<std::size_t>(numeric)];
        if (static_cast<std::size_t>(bankIndex) >= entries.size())
            return false;
        const auto&                                lookup = entries[static_cast<std::size_t>(bankIndex)];
        std::ifstream                              stream(SfxPath(lookupIndex), std::ios::binary);
        std::array<unsigned char, BankHeaderBytes> header{};
        if (!ReadAt(stream, lookup.offset, header.data(), header.size()))
            return false;
        const auto count = Read32(header.data());
        if (count > MaximumSounds || audioIndex < 0 || static_cast<std::uint32_t>(audioIndex) >= count)
            return false;
        const auto* sound = header.data() + 4 + static_cast<std::size_t>(audioIndex) * SoundBytes;
        const auto  offset = Read32(sound);
        if (offset > lookup.length)
            return false;
        const auto end = static_cast<std::uint32_t>(audioIndex) + 1 < count ? Read32(sound + SoundBytes) : lookup.length;
        if (end < offset || end > lookup.length)
            return false;
        const auto rawLength = end - offset;
        if (rawLength > 2 * 1024 * 1024)
            return false;
        auto buffer = std::make_unique<unsigned char[]>(WaveHeaderBytes + rawLength);
        // Exact fixed-width RIFF PCM representation, independent of host
        // unsigned-long width or structure packing. The caller still owns the
        // final new[] buffer and receives no output on any read failure.
        std::memcpy(buffer.get(), "RIFF", 4);
        Write32(buffer.get() + 4, 36 + rawLength);
        std::memcpy(buffer.get() + 8, "WAVEfmt ", 8);
        Write32(buffer.get() + 16, 16);
        Write16(buffer.get() + 20, 1);
        Write16(buffer.get() + 22, 1);
        const auto rate = Read16(sound + 8);
        Write32(buffer.get() + 24, rate);
        Write32(buffer.get() + 28, static_cast<std::uint32_t>(rate) * 2);
        Write16(buffer.get() + 32, 2);
        Write16(buffer.get() + 34, 16);
        std::memcpy(buffer.get() + 36, "data", 4);
        Write32(buffer.get() + 40, rawLength);
        if (!ReadAt(stream, static_cast<std::uint64_t>(lookup.offset) + BankHeaderBytes + offset, buffer.get() + WaveHeaderBytes, rawLength))
            return false;
        memory = buffer.release();
        length = static_cast<unsigned int>(WaveHeaderBytes + rawLength);
        return true;
    }
    bool CAudioContainerFiles::ValidateContainer(eAudioLookupIndex lookupIndex)
    {
#ifdef MTA_DEBUG
        (void)lookupIndex;
        return true;
#else
        const auto path = SfxPath(lookupIndex);
        if (path.empty())
            return false;
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return false;
        stream.seekg(0, std::ios::end);
        const auto end = stream.tellg();
        if (end < 0)
            return false;
        const auto step = end / static_cast<std::streamoff>(10);
        stream.seekg(0);
        std::array<unsigned char, ValidateBufferBytes> buffer{};
        std::uint64_t                                  zeros = 0;
        while (stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size())))
        {
            zeros += static_cast<std::uint64_t>(std::count(buffer.begin(), buffer.end(), static_cast<unsigned char>(0)));
            stream.seekg(step, std::ios::cur);
        }
        // Retain MTA's ten-block denominator and seek pattern, including its
        // behavior for small existing archives. This is not a format validator.
        return static_cast<float>(zeros) / static_cast<float>(10 * ValidateBufferBytes) < 0.8f;
#endif
    }
    bool CAudioContainerFiles::GetRadioAudioData(eRadioStreamIndex streamIndex, int trackIndex, void*& memory, unsigned int& length)
    {
        const auto path = RadioPath(streamIndex);
        if (path.empty())
            return false;
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return false;
        std::array<unsigned char, RadioHeaderBytes> header{};
        std::uint32_t                               size = 0;
        // Preserve the actual legacy selection: index1 and all smaller signed
        // indices select the first track. No new public numbering is invented.
        for (int track = 1; track < trackIndex; ++track)
        {
            if (!ReadRadio(stream, header.data(), header.size()) || !RadioSize(header, size) || !HasRemaining(stream, size))
                return false;
            stream.seekg(static_cast<std::streamoff>(size), std::ios::cur);
            if (!stream)
                return false;
        }
        if (!ReadRadio(stream, header.data(), header.size()) || !RadioSize(header, size) || size <= 4500 || !HasRemaining(stream, size))
            return false;
        auto buffer = std::make_unique<unsigned char[]>(size);
        if (!ReadRadio(stream, buffer.get(), size) || std::memcmp(buffer.get(), "OggS", 4) != 0)
            return false;
        memory = buffer.release();
        length = size;
        return true;
    }
}
