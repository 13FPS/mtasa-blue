/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify the existing voice codec across classic and DE architectures
 *****************************************************************************/

#include <speex/speex.h>
#include <speex/speex_bits.h>
#include <speex/speex_preprocess.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    constexpr unsigned                     frameCount = 48;
    constexpr unsigned                     repetitions = 2;
    constexpr unsigned                     settingCount = 3;
    constexpr unsigned                     caseCount = 3 * settingCount * repetitions;
    constexpr unsigned                     maximumPacketBytes = 2048;
    constexpr unsigned                     maximumFixtureBytes = 4 * 1024 * 1024;
    constexpr unsigned                     fixtureVersion = 1;
    constexpr unsigned                     fixtureFlags = 3;  // Existing DTX and preprocessing settings.
    constexpr std::array<unsigned char, 8> magic{'M', 'T', 'A', 'S', 'P', 'X', '1', 0};
    constexpr spx_int16_t                  guard = 0x5a3c;

    unsigned checks = 0;
    unsigned liveEncoders = 0, liveDecoders = 0, livePreprocessors = 0, liveBitsets = 0;
    unsigned encoderCreations = 0, decoderCreations = 0, preprocessorCreations = 0, bitsetCreations = 0;
    unsigned expectedWarnings = 0, completedCases = 0, completedFrames = 0;
    double   minimumCorrelation = 1.0, maximumReconstructionError = 0.0, maximumCrossDecoderRms = 0.0;
    unsigned maximumCrossDecoderDifference = 0;

    void Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }

    struct Setting
    {
        unsigned mode = 0, rate = 0, quality = 0, bitrate = 0;
    };

    Setting CaseSetting(unsigned index)
    {
        Check(index < caseCount, "Fixture case index is bounded");
        const unsigned mode = (index / settingCount) % 3;
        const unsigned setting = index % settingCount;
        // CVoiceRecorder selects these same three modes and accepts either
        // quality alone or a nonzero server bitrate after the quality setting.
        return {mode, 8000u << mode, setting == 0 ? 4u : 8u, setting == 2 ? (11000u << mode) : 0u};
    }

    const SpeexMode* Mode(unsigned mode)
    {
        Check(mode < 3, "Only MTA's three existing Speex modes are admitted");
        return mode == 0 ? &speex_nb_mode : mode == 1 ? &speex_wb_mode : &speex_uwb_mode;
    }

    struct Bits
    {
        SpeexBits state{};

        Bits()
        {
            speex_bits_init(&state);
            Check(state.chars != nullptr, "Speex allocated the actual bit buffer");
            ++liveBitsets;
            ++bitsetCreations;
        }
        ~Bits()
        {
            speex_bits_destroy(&state);
            --liveBitsets;
        }
        Bits(const Bits&) = delete;
        Bits& operator=(const Bits&) = delete;
    };

    struct Encoder
    {
        void*                 state = nullptr;
        SpeexPreprocessState* preprocess = nullptr;
        int                   frameSamples = 0, rate = 0, bitrate = 0, lookahead = 0;

        explicit Encoder(const Setting& setting)
        {
            state = speex_encoder_init(Mode(setting.mode));
            Check(state != nullptr, "Speex constructed the actual encoder");
            ++liveEncoders;
            ++encoderCreations;
            // Finish initialization through a separate function so constructor
            // failure still destroys the native owner instead of leaking it.
            try
            {
                Configure(setting);
            }
            catch (...)
            {
                Retire();
                throw;
            }
        }

        void Configure(const Setting& setting)
        {
            Check(speex_encoder_ctl(state, SPEEX_GET_FRAME_SIZE, &frameSamples) == 0, "MTA encoder frame-size query succeeds");
            Check(frameSamples == static_cast<int>(setting.rate / 50), "Every codec frame contains exactly 20 ms of PCM");
            int quality = static_cast<int>(setting.quality);
            Check(speex_encoder_ctl(state, SPEEX_SET_QUALITY, &quality) == 0, "MTA encoder quality selection succeeds");
            if (setting.bitrate)
            {
                int requested = static_cast<int>(setting.bitrate);
                Check(speex_encoder_ctl(state, SPEEX_SET_BITRATE, &requested) == 0, "MTA's optional bitrate selection succeeds");
            }
            Check(speex_encoder_ctl(state, SPEEX_GET_SAMPLING_RATE, &rate) == 0 && rate == static_cast<int>(setting.rate),
                  "Encoder reports the selected MTA sample rate");
            preprocess = speex_preprocess_state_init(frameSamples, rate);
            Check(preprocess != nullptr, "Speex constructed MTA's actual preprocessor");
            ++livePreprocessors;
            ++preprocessorCreations;
            int enabled = 1, disabled = 0;
            Check(speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_AGC, &enabled) == 0, "MTA enables AGC");
            Check(speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_DENOISE, &enabled) == 0, "MTA enables denoising");
            Check(speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_DEREVERB, &enabled) == 0, "MTA enables the existing dereverb option");
            Check(speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_VAD, &disabled) == 0, "MTA disables VAD");
            // The vendored preprocessor warns unconditionally for this existing
            // MTA VAD request. The driver admits only this exact known warning.
            ++expectedWarnings;
            Check(speex_encoder_ctl(state, SPEEX_SET_DTX, &enabled) == 0, "MTA enables DTX");
            int dtx = 0;
            Check(speex_encoder_ctl(state, SPEEX_GET_DTX, &dtx) == 0 && dtx == 1, "DTX selection survives round-trip query");
            Check(speex_encoder_ctl(state, SPEEX_GET_BITRATE, &bitrate) == 0 && bitrate > 0 && bitrate <= 64000,
                  "Selected bitrate remains a bounded real codec mode");
            Check(speex_encoder_ctl(state, SPEEX_GET_LOOKAHEAD, &lookahead) == 0 && lookahead >= 0 && lookahead <= frameSamples * 2,
                  "Codec latency is bounded separately from packet interoperability");
        }

        void Retire() noexcept
        {
            if (preprocess)
            {
                speex_preprocess_state_destroy(preprocess);
                preprocess = nullptr;
                --livePreprocessors;
            }
            if (state)
            {
                speex_encoder_destroy(state);
                state = nullptr;
                --liveEncoders;
            }
        }
        ~Encoder() { Retire(); }
        Encoder(const Encoder&) = delete;
        Encoder& operator=(const Encoder&) = delete;
    };

    struct Decoder
    {
        void* state = nullptr;
        int   frameSamples = 0;

        explicit Decoder(const Setting& setting)
        {
            state = speex_decoder_init(Mode(setting.mode));
            Check(state != nullptr, "Speex constructed the actual decoder");
            ++liveDecoders;
            ++decoderCreations;
            try
            {
                Check(speex_decoder_ctl(state, SPEEX_GET_FRAME_SIZE, &frameSamples) == 0 && frameSamples == static_cast<int>(setting.rate / 50),
                      "MTA decoder reports exactly the expected frame size");
                int       quality = static_cast<int>(setting.quality);
                const int result = speex_decoder_ctl(state, SPEEX_SET_QUALITY, &quality);
                // CClientPlayerVoice makes this request for every mode. The
                // existing NB decoder rejects it, unlike WB/UWB; preserving
                // that result avoids silently inventing a different MTA policy.
                Check(result == (setting.mode == 0 ? -1 : 0), "Decoder preserves the actual MTA quality-request behavior");
                if (setting.mode == 0)
                    ++expectedWarnings;
            }
            catch (...)
            {
                Retire();
                throw;
            }
        }

        void Retire() noexcept
        {
            if (state)
            {
                speex_decoder_destroy(state);
                state = nullptr;
                --liveDecoders;
            }
        }
        ~Decoder() { Retire(); }
        Decoder(const Decoder&) = delete;
        Decoder& operator=(const Decoder&) = delete;
    };

    void WriteWord(std::ostream& output, unsigned value, unsigned bytes = 4)
    {
        for (unsigned index = 0; index < bytes; ++index)
            output.put(static_cast<char>((value >> (index * 8)) & 255));
        Check(output.good(), "Fixture output remains writable");
    }

    unsigned ReadWord(std::istream& input, unsigned bytes = 4)
    {
        unsigned result = 0;
        for (unsigned index = 0; index < bytes; ++index)
        {
            const int value = input.get();
            Check(value != std::char_traits<char>::eof(), "Fixture scalar must not be truncated");
            result |= static_cast<unsigned>(value) << (index * 8);
        }
        return result;
    }

    void WriteSamples(std::ostream& output, const std::vector<spx_int16_t>& samples)
    {
        for (auto sample : samples)
            WriteWord(output, static_cast<std::uint16_t>(sample), 2);
    }

    std::vector<spx_int16_t> ReadSamples(std::istream& input, unsigned count)
    {
        Check(count >= 160 && count <= 640, "Fixture PCM frame is bounded");
        std::vector<spx_int16_t> result(count);
        for (auto& sample : result)
        {
            const unsigned value = ReadWord(input, 2);
            sample = static_cast<spx_int16_t>(value < 32768 ? static_cast<int>(value) : static_cast<int>(value) - 65536);
        }
        return result;
    }

    std::vector<spx_int16_t> Pcm(const Setting& setting, unsigned frame)
    {
        std::vector<spx_int16_t> result(setting.rate / 50);
        constexpr double         pi = 3.14159265358979323846;
        for (std::size_t index = 0; index < result.size(); ++index)
        {
            const double time = static_cast<double>(static_cast<std::size_t>(frame) * result.size() + index) / setting.rate;
            const double phase = 2 * pi * (145 * time + 105 * time * time);
            const double envelope = 6500 + 2100 * std::sin(2 * pi * 3.2 * time);
            const double value = envelope * (std::sin(phase) + 0.35 * std::sin(2 * phase) + 0.16 * std::sin(3 * phase));
            result[index] = static_cast<spx_int16_t>(value);
        }
        return result;
    }

    std::vector<spx_int16_t> Decode(Decoder& decoder, const std::vector<char>& packet)
    {
        Check(!packet.empty() && packet.size() <= maximumPacketBytes, "MTA voice payload length is bounded");
        Bits bits;
        speex_bits_read_from(&bits.state, packet.data(), static_cast<int>(packet.size()));
        std::vector<spx_int16_t> guarded(static_cast<std::size_t>(decoder.frameSamples) + 8, guard);
        Check(speex_decode_int(decoder.state, &bits.state, guarded.data() + 4) == 0, "Actual Speex decoder accepts the compressed frame");
        Check(bits.state.overflow == 0, "Decoder does not read beyond the advertised packet bytes");
        Check(std::all_of(guarded.begin(), guarded.begin() + 4, [](auto value) { return value == guard; }) &&
                  std::all_of(guarded.end() - 4, guarded.end(), [](auto value) { return value == guard; }),
              "Decoder writes exactly one frame without crossing PCM guards");
        ++completedFrames;
        return {guarded.begin() + 4, guarded.end() - 4};
    }

    void Reconstruction(const std::vector<spx_int16_t>& source, const std::vector<spx_int16_t>& decoded, unsigned frameSamples)
    {
        Check(source.size() == decoded.size() && source.size() == static_cast<std::size_t>(frameCount) * frameSamples,
              "Complete decoded sample count matches the fixture");
        // Speex is lossy and delayed. Search only a bounded codec-delay window
        // after preprocessor warm-up; packet identity is independently checked
        // against the source architecture's decode of these exact same bytes.
        const std::size_t begin = static_cast<std::size_t>(frameSamples) * 8;
        const unsigned    maximumLag = frameSamples * 3;
        const std::size_t end = source.size() - maximumLag;
        double            best = -1;
        unsigned          bestLag = 0;
        for (unsigned lag = 0; lag <= maximumLag; ++lag)
        {
            double inputEnergy = 0, outputEnergy = 0, product = 0;
            for (std::size_t index = begin; index < end; index += 8)
            {
                const double input = source[index], output = decoded[index + lag];
                inputEnergy += input * input;
                outputEnergy += output * output;
                product += input * output;
            }
            const double correlation = product / std::sqrt(std::max(1.0, inputEnergy * outputEnergy));
            if (correlation > best)
            {
                best = correlation;
                bestLag = lag;
            }
        }
        double inputEnergy = 0, outputEnergy = 0, error = 0;
        for (std::size_t index = begin; index < end; ++index)
        {
            const double input = source[index], output = decoded[index + bestLag];
            inputEnergy += input * input;
            outputEnergy += output * output;
            error += (input - output) * (input - output);
        }
        const double rms = std::sqrt(outputEnergy / static_cast<double>(end - begin));
        const double ratio = outputEnergy / std::max(1.0, inputEnergy);
        const double normalizedError = std::sqrt(error / std::max(1.0, inputEnergy));
        Check(std::isfinite(rms) && std::isfinite(ratio) && std::isfinite(normalizedError) && std::isfinite(best),
              "Decoded energy and reconstruction measurements remain finite");
        Check(rms > 100 && ratio >= 0.10 && ratio <= 4.0, "Decoded speech fixture has substantial nonzero bounded energy");
        Check(best >= 0.65 && normalizedError <= 0.95, "Lossy reconstruction remains correlated within a bounded codec delay");
        minimumCorrelation = std::min(minimumCorrelation, best);
        maximumReconstructionError = std::max(maximumReconstructionError, normalizedError);
        std::printf("SPEEX_CASE rate_samples=%zu lag=%u correlation=%.6f normalized_error=%.6f rms=%.3f\n", source.size(), bestLag, best, normalizedError, rms);
    }

    void Retired()
    {
        Check(!liveEncoders && !liveDecoders && !livePreprocessors && !liveBitsets, "Every tracked actual codec owner has been explicitly destroyed");
    }

    void EncodeFixture(const std::filesystem::path& path)
    {
        Check(!std::filesystem::exists(path), "A fresh fixture must not overwrite another run");
        std::ofstream output(path, std::ios::binary);
        Check(output.good(), "Fixture output opened");
        output.write(reinterpret_cast<const char*>(magic.data()), static_cast<std::streamsize>(magic.size()));
        WriteWord(output, fixtureVersion);
        WriteWord(output, static_cast<unsigned>(sizeof(void*)));
        WriteWord(output, caseCount);
        for (unsigned index = 0; index < caseCount; ++index)
        {
            const Setting setting = CaseSetting(index);
            {
                Encoder encoder(setting);
                Decoder decoder(setting);
                Bits    bits;
                // The fixture is test-only: encode each integer explicitly so
                // native pointer width, packing and SpeexBits never cross ABI.
                for (unsigned value : {index, setting.mode, setting.rate, setting.quality, setting.bitrate, static_cast<unsigned>(encoder.frameSamples),
                                       frameCount, static_cast<unsigned>(encoder.lookahead), static_cast<unsigned>(encoder.bitrate), fixtureFlags})
                    WriteWord(output, value);
                std::vector<spx_int16_t> source, reconstructed;
                for (unsigned frame = 0; frame < frameCount; ++frame)
                {
                    auto pcm = Pcm(setting, frame);
                    speex_preprocess_run(encoder.preprocess, pcm.data());
                    source.insert(source.end(), pcm.begin(), pcm.end());
                    const auto referencePcm = pcm;
                    speex_bits_reset(&bits.state);
                    Check(speex_encode_int(encoder.state, pcm.data(), &bits.state) >= 0, "MTA integer encoder accepts the preprocessed frame");
                    std::array<char, maximumPacketBytes> storage{};
                    const int                            bytes = speex_bits_write(&bits.state, storage.data(), static_cast<int>(storage.size()));
                    Check(bytes > 0 && bytes <= static_cast<int>(maximumPacketBytes) && bytes == speex_bits_nbytes(&bits.state),
                          "MTA's 2048-byte output buffer contains the complete encoded frame");
                    const std::vector<char> packet(storage.begin(), storage.begin() + bytes);
                    const auto              decoded = Decode(decoder, packet);
                    reconstructed.insert(reconstructed.end(), decoded.begin(), decoded.end());
                    WriteWord(output, static_cast<unsigned>(bytes));
                    output.write(packet.data(), static_cast<std::streamsize>(packet.size()));
                    WriteSamples(output, referencePcm);
                    WriteSamples(output, decoded);
                }
                Reconstruction(source, reconstructed, static_cast<unsigned>(encoder.frameSamples));
            }
            Retired();
            ++completedCases;
        }
        output.flush();
        Check(output.good(), "Complete fixture flushed before the opposite process reads it");
    }

    void DecodeFixture(const std::filesystem::path& path)
    {
        const auto length = std::filesystem::file_size(path);
        Check(length >= 20 && length <= maximumFixtureBytes, "Interoperability fixture has bounded bytes");
        std::ifstream                           input(path, std::ios::binary);
        std::array<unsigned char, magic.size()> actualMagic{};
        input.read(reinterpret_cast<char*>(actualMagic.data()), static_cast<std::streamsize>(actualMagic.size()));
        Check(input.good() && actualMagic == magic, "Fixture carries the exact versioned test magic");
        Check(ReadWord(input) == fixtureVersion, "Fixture version is supported");
        const unsigned sourcePointerBytes = ReadWord(input);
        Check((sourcePointerBytes == 4 || sourcePointerBytes == 8) && sourcePointerBytes != sizeof(void*),
              "Only an actual opposite-architecture fixture establishes interoperability");
        Check(ReadWord(input) == caseCount, "All expected codec settings and repeated lifetimes are present");
        for (unsigned index = 0; index < caseCount; ++index)
        {
            const Setting setting = CaseSetting(index);
            Check(ReadWord(input) == index && ReadWord(input) == setting.mode && ReadWord(input) == setting.rate && ReadWord(input) == setting.quality &&
                      ReadWord(input) == setting.bitrate,
                  "Fixture setting exactly matches the known MTA codec case");
            const unsigned frameSamples = ReadWord(input), frames = ReadWord(input), lookahead = ReadWord(input), bitrate = ReadWord(input);
            Check(frameSamples == setting.rate / 50 && frames == frameCount && lookahead <= frameSamples * 2 && bitrate > 0 && bitrate <= 64000 &&
                      ReadWord(input) == fixtureFlags,
                  "Fixture frame, rate, preprocessing and DTX fields remain bounded");
            {
                Decoder                  decoder(setting);
                std::vector<spx_int16_t> source, reconstructed;
                double                   squaredDifference = 0;
                unsigned                 maximumDifference = 0;
                for (unsigned frame = 0; frame < frames; ++frame)
                {
                    const unsigned bytes = ReadWord(input);
                    Check(bytes > 0 && bytes <= maximumPacketBytes, "Foreign compressed frame length is bounded before allocation");
                    std::vector<char> packet(bytes);
                    input.read(packet.data(), static_cast<std::streamsize>(packet.size()));
                    Check(input.good(), "Foreign compressed payload must not be truncated");
                    const auto pcm = ReadSamples(input, frameSamples);
                    const auto reference = ReadSamples(input, frameSamples);
                    const auto decoded = Decode(decoder, packet);
                    source.insert(source.end(), pcm.begin(), pcm.end());
                    reconstructed.insert(reconstructed.end(), decoded.begin(), decoded.end());
                    for (std::size_t sample = 0; sample < decoded.size(); ++sample)
                    {
                        const int difference = std::abs(static_cast<int>(decoded[sample]) - static_cast<int>(reference[sample]));
                        maximumDifference = std::max(maximumDifference, static_cast<unsigned>(difference));
                        squaredDifference += static_cast<double>(difference) * difference;
                    }
                }
                const double crossRms = std::sqrt(squaredDifference / static_cast<double>(reconstructed.size()));
                // Floating-point implementation details may differ across x86
                // and x64. This is a tight PCM tolerance for identical packets,
                // separate from the much looser lossy source reconstruction.
                Check(std::isfinite(crossRms) && crossRms <= 16.0 && maximumDifference <= 256,
                      "Opposite-architecture decode agrees with the source decoder within bounded PCM tolerance");
                maximumCrossDecoderRms = std::max(maximumCrossDecoderRms, crossRms);
                maximumCrossDecoderDifference = std::max(maximumCrossDecoderDifference, maximumDifference);
                Reconstruction(source, reconstructed, frameSamples);
            }
            Retired();
            ++completedCases;
        }
        Check(input.peek() == std::char_traits<char>::eof(), "Fixture ends exactly after its declared frames");
    }
}

