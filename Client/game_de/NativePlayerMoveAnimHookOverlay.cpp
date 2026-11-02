/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compare one exact acknowledged overlay and every remaining stock byte
 ****************************************************************************/
#include "NativePlayerMoveAnimHookOverlay.h"
#include "NativePlayerMoveAnimHookProfile.h"
#include <cstring>
#include <limits>

namespace mta::game_de::NativePlayerMoveAnimHookOverlay
{
    namespace Bridge = NativePlayerMoveAnimHookBridge;
    namespace Profile = NativePlayerMoveAnimHookProfile;

    void Patch::Invalidate() noexcept
    {
        installedVerified = false;
        image = site = stub = 0;
        generation = admissionSerial = 0;
        kind = Bridge::Kind::ProcessCompare;
        bytes.fill(0);
        size = 0;
    }

    bool Patch::Matches(std::uintptr_t expectedImage, const unsigned char* span, std::size_t spanBytes, Bridge::Kind expectedKind) const noexcept
    {
        const auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
        if (sizeof(void*) != 8 || !installedVerified || !generation || !admissionSerial || !expectedImage || image != expectedImage ||
            expectedImage > maximum - Profile::ImageSize || !stub || kind != expectedKind || !span)
            return false;

        const unsigned char* original = nullptr;
        std::size_t          expectedBytes = 0, overlayOffset = 0, overlayBytes = 0;
        std::uintptr_t       siteRva = 0;
        if (expectedKind == Bridge::Kind::ProcessCompare)
        {
            original = Profile::ProcessContext.data();
            expectedBytes = Profile::ProcessContext.size();
            overlayOffset = Profile::ProcessOverlayOffset;
            overlayBytes = Profile::ProcessCompareBytes.size();
            siteRva = Profile::ProcessCompareRva;
        }
        else if (expectedKind == Bridge::Kind::ClothesCall)
        {
            original = NativePedRebuildProfile::Entry.data();
            expectedBytes = NativePedRebuildProfile::Entry.size();
            overlayOffset = Profile::ClothesRebuildOverlayOffset;
            overlayBytes = Profile::ClothesCallBytes.size();
            siteRva = Profile::ClothesCallRva;
        }
        else
            return false;

        if (spanBytes != expectedBytes || site != expectedImage + siteRva || size != overlayBytes || overlayOffset > expectedBytes ||
            overlayBytes > expectedBytes - overlayOffset || reinterpret_cast<std::uintptr_t>(span) > maximum - spanBytes)
            return false;

        Bridge::CallPatch expected;
        if (!Bridge::EncodeCallPatch(site, stub, expectedKind, expected) || expected.size != size ||
            std::memcmp(bytes.data(), expected.bytes.data(), bytes.size()) != 0)
            return false;

        // The stored acknowledgement alone is insufficient: re-encode its
        // actual stub displacement, check every installed byte and compare all
        // other bytes against the original profile. There is no ignored span.
        const auto suffix = overlayOffset + overlayBytes;
        return std::memcmp(span, original, overlayOffset) == 0 && std::memcmp(span + overlayOffset, expected.bytes.data(), overlayBytes) == 0 &&
               std::memcmp(span + suffix, original + suffix, expectedBytes - suffix) == 0;
    }

    bool Patch::MatchesProcess(std::uintptr_t expectedImage, const unsigned char* span, std::size_t spanBytes) const noexcept
    {
        return Matches(expectedImage, span, spanBytes, Bridge::Kind::ProcessCompare);
    }

    bool Patch::MatchesRebuild(std::uintptr_t expectedImage, const unsigned char* span, std::size_t spanBytes) const noexcept
    {
        return Matches(expectedImage, span, spanBytes, Bridge::Kind::ClothesCall);
    }

    bool MatchesProcess(const Patch& patch, std::uintptr_t image, const unsigned char* span, std::size_t spanBytes) noexcept
    {
        return patch.MatchesProcess(image, span, spanBytes);
    }

    bool MatchesRebuild(const Patch& patch, std::uintptr_t image, const unsigned char* span, std::size_t spanBytes) noexcept
    {
        return patch.MatchesRebuild(image, span, spanBytes);
    }
}
