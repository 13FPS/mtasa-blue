/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared real Atomic/default-box source fixture for World Add/Remove
 ****************************************************************************/
#pragma once
#include "NativeModelObjectGroupFixture.h"
#include "../../Client/game_de/NativeStreamingRequestProfile.h"
#include "../../Client/game_de/NativeWorldBoundRectProfile.h"

namespace NativeWorldBoundRectFixture
{
    using Writer = NativeModelObjectGroupFixture::Writer;
    inline void PublishProvider(std::uintptr_t root, std::uintptr_t provider, std::uintptr_t rows, std::int32_t count)
    {
        // LoadFixture replaces the request-only registry with the typed world
        // graph. Publish an already registered graph object as this fixture's
        // provider, as the actual object-group owner fixture does; do not weaken
        // production UObject membership to admit the now-unregistered old one.
        NativeModelObjectGroupFixture::Put(reinterpret_cast<unsigned char*>(root), mta::game_de::NativeStreamingRequestProfile::ProviderOffset, provider);
        NativeModelObjectGroupFixture::Put(reinterpret_cast<unsigned char*>(provider), mta::game_de::NativeStreamingRequestProfile::ProviderRowsOffset, rows);
        NativeModelObjectGroupFixture::Put(reinterpret_cast<unsigned char*>(provider), mta::game_de::NativeStreamingRequestProfile::ProviderCountOffset, count);
    }
    inline void Install(std::uintptr_t image, Writer writer, void* host, std::uint32_t used = 2, std::uint32_t firstModel = 17)
    {
        // Reuse the real counted normalAtomic allocation, not arbitrary heap
        // metadata with a forged AL1 getter. The default box is genuine static
        // image storage; no engine loader or original function is executed.
        NativeModelObjectGroupFixture::Install(image, writer, host, used, firstModel);
        // The reused native model context requires both actual virtual methods
        // to be executable. GP's table already publishes this genuine base
        // GetAnimFileIndex leaf, but the request-only fixture had populated
        // only its old unrelated animation page. Seed the real complete leaf
        // instead of weakening production method-page ownership checks.
        const std::array<unsigned char, 8> noAnimation = {0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc3};
        writer(host, mta::game_de::NativeModelObjectGroupProfile::VtableMethods[15], noAnimation.data(), noAnimation.size(), PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativeWorldBoundRectProfile::Pins)
            writer(host, pin.rva, pin.bytes, pin.size,
                   pin.code                                                              ? PAGE_EXECUTE_READ
                   : pin.rva == mta::game_de::NativeWorldBoundRectProfile::DefaultBoxRva ? PAGE_READWRITE
                                                                                         : PAGE_READONLY);
        for (const auto& slot : mta::game_de::NativeWorldAddProfile::Slots)
        {
            const auto target = image + slot.target;
            writer(host, slot.rva, &target, sizeof(target), PAGE_READONLY);
        }
    }
}
