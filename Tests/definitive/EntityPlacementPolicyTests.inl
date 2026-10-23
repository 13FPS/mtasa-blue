/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual shared finite placement and teleport sequence
 ****************************************************************************/
#include <game/CEntityPlacementPolicy.h>
namespace
{
    void RunEntityPlacementPolicyTests()
    {
        using Policy = MtaGame::CEntityPlacementPolicy;
        for (std::uint32_t model = 0; model != 65536; ++model)
        {
            const bool expected = model == 537 || model == 538 || model == 569 || model == 570 || model == 590 || model == 449;
            Check(Policy::RequiresTrainRailRecalculation(static_cast<std::uint16_t>(model)) == expected,
                  "the actual classic train-effect WORD predicate has exactly six IDs, without an invented model-classification range");
        }
        for (const auto bad : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
            for (unsigned component = 0; component != 3; ++component)
            {
                CVector input(1, 2, 3);
                if (component == 0)
                    input.fX = bad;
                if (component == 1)
                    input.fY = bad;
                if (component == 2)
                    input.fZ = bad;
                unsigned calls = 0;
                Policy::SetPosition(
                    input.fX, input.fY, input.fZ,
                    [&](const CVector&) -> CVector*
                    {
                        ++calls;
                        return nullptr;
                    },
                    [&](const CVector&) { ++calls; }, [&] { ++calls; });
                Policy::Teleport(
                    input.fX, input.fY, input.fZ,
                    [&]
                    {
                        ++calls;
                        return true;
                    },
                    [&](const CVector&) { ++calls; }, [&](const CVector&, bool) { ++calls; });
                Check(!calls, "nonfinite input returns before placement, remember, class effects, matrix lookup or native teleport");
            }
        CVector               committed(-0.0f, (std::numeric_limits<float>::max)(), -3.5f);
        std::vector<unsigned> order;
        Policy::SetPosition(
            committed.fX, committed.fY, committed.fZ,
            [&](const CVector& input) -> CVector*
            {
                Check(std::memcmp(&input, &committed, sizeof(input)) == 0, "finite targets keep raw bits without physics repair bounds");
                order.push_back(1);
                return &committed;
            },
            [&](const CVector& actual)
            {
                Check(&actual == &committed, "remember sees the actual committed vector identity");
                order.push_back(2);
            },
            [&] { order.push_back(3); });
        Check(order == std::vector<unsigned>({1, 2, 3}), "last-good placement commits before native train/object post-effects");
        order.clear();
        Policy::SetPosition(
            1, 2, 3,
            [&](const CVector&) -> CVector*
            {
                order.push_back(1);
                return nullptr;
            },
            [&](const CVector&) { order.push_back(2); }, [&] { order.push_back(3); });
        Check(order == std::vector<unsigned>({1, 3}), "classic null placement skips remember while retaining class post-effect ordering");
        unsigned pointerCalls = 0;
        Policy::SetPosition(static_cast<CVector*>(nullptr), [&](float, float, float) { ++pointerCalls; });
        Policy::SetPosition(&committed,
                            [&](float x, float y, float z)
                            {
                                ++pointerCalls;
                                CVector copied(x, y, z);
                                Check(std::memcmp(&copied, &committed, sizeof(copied)) == 0,
                                      "pointer overload forwards raw components to the actual scalar setter");
                            });
        Check(pointerCalls == 1, "null CVector pointer remains a no-op, never a native owner call");
        for (const bool matrix : {false, true})
        {
            bool currentMatrix = matrix;
            order.clear();
            Policy::Teleport(
                1, 2, 3,
                [&]
                {
                    order.push_back(1);
                    return currentMatrix;
                },
                [&](const CVector&)
                {
                    order.push_back(2);
                    currentMatrix = !currentMatrix;
                },
                [&](const CVector& value, bool reset)
                {
                    Check(value == CVector(1, 2, 3) && reset, "matrix teleport preserves exact target and true reset argument");
                    order.push_back(3);
                });
            Check(order == (matrix ? std::vector<unsigned>({1, 2, 3}) : std::vector<unsigned>({1, 2})),
                  "teleport selects its matrix branch before SetPosition and never upgrades inline placement into native action");
        }
        unsigned after = 0;
        bool     threw = false;
        // Keep the refusal a runtime outcome so the optimized MSVC fixture
        // still compiles the policy's successful continuation under /WX.
        volatile bool refuse = true;
        try
        {
            Policy::SetPosition(
                1, 2, 3,
                [&](const CVector&) -> CVector*
                {
                    if (refuse)
                        throw std::runtime_error("write refusal");
                    return nullptr;
                },
                [&](const CVector&) { ++after; }, [&] { ++after; });
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && !after, "refused or uncertain placement cannot commit a last-good position or enter later native effects");
        std::puts("Production shared entity placement policy fixture completed");
    }
}
