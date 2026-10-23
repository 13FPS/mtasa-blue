/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Build synthetic legacy-compatible audio archives for actual SDK tests
 ****************************************************************************/
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace audio_files_fixture
{
    using Bytes = std::vector<unsigned char>;

    // Deliberately independent of production struct layout: these files test
    // the on-disk protocol rather than serializing the parser's own structs.
    inline constexpr std::size_t                   LookupRecordBytes = 12;
    inline constexpr std::size_t                   SfxHeaderBytes = 4804;
    inline constexpr std::size_t                   SfxMaximumSounds = 400;
    inline constexpr std::size_t                   RadioHeaderBytes = 8068;
    inline constexpr std::size_t                   RadioLengthsOffset = 8000;
    inline constexpr std::array<unsigned char, 16> RadioXorKey = {
        0xea, 0x3a, 0xc4, 0xa1, 0x9a, 0xa8, 0x14, 0xf3, 0x48, 0xb0, 0xd7, 0x23, 0x9d, 0xe8, 0xff, 0xf1,
    };
    inline constexpr std::array<const char*, 9> SfxNames = {
        "FEET", "GENRL", "PAIN_A", "SCRIPT", "SPC_EA", "SPC_FA", "SPC_GA", "SPC_NA", "SPC_PA",
    };

    inline void SetU32(Bytes& bytes, std::size_t offset, std::uint32_t value)
    {
        if (offset > bytes.size() || bytes.size() - offset < 4)
            throw std::out_of_range("synthetic archive DWORD write");
        for (unsigned int index = 0; index != 4; ++index)
            bytes[offset + index] = static_cast<unsigned char>(value >> (8 * index));
    }

    inline void SetU16(Bytes& bytes, std::size_t offset, std::uint16_t value)
    {
        if (offset > bytes.size() || bytes.size() - offset < 2)
            throw std::out_of_range("synthetic archive WORD write");
        bytes[offset] = static_cast<unsigned char>(value);
        bytes[offset + 1] = static_cast<unsigned char>(value >> 8);
    }

    inline void WriteBytes(const std::filesystem::path& path, const Bytes& bytes)
    {
        if (bytes.size() > static_cast<std::size_t>((std::numeric_limits<std::streamsize>::max)()))
            throw std::length_error("synthetic archive exceeds stream write range");
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("cannot create synthetic archive");
        if (!bytes.empty())
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        stream.close();
        if (!stream)
            throw std::runtime_error("cannot complete synthetic archive write");
    }

    inline Bytes ReadBytes(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
            throw std::runtime_error("cannot open synthetic archive");
        const std::streamoff length = stream.tellg();
        if (length < 0 || static_cast<std::uintmax_t>(length) > (std::numeric_limits<std::size_t>::max)() ||
            length > (std::numeric_limits<std::streamsize>::max)())
            throw std::length_error("synthetic archive exceeds stream read range");
        Bytes bytes(static_cast<std::size_t>(length));
        stream.seekg(0);
        if (!bytes.empty())
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!stream)
            throw std::runtime_error("cannot complete synthetic archive read");
        return bytes;
    }

    inline void CreateSfx(const std::filesystem::path& root, const std::vector<Bytes>& sounds, const std::vector<std::uint16_t>& rates,
                          std::uint8_t lookupIndex = 0, std::uint32_t bankPrefix = 0)
    {
        if (sounds.size() > SfxMaximumSounds || sounds.size() != rates.size() || lookupIndex >= SfxNames.size())
            throw std::invalid_argument("invalid synthetic SFX construction request");
        std::uint64_t payloadBytes = 0;
        for (const auto& sound : sounds)
        {
            if (sound.size() > (std::numeric_limits<std::uint32_t>::max)() - payloadBytes)
                throw std::length_error("synthetic SFX exceeds payload size");
            payloadBytes += sound.size();
        }
        const std::uint64_t archiveBytes = static_cast<std::uint64_t>(bankPrefix) + SfxHeaderBytes + payloadBytes;
        if (payloadBytes > (std::numeric_limits<std::uint32_t>::max)() || archiveBytes > (std::numeric_limits<std::size_t>::max)())
            throw std::length_error("synthetic SFX exceeds format size");

        Bytes archive(static_cast<std::size_t>(archiveBytes));
        // A nonzero prefix distinguishes bank-relative sound offsets from
        // absolute archive offsets without copying any real archive bytes.
        std::memset(archive.data(), 0xa5, bankPrefix);
        const std::size_t bankOffset = bankPrefix;
        SetU32(archive, bankOffset, static_cast<std::uint32_t>(sounds.size()));
        std::uint32_t soundOffset = 0;
        for (std::size_t sound = 0; sound != sounds.size(); ++sound)
        {
            const std::size_t entry = bankOffset + 4 + sound * LookupRecordBytes;
            SetU32(archive, entry, soundOffset);
            SetU16(archive, entry + 8, rates[sound]);
            if (!sounds[sound].empty())
                std::memcpy(archive.data() + bankOffset + SfxHeaderBytes + soundOffset, sounds[sound].data(), sounds[sound].size());
            soundOffset += static_cast<std::uint32_t>(sounds[sound].size());
        }
        Bytes lookup(LookupRecordBytes);
        lookup[0] = lookupIndex;
        SetU32(lookup, 4, bankPrefix);
        SetU32(lookup, 8, static_cast<std::uint32_t>(payloadBytes));
        WriteBytes(root / "SFX" / SfxNames[lookupIndex], archive);
        // Each helper invocation creates one lookup row. Tests which need
        // interleaved banks can compose/mutate those bytes explicitly.
        WriteBytes(root / "CONFIG" / "BankLkup.dat", lookup);
    }

    inline void CreateRadio(const std::filesystem::path& root, const std::vector<Bytes>& tracks, const std::string& archiveName = "AA")
    {
        Bytes archive;
        for (const auto& track : tracks)
        {
            if (track.size() > (std::numeric_limits<std::uint32_t>::max)() || RadioHeaderBytes > archive.max_size() - archive.size() ||
                track.size() > archive.max_size() - archive.size() - RadioHeaderBytes)
                throw std::length_error("synthetic radio exceeds format size");
            const std::size_t start = archive.size();
            archive.resize(start + RadioHeaderBytes + track.size());
            for (unsigned int slot = 0; slot != 8; ++slot)
                SetU32(archive, start + RadioLengthsOffset + slot * 8, 0xcdcdcdcd);
            SetU32(archive, start + RadioLengthsOffset, static_cast<std::uint32_t>(track.size()));
            if (!track.empty())
                std::memcpy(archive.data() + start + RadioHeaderBytes, track.data(), track.size());
        }
        // The cipher position is the absolute file offset, including every
        // header and earlier payload. It must not restart for each track.
        for (std::size_t index = 0; index != archive.size(); ++index)
            archive[index] ^= RadioXorKey[index % RadioXorKey.size()];
        WriteBytes(root / "STREAMS" / archiveName, archive);
    }
}
