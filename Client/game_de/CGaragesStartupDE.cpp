/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the garage SDK while numeric identity stays with its owner
 ****************************************************************************/
#include "CGaragesStartupDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    namespace
    {
        constexpr bool ValidStartupDescriptors() noexcept
        {
            if (NativeNumericGarageProfile::Descriptors.size() != MAX_GARAGES || !NativeNumericGarageProfile::Valid())
                return false;
            for (const auto sdkId : MtaGame::CGarageInitializationPolicy::Indices)
                if (!NativeNumericGarageProfile::Find(sdkId))
                    return false;
            return true;
        }
        // The fixed-storage constructor performs no native call or allocation.
        // Its existing named property base can never reach the invalid-name
        // throw path for these independently qualified compile-time inputs.
        static_assert(ValidStartupDescriptors());
    }

    namespace NumericGarageSdkDetail
    {
        BoundAccess::BoundAccess(NativeNumericGarageAccess& access, const NativeNumericGarageDescriptor& descriptor) noexcept
            : native(access), sdkId(descriptor.sdkId), name(descriptor.name)
        {
        }
        bool BoundAccess::Accepts(const NativeGarageName& requested) const noexcept
        {
            return requested == name;
        }
        ClockResult BoundAccess::ReadNamedGarageGeometry(const NativeGarageName& requested, NativeGarageGeometry& output) noexcept
        {
            return Accepts(requested) ? native.ReadNumericGarageGeometry(sdkId, output) : ClockResult::InvalidArgument;
        }
        ClockResult BoundAccess::ReadNamedGarageType(const NativeGarageName& requested, std::uint8_t& output) noexcept
        {
            return Accepts(requested) ? native.ReadNumericGarageType(sdkId, output) : ClockResult::InvalidArgument;
        }
        ClockResult BoundAccess::SetNamedGarageType(const NativeGarageName& requested, std::uint8_t value) noexcept
        {
            return Accepts(requested) ? native.SetNumericGarageType(sdkId, value) : ClockResult::InvalidArgument;
        }
        ClockResult BoundAccess::ReadNamedGarageOpen(const NativeGarageName& requested, bool& output) noexcept
        {
            return Accepts(requested) ? native.ReadNumericGarageOpen(sdkId, output) : ClockResult::InvalidArgument;
        }
        ClockResult BoundAccess::SetNamedGarageOpen(const NativeGarageName& requested, bool value) noexcept
        {
            return Accepts(requested) ? native.SetNumericGarageOpen(sdkId, value) : ClockResult::InvalidArgument;
        }
        AdapterStorage::AdapterStorage(NativeNumericGarageAccess& access, const NativeNumericGarageDescriptor& descriptor) noexcept
            : adapter(access, descriptor)
        {
        }
        Garage::Garage(NativeNumericGarageAccess& access, const NativeNumericGarageDescriptor& descriptor) noexcept
            : AdapterStorage(access, descriptor), CGarageNamedDE(adapter, descriptor.name)
        {
        }
    }

    CGaragesStartupDE::CGaragesStartupDE(NativeNumericGarageAccess& access) noexcept
        : native(access), garages{{{access, NativeNumericGarageProfile::Descriptors[0]},  {access, NativeNumericGarageProfile::Descriptors[1]},
                                   {access, NativeNumericGarageProfile::Descriptors[2]},  {access, NativeNumericGarageProfile::Descriptors[3]},
                                   {access, NativeNumericGarageProfile::Descriptors[4]},  {access, NativeNumericGarageProfile::Descriptors[5]},
                                   {access, NativeNumericGarageProfile::Descriptors[6]},  {access, NativeNumericGarageProfile::Descriptors[7]},
                                   {access, NativeNumericGarageProfile::Descriptors[8]},  {access, NativeNumericGarageProfile::Descriptors[9]},
                                   {access, NativeNumericGarageProfile::Descriptors[10]}, {access, NativeNumericGarageProfile::Descriptors[11]},
                                   {access, NativeNumericGarageProfile::Descriptors[12]}, {access, NativeNumericGarageProfile::Descriptors[13]},
                                   {access, NativeNumericGarageProfile::Descriptors[14]}, {access, NativeNumericGarageProfile::Descriptors[15]},
                                   {access, NativeNumericGarageProfile::Descriptors[16]}, {access, NativeNumericGarageProfile::Descriptors[17]},
                                   {access, NativeNumericGarageProfile::Descriptors[18]}, {access, NativeNumericGarageProfile::Descriptors[19]},
                                   {access, NativeNumericGarageProfile::Descriptors[20]}, {access, NativeNumericGarageProfile::Descriptors[21]},
                                   {access, NativeNumericGarageProfile::Descriptors[22]}, {access, NativeNumericGarageProfile::Descriptors[23]},
                                   {access, NativeNumericGarageProfile::Descriptors[24]}, {access, NativeNumericGarageProfile::Descriptors[25]},
                                   {access, NativeNumericGarageProfile::Descriptors[26]}, {access, NativeNumericGarageProfile::Descriptors[27]},
                                   {access, NativeNumericGarageProfile::Descriptors[28]}, {access, NativeNumericGarageProfile::Descriptors[29]},
                                   {access, NativeNumericGarageProfile::Descriptors[30]}, {access, NativeNumericGarageProfile::Descriptors[31]},
                                   {access, NativeNumericGarageProfile::Descriptors[32]}, {access, NativeNumericGarageProfile::Descriptors[33]},
                                   {access, NativeNumericGarageProfile::Descriptors[34]}, {access, NativeNumericGarageProfile::Descriptors[35]},
                                   {access, NativeNumericGarageProfile::Descriptors[36]}, {access, NativeNumericGarageProfile::Descriptors[37]},
                                   {access, NativeNumericGarageProfile::Descriptors[38]}, {access, NativeNumericGarageProfile::Descriptors[39]},
                                   {access, NativeNumericGarageProfile::Descriptors[40]}, {access, NativeNumericGarageProfile::Descriptors[41]},
                                   {access, NativeNumericGarageProfile::Descriptors[42]}, {access, NativeNumericGarageProfile::Descriptors[43]},
                                   {access, NativeNumericGarageProfile::Descriptors[44]}, {access, NativeNumericGarageProfile::Descriptors[45]},
                                   {access, NativeNumericGarageProfile::Descriptors[46]}, {access, NativeNumericGarageProfile::Descriptors[47]},
                                   {access, NativeNumericGarageProfile::Descriptors[48]}, {access, NativeNumericGarageProfile::Descriptors[49]}}}
    {
    }
    CGarage* CGaragesStartupDE::GetGarage(DWORD sdkId)
    {
        if (sdkId >= MAX_GARAGES)
            return nullptr;
        for (std::size_t index = 0; index != NativeNumericGarageProfile::Descriptors.size(); ++index)
            if (NativeNumericGarageProfile::Descriptors[index].sdkId == sdkId)
                return &garages[index];
        // A future profile regression must never alias a native ordinal,
        // a same-name row or an unrelated zero-filled CPU garage.
        throw std::runtime_error("DE startup garage ID is not qualified: " + std::to_string(sdkId));
    }
    ClockResult CGaragesStartupDE::RequireReady() noexcept
    {
        return native.RequireStartupNumericGaragesReady();
    }
}
