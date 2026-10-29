/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact original group-copy and warm hierarchy entry bodies
 ****************************************************************************/
#pragma once
#include "NativeAnimationStaticAssociationProfile.h"
#include "NativeAnimationAssociationProgressProfile.h"
namespace mta::game_de::NativeAnimationAssociationGroupCopyProfile
{
    using Pin = NativeCustomAnimSequenceStorageProfile::Pin;
    inline constexpr std::array<Pin, 2> Code{
        {{0xff1120, NativeAnimationStaticAssociationProfile::Select.data(), NativeAnimationStaticAssociationProfile::Select.size()},
         {NativeAnimationAssociationProgressProfile::HierarchyTimeRva, NativeAnimationAssociationProgressProfile::HierarchyTimeBytes.data(),
          NativeAnimationAssociationProgressProfile::HierarchyTimeBytes.size()}}};
    inline constexpr std::size_t MaximumPinBytes = 647;
}
