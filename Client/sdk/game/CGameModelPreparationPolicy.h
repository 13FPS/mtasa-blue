/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's actual special-model and broken-collision startup order
 *****************************************************************************/
#pragma once

namespace MtaGame
{
    class CGameModelPreparationPolicy final
    {
    public:
        template <class MakePedModel>
        static void SetupSpecialCharacters(MakePedModel&& makePedModel)
        {
            // These are MTA's active replacements, including its existing
            // numeric IDs. Repeat the schedule on each explicit later
            // Initialize call; this policy does not allocate native metadata,
            // cache a facade, request-only, or invent a new once guard.
            makePedModel(1, "TRUTH");
            makePedModel(2, "MACCER");
            makePedModel(3, "CDEPUT");
            makePedModel(4, "SFPDM1");
            makePedModel(5, "BB");
            makePedModel(6, "WFYCRP");
            makePedModel(8, "WMYCD2");
            makePedModel(42, "SUZIE");
            makePedModel(65, "VWMYAP");
            makePedModel(86, "VHFYST");
            makePedModel(119, "LVPDM1");
            makePedModel(265, "TENPEN");
            makePedModel(266, "PULASKI");
            makePedModel(267, "HERN");
            makePedModel(268, "DWAYNE");
            makePedModel(269, "SMOKE");
            makePedModel(270, "SWEET");
            makePedModel(271, "RYDER");
            makePedModel(272, "FORELLI");
            makePedModel(273, "MEDIATR");
            makePedModel(289, "SOMYAP");
            makePedModel(290, "ROSE");
            makePedModel(291, "PAUL");
            makePedModel(292, "CESAR");
            makePedModel(293, "OGLOC");
            makePedModel(294, "WUZIMU");
            makePedModel(295, "TORINO");
            makePedModel(296, "JIZZY");
            makePedModel(297, "MADDOGG");
            makePedModel(298, "CAT");
            makePedModel(299, "CLAUDE");
            makePedModel(300, "RYDER2");
            makePedModel(301, "RYDER3");
            makePedModel(302, "EMMET");
            makePedModel(303, "ANDRE");
            makePedModel(304, "KENDL");
            makePedModel(305, "JETHRO");
            makePedModel(306, "ZERO");
            makePedModel(307, "TBONE");
            makePedModel(308, "SINDACO");
            makePedModel(309, "JANITOR");
            makePedModel(310, "BBTHIN");
            makePedModel(311, "SMOKEV");
            makePedModel(312, "PSYCHO");
        }

        template <class FixModelCollision>
        static void SetupBrokenModels(FixModelCollision&& fixModelCollision)
        {
            // The receiver retains the classic conditional raw-pointer copy:
            // target exists without collision, source exists, then borrow its
            // collision pointer. SetColModel's custom-model policy differs.
            fixModelCollision(3118, 3059);
            fixModelCollision(3553, 3554);
            // An exception retains the accepted prefix. There is no rollback,
            // automatic retry or successful stand-in for a missing backend.
        }
    };
}
