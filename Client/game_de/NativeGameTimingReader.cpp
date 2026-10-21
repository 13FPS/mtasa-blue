/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native CTimer reads without hooks, writes or fabricated CGame values
 *****************************************************************************/
#include "NativeGameTimingReader.h"
#include "NativeGameTimingProfile.h"
#include "NativeGameFPSProfile.h"
#include "NativeClockProfile.h"
#include "NativeImageAccess.h"
#include "NativeGravityReadAccess.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace
    {
        namespace TimingProfile = NativeGameTimingProfile;
        namespace FPSProfile = NativeGameFPSProfile;
        namespace TimingImage = NativeImageAccess;

        template <std::size_t Size>
        bool TimingCode(std::uintptr_t image, std::uintptr_t rva, const std::array<unsigned char, Size>& expected) noexcept
        {
            std::array<unsigned char, Size> actual{};
            return TimingImage::ReadImage(image, image + rva, actual.data(), actual.size(), false, true) && actual == expected;
        }

        bool TimingBinding(std::uintptr_t image) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - NativeClockProfile::imageSize)
                return false;
            // NativeGameTimeReader establishes the exact executable, PE layout
            // and clock span. Independently qualify every new instruction and
            // writable, non-executable data page before using its operand.
            return TimingCode(image, TimingProfile::initializeRva, TimingProfile::initialize) &&
                   TimingCode(image, TimingProfile::elapsedMultiplierRva, TimingProfile::elapsedMultiplier) &&
                   TimingCode(image, TimingProfile::stepUpdateRva, TimingProfile::stepUpdate) &&
                   TimingCode(image, TimingProfile::frameGetterRva, TimingProfile::frameGetter) &&
                   TimingCode(image, TimingProfile::stepConsumerRva, TimingProfile::stepConsumer) &&
                   TimingCode(image, TimingProfile::scriptScaleRva, TimingProfile::scriptScale) &&
                   TimingImage::ImagePages(image, image + TimingProfile::frameCounterRva, sizeof(std::uint32_t), true) &&
                   TimingImage::ImagePages(image, image + TimingProfile::timeStepRva, sizeof(float), true) &&
                   TimingImage::ImagePages(image, image + TimingProfile::oldTimeStepRva, sizeof(float), true) &&
                   TimingImage::ImagePages(image, image + TimingProfile::timeScaleRva, sizeof(float), true);
        }

        bool TimingSample(std::uintptr_t image, const NativeGameTimeReader& clock, NativeGameTimingValues& output) noexcept
        {
            const auto read = [image](std::uintptr_t rva, auto& value) { return TimingImage::ReadImage(image, image + rva, &value, sizeof(value), true); };
            NativeGameTimingValues next;
            std::uint32_t          frameBefore = 0, frameAfter = 0;
            if (!read(TimingProfile::frameCounterRva, frameBefore) || clock.Read(next.clock) != Status::Ok ||
                !read(TimingProfile::frameCounterRva, next.frameCounter) || !read(TimingProfile::timeStepRva, next.timeStep) ||
                !read(TimingProfile::oldTimeStepRva, next.oldTimeStep) || !read(TimingProfile::timeScaleRva, next.timeScale) ||
                !read(TimingProfile::frameCounterRva, frameAfter) || frameBefore != next.frameCounter || frameAfter != next.frameCounter ||
                !std::isfinite(next.timeStep) || !std::isfinite(next.oldTimeStep) || !std::isfinite(next.timeScale))
                return false;
            output = next;
            return true;
        }

        template <typename T>
        bool FPSConstant(std::uintptr_t image, std::uintptr_t rva, T expected) noexcept
        {
            T actual{};
            return TimingImage::ReadImage(image, image + rva, &actual, sizeof(actual)) && std::memcmp(&actual, &expected, sizeof(actual)) == 0;
        }

        bool FPSBinding(std::uintptr_t image) noexcept
        {
            // The completed timing Bind owns the executable identity and thread.
            // FPS has its own native producer/consumer proof, independent of the
            // gameplay clock and its scaling. Recheck all new pins on each read
            // so stale or modified code cannot lend meaning to an arbitrary float.
            return TimingCode(image, FPSProfile::frequencyScaleRva, FPSProfile::frequencyScale) &&
                   TimingCode(image, FPSProfile::computationRva, FPSProfile::computation) && TimingCode(image, FPSProfile::consumerRva, FPSProfile::consumer) &&
                   FPSConstant(image, FPSProfile::previousWeightRva, FPSProfile::previousWeight) &&
                   FPSConstant(image, FPSProfile::currentWeightRva, FPSProfile::currentWeight) &&
                   FPSConstant(image, FPSProfile::fpsNumeratorRva, FPSProfile::fpsNumerator) &&
                   FPSConstant(image, FPSProfile::millisecondsRva, FPSProfile::milliseconds) &&
                   FPSConstant(image, FPSProfile::secondsOriginRva, FPSProfile::secondsOrigin) &&
                   FPSConstant(image, FPSProfile::frequencyNumeratorRva, FPSProfile::frequencyNumerator) &&
                   [&]
            {
                std::array<unsigned char, FPSProfile::label.size()> label{};
                return TimingImage::ReadImage(image, image + FPSProfile::labelRva, label.data(), label.size()) && label == FPSProfile::label;
            }() && TimingImage::ImagePages(image, image + FPSProfile::fpsRva, sizeof(float), true) &&
                   TimingImage::ImagePages(image, image + FPSProfile::averageMsRva, sizeof(float), true) &&
                   TimingImage::ImagePages(image, image + FPSProfile::previousSecondsRva, sizeof(double), true) &&
                   TimingImage::ImagePages(image, image + FPSProfile::secondsPerCycleRva, sizeof(double), true);
        }
    }

    Status NativeGameTimingReader::Bind() noexcept
    {
        const auto thread = GetCurrentThreadId();
        if (ownerThread.load() && ownerThread.load() != thread)
            return Status::WrongThread;
        Reset();
        const auto result = clock.Bind();
        if (result != Status::Ok)
            return result;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!TimingBinding(image))
        {
            clock.Reset();
            return Status::Unsupported;
        }
        imageAddress.store(image);
        ownerThread.store(thread);
        return Status::Ok;
    }

    Status NativeGameTimingReader::Read(NativeGameTimingValues& output) const noexcept
    {
        const auto owner = ownerThread.load();
        if (!owner || !imageAddress.load())
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        const auto             image = imageAddress.load();
        NativeGameTimingValues first, second;
        if (!TimingBinding(image) || !TimingSample(image, clock, first) || !TimingSample(image, clock, second) ||
            std::memcmp(&first, &second, sizeof(first)) != 0 || !TimingBinding(image))
            return Status::Unsupported;
        // No cached success is exposed on an overlapping update, changed pin,
        // invalid scalar or failed OS read. Each CGame getter requests anew.
        output = second;
        return Status::Ok;
    }

    Status NativeGameTimingReader::ReadFPS(float& output) const noexcept
    {
        const auto owner = ownerThread.load();
        if (!owner || !imageAddress.load())
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        const auto image = imageAddress.load();
        float      first = 0, second = 0;
        if (!TimingBinding(image) || !FPSBinding(image) || !TimingImage::ReadImage(image, image + FPSProfile::fpsRva, &first, sizeof(first), true) ||
            !TimingImage::ReadImage(image, image + FPSProfile::fpsRva, &second, sizeof(second), true) || !std::isfinite(first) || first < 0 ||
            !std::isfinite(second) || second < 0 || std::memcmp(&first, &second, sizeof(first)) != 0 || !FPSBinding(image) || !TimingBinding(image))
            return Status::Unsupported;
        // Native division has no zero guard. Refuse invalid or overlapping
        // samples, preserving the caller's value instead of computing an FPS
        // estimate from the independently scaled/clamped gameplay timestep.
        output = second;
        return Status::Ok;
    }

    Status NativeGameTimingReader::ReadGravity(float& output) const noexcept
    {
        const auto owner = ownerThread.load();
        const auto image = imageAddress.load();
        if (!owner || !image)
            return Status::NotInitialized;
        if (owner != GetCurrentThreadId())
            return Status::WrongThread;
        if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)))
            return Status::Unsupported;
        const auto read = [](std::uintptr_t base, std::uintptr_t address, void* destination, std::size_t bytes, bool executable) noexcept
        { return TimingImage::ReadImage(base, address, destination, bytes, false, executable); };
        float next = 0;
        if (!TimingBinding(image) || !NativeGravityReadAccess(image, read).Read(next) || !TimingBinding(image))
            return Status::Unsupported;
        // Reuse the existing bound image and caller-thread ownership. No new
        // native callback, mutable global, or cached default is introduced.
        std::memcpy(&output, &next, sizeof(output));
        return Status::Ok;
    }

    void NativeGameTimingReader::Reset() noexcept
    {
        ownerThread.store(0);
        imageAddress.store(0);
        clock.Reset();
    }

    bool NativeGameTimingReader::IsBound() const noexcept
    {
        return ownerThread.load() != 0 && imageAddress.load() != 0 && clock.IsBound();
    }
}
