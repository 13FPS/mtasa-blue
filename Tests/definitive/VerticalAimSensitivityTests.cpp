/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compare the controller SDK's slider range to its classic calculations
 *****************************************************************************/
#include <game/CControllerConfigManager.h>
#include <windows.h>
#pragma warning(push, 3)
#include <SharedUtil.Defines.h>
#include <SharedUtil.Misc.h>
#pragma warning(pop)
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace
{
    class Controller final : public CControllerConfigManager
    {
    public:
        float    raw = 0.0f;
        unsigned reads = 0, writes = 0;
        float    GetVerticalAimSensitivityRawValue() override
        {
            ++reads;
            return raw;
        }
        void SetVerticalAimSensitivityRawValue(float value) override
        {
            ++writes;
            raw = value;
        }
        void SetControllerKeyAssociatedWithAction(eControllerAction, int, eControllerType) override { std::abort(); }
        int  GetControllerKeyAssociatedWithAction(eControllerAction, eControllerType) override { std::abort(); }
        void SetClassicControls(bool) override { std::abort(); }
        void SetMouseInverted(bool) override { std::abort(); }
        void SetFlyWithMouse(bool) override { std::abort(); }
        void SetSteerWithMouse(bool) override { std::abort(); }
        void SuspendSteerAndFlyWithMouse(bool) override { std::abort(); }
        void SetVerticalAimSensitivitySameAsHorizontal(bool) override { std::abort(); }
    };

    bool Same(float actual, float expected)
    {
        return (std::isnan(expected) && std::isnan(actual)) || std::memcmp(&actual, &expected, sizeof(float)) == 0;
    }
}

int main()
{
    // The original classic methods use double arithmetic in UnlerpClamped
    // and float arithmetic in Lerp. Compare their actual shared utility code.
    constexpr float             min = 0.000312f, middle = 0.0015f, max = middle * 2 - min;
    const auto                  infinity = std::numeric_limits<float>::infinity();
    const std::array<float, 21> values = {-infinity,
                                          -2.f,
                                          -0.f,
                                          0.f,
                                          std::numeric_limits<float>::denorm_min(),
                                          std::nextafter(min, -infinity),
                                          min,
                                          std::nextafter(min, infinity),
                                          std::nextafter(middle, -infinity),
                                          middle,
                                          std::nextafter(middle, infinity),
                                          std::nextafter(max, -infinity),
                                          max,
                                          std::nextafter(max, infinity),
                                          0.25f,
                                          0.5f,
                                          1.f,
                                          2.f,
                                          std::numeric_limits<float>::max(),
                                          infinity,
                                          std::numeric_limits<float>::quiet_NaN()};
    Controller                  controller;
    CControllerConfigManager&   sdk = controller;
    unsigned                    samples = 0;
    for (float input : values)
    {
        controller.raw = input;
        const float normalized = sdk.GetVerticalAimSensitivity();
        if (!Same(normalized, SharedUtil::UnlerpClamped(min, input, max)) || controller.reads != samples + 1 || controller.writes != samples ||
            !Same(controller.raw, input))
            return EXIT_FAILURE;

        sdk.SetVerticalAimSensitivity(input);
        if (!Same(controller.raw, SharedUtil::Lerp(min, input, max)) || controller.reads != samples + 1 || controller.writes != samples + 1)
            return EXIT_FAILURE;
        ++samples;
    }
    std::printf("Passed %u classic normalized vertical-aim reads and writes (%u-bit)\n", samples, static_cast<unsigned>(sizeof(void*) * 8));
}