int main(int count, char** arguments)
{
    try
    {
        static_assert(sizeof(spx_int16_t) == 2);
        Check(count == 3, "Use encode or decode and an owned fixture path");
        const std::string operation = arguments[1];
        Check(operation == "encode" || operation == "decode", "Only explicit fixture operations are supported");
        if (operation == "encode")
            EncodeFixture(arguments[2]);
        else
            DecodeFixture(arguments[2]);
        Retired();
        Check(completedCases == caseCount && completedFrames == caseCount * frameCount, "Every declared case/frame completed");
        std::printf(
            "{\"schema\":\"mta_speex_client_interop_v1\",\"success\":true,\"operation\":\"%s\",\"pointerBytes\":%zu,"
            "\"checks\":%u,\"cases\":%u,\"frames\":%u,\"encoderCreations\":%u,\"decoderCreations\":%u,"
            "\"preprocessorCreations\":%u,\"bitsetCreations\":%u,\"liveOwners\":%u,\"expectedWarnings\":%u,"
            "\"minimumCorrelation\":%.9f,\"maximumReconstructionError\":%.9f,\"maximumCrossDecoderRms\":%.9f,"
            "\"maximumCrossDecoderDifference\":%u,\"heapLeakInstrumented\":false,\"audioHardwareTested\":false,"
            "\"networkStarted\":false,\"gameplayReady\":false,\"fullMtaReady\":false}\n",
            operation.c_str(), sizeof(void*), checks, completedCases, completedFrames, encoderCreations, decoderCreations, preprocessorCreations,
            bitsetCreations, liveEncoders + liveDecoders + livePreprocessors + liveBitsets, expectedWarnings, minimumCorrelation, maximumReconstructionError,
            maximumCrossDecoderRms, maximumCrossDecoderDifference);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "SPEEX_INTEROP_FAILURE %s\n", error.what());
        return 1;
    }
}
