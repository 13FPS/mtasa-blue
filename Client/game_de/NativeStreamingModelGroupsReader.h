/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Value-only native loaded group observations for MTA streaming getters
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include "NativeStreamingModelGroupsProfile.h"
#include <array>
namespace mta::game_de
{
    class NativeStreamingModelGroupsReader final
    {
    public:
        using Binding = NativeModelBankReader::Binding;
        using CheckOwner = NativeModelBankReader::CheckOwner;
        struct Value
        {
            std::uint32_t                                                             pedCount = 0;
            std::array<std::int32_t, NativeStreamingModelGroupsProfile::PedSlots>     peds{};
            std::array<std::int16_t, NativeStreamingModelGroupsProfile::VehicleSlots> vehicles{};
        };
        NativeStreamingModelGroupsReader(Binding binding, CheckOwner owner) noexcept : binding(binding), owner(owner) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult Read(Value&) const noexcept;
        ClockResult Recheck(const Value&) const noexcept;
        // These scalar selection rules preserve the actual MTA CStreamingSA
        // getters while using the independently proved DE array capacities.
        // Inline pure rules keep actual SDK consumers independent of native
        // image-reader linkage while sharing the exact tested selection logic.
        static bool IsPedTracked(const Value& value, std::uint16_t model) noexcept
        {
            for (const auto slot : value.peds)
                if (slot >= 0 && slot <= 0xffff && static_cast<std::uint16_t>(slot) == model)
                    return true;
            return false;
        }
        static std::uint32_t VehicleCount(const Value& value) noexcept
        {
            std::uint32_t count = 0;
            for (const auto slot : value.vehicles)
            {
                if (slot == -1)
                    break;
                ++count;
            }
            return count;
        }
        static bool IsVehicleTracked(const Value& value, std::uint16_t model) noexcept
        {
            for (const auto slot : value.vehicles)
            {
                if (slot == -1)
                    break;
                if (slot >= 0 && static_cast<std::uint16_t>(slot) == model)
                    return true;
            }
            return false;
        }
        // A tracked model is not thereby streamed/instantiated. This value
        // carries no model object, native row pointer or retained lifetime lease.
    private:
        Binding    binding;
        CheckOwner owner = nullptr;
    };
}
