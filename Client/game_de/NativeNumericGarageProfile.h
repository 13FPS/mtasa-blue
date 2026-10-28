/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact source-qualified startup descriptors without a native ordinal assumption
 ****************************************************************************/
#pragma once
#include "NativeNumericGarageAccess.h"
#include <game/CGarageInitializationPolicy.h>
#include <array>

namespace mta::game_de::NativeNumericGarageProfile
{
    // Reference: source-qualified legacy-normalized comparison 17f09e3944499940
    // 35ee4894159ffb5678e45b8e330da230c9ff785947fff067. Classic API SDK IDs
    // match all ten exact scalar fields independently of DE's current order.
    // Keep all fourteen DE geometry words to fence the selected row later.
    // The observed count fences this certified published set; it is not a
    // claim about native array capacity. A different set must be requalified.
    inline constexpr std::uint32_t                                 CertifiedPublishedCount = 50;
    inline constexpr std::array<NativeNumericGarageDescriptor, 16> Descriptors{
        {{7,
          {{0x6d, 0x6f, 0x64, 0x6c, 0x61, 0x73, 0x74, 0x00}},
          {{{0x45250c7b, 0xc5001fd7, 0x4148b021, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x4181a5e3, 0x4102e100, 0x41270a00, 0x45250c7b, 0x45258f5d,
             0xc5001fd7, 0xc4fef19a}}}},
         {8,
          {{0x73, 0x70, 0x72, 0x4c, 0x41, 0x65, 0x00, 0x00}},
          {{{0x4500899a, 0xc4e57ccd, 0x4148b574, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x41945aba, 0x416b3300, 0x410ee180, 0x4500899a, 0x450174ce,
             0xc4e57ccd, 0xc4e45f0a}}}},
         {10,
          {{0x62, 0x6f, 0x64, 0x4c, 0x41, 0x77, 0x4e, 0x00}},
          {{{0x4481c7ae, 0xc4803571, 0x41f8d254, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x421c692a, 0x41f0e140, 0x41f50220, 0x4481c7ae, 0x44858b33,
             0xc4803571, 0xc478c2d1}}}},
         {11,
          {{0x73, 0x70, 0x72, 0x4c, 0x41, 0x65, 0x00, 0x00}},
          {{{0x447f73d7, 0xc47ead71, 0x41f743ca, 0x00000000, 0xbf800000, 0x3f800000, 0x00000000, 0x4213a1e5, 0x412d1ec0, 0x40c3d780, 0x447f73d7, 0x44807dc3,
             0xc480b0f6, 0xc47ead71}}}},
         {12,
          {{0x73, 0x70, 0x4c, 0x41, 0x77, 0x32, 0x00, 0x00}},
          {{{0x43f58d2f, 0xc4da719a, 0x41174856, 0x3e0b009b, 0x3f7da164, 0xbf7da268, 0x3e0ae2e1, 0x418c851f, 0x413e3dca, 0x41137574, 0x43f0fc6a, 0x43f65bc7,
             0xc4da719a, 0xc4d8d0a4}}}},
         {15,
          {{0x6d, 0x64, 0x73, 0x31, 0x53, 0x46, 0x53, 0x00}},
          {{{0xc52a887b, 0x43544b85, 0x405cdf26, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x410afe28, 0x4142b800, 0x4125fbf0, 0xc52a887b, 0xc529c5c3,
             0x43544b85, 0x435eab44}}}},
         {18,
          {{0x6d, 0x64, 0x73, 0x53, 0x46, 0x53, 0x65, 0x00}},
          {{{0xc4f2a148, 0x437bb6c9, 0x4205b5a8, 0x00000000, 0xbf800000, 0x3f800000, 0x00000000, 0x421aa752, 0x4142c4a0, 0x41261480, 0xc4f2a148, 0xc4f1551f,
             0x436f8a7f, 0x437bb6c9}}}},
         {19,
          {{0x73, 0x70, 0x72, 0x73, 0x66, 0x73, 0x65, 0x00}},
          {{{0xc4ee9dc3, 0x43922d2f, 0x42202a4b, 0x00000000, 0xbf800000, 0x3f800000, 0x00000000, 0x423627f0, 0x4165d2e0, 0x410d1f00, 0xc4ee9dc3, 0xc4ed8385,
             0x438afe98, 0x43922d2f}}}},
         {24,
          {{0x6d, 0x69, 0x63, 0x68, 0x64, 0x72, 0x00, 0x00}},
          {{{0xc4dfdf0a, 0x449736b8, 0x41be1aa0, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x41e7b439, 0x41063d80, 0x413c0000, 0xc4dfdf0a, 0xc4ded28e,
             0x449736b8, 0x4498aeb8}}}},
         {27,
          {{0x73, 0x70, 0x72, 0x73, 0x66, 0x77, 0x00, 0x00}},
          {{{0xc517e214, 0x447d6d71, 0x42455d7e, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x425d5d7e, 0x410ee100, 0x4160ccc0, 0xc517e214, 0xc5175333,
             0x447d6d71, 0x44807852}}}},
         {32,
          {{0x74, 0x69, 0x6d, 0x79, 0x31, 0x00, 0x00, 0x00}},
          {{{0x4515599a, 0x44b96852, 0x411d184a, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x417af213, 0x41082900, 0x41694780, 0x4515599a, 0x4515e1c3,
             0x44b96852, 0x44bb3ae2}}}},
         {33,
          {{0x76, 0x45, 0x63, 0x6d, 0x6f, 0x64, 0x00, 0x00}},
          {{{0x4514e47b, 0x44828000, 0x411d56d6, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x415ef0d8, 0x410dc200, 0x417bae00, 0x4514e47b, 0x4515723d,
             0x44828000, 0x4484775c}}}},
         {36,
          {{0x62, 0x6c, 0x6f, 0x62, 0x31, 0x00, 0x00, 0x00}},
          {{{0x44f6075c, 0x4506de14, 0x41198d26, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x4181e4c3, 0x41717080, 0x41126700, 0x44f6075c, 0x44f7ea3d,
             0x4506de14, 0x4507707c}}}},
         {40,
          {{0x43, 0x4e, 0x73, 0x70, 0x72, 0x61, 0x79, 0x00}},
          {{{0xc4b20385, 0x452109c3, 0x425b432d, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x42737fb1, 0x40e85200, 0x4163ae00, 0xc4b20385, 0xc4b11b33,
             0x452109c3, 0x4521ed71}}}},
         {41,
          {{0x43, 0x4e, 0x32, 0x73, 0x70, 0x72, 0x79, 0x00}},
          {{{0xc2cf45a2, 0x448b0d71, 0x41959d15, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x41c46426, 0x40e8ca60, 0x4155eb80, 0xc2cf45a2, 0xc2c0b8fc,
             0x448b0d71, 0x448cb948}}}},
         {47,
          {{0x43, 0x45, 0x73, 0x70, 0x72, 0x61, 0x79, 0x00}},
          {{{0x4432f396, 0xc3e73396, 0x416f6a7f, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000, 0x41ab84b6, 0x4107cac0, 0x4171cee0, 0x4432f396, 0x443512c2,
             0xc3e73396, 0xc3dfa51e}}}}}};
    constexpr bool Valid() noexcept
    {
        if (Descriptors.size() != MtaGame::CGarageInitializationPolicy::Indices.size())
            return false;
        for (std::size_t i = 0; i != Descriptors.size(); ++i)
        {
            const auto& value = Descriptors[i];
            if (value.sdkId != MtaGame::CGarageInitializationPolicy::Indices[i] || !value.name[0])
                return false;
            bool terminated = false;
            for (const auto byte : value.name)
                terminated = terminated || !byte;
            if (!terminated)
                return false;
            for (std::size_t j = 0; j != i; ++j)
            {
                if (value.sdkId == Descriptors[j].sdkId)
                    return false;
                bool same = true;
                for (std::size_t n = 0; n != value.name.size(); ++n)
                    same = same && value.name[n] == Descriptors[j].name[n];
                for (std::size_t n = 0; n != value.geometry.words.size(); ++n)
                    same = same && value.geometry.words[n] == Descriptors[j].geometry.words[n];
                if (same)
                    return false;
            }
        }
        return true;
    }
    static_assert(Valid(), "Each startup ID has one terminated and distinct qualified descriptor");
    constexpr const NativeNumericGarageDescriptor* Find(std::uint32_t id) noexcept
    {
        for (const auto& value : Descriptors)
            if (value.sdkId == id)
                return &value;
        return nullptr;
    }
}
