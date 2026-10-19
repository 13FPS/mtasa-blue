/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native CTimer reads without hooks, writes or fabricated CGame values
 *****************************************************************************/
#include "NativeGameTimingReader.h"
#include "NativeGameTimingProfile.h"
#include "NativeClockProfile.h"
#include "NativeImageAccess.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace mta::game_de
{
    namespace
    {
        namespace TimingProfile = NativeGameTimingProfile;
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
