/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Refuse real native updater qualification in an ordinary executable
 *****************************************************************************/
#include "../../Client/game_de/NativePadUpdateControl.h"
#include "../../Client/game_de/NativePadUpdateProfile.h"
#include "../../Client/game_de/NativeServiceAdmission.h"
#include <cstdio>
#include <cstdlib>

namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* description)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, description);
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    namespace Admission = mta::game_de::NativeServiceAdmission;
    namespace Profile = mta::game_de::NativePadUpdateProfile;
    Profile::Binding binding{11, 22, 33};
    Check(!Profile::Qualify(binding) && binding.image == 11 && binding.updater == 22 && binding.pads == 33,
          "real exact-executable qualification refuses this ordinary host without publishing a partial binding");
    Check(!Profile::Revalidate(binding), "real native profile never treats a synthetic binding as a game");
    mta::game_de::NativePadUpdateControl owner;
    bool                                 output = true;
    Check(!owner.Begin() && !owner.IsEnabled(output) && output && !owner.SetDisabled(true),
          "unmodified production owner refuses wrong executable without admitting a hook or fabricating output");
    mta::game_de::NativePadUpdateState state;
    owner.Read(state);
    Check(!state.generation && !state.bound && !state.hooked && !state.calls && !state.callbacks && !state.retainedPeers,
          "wrong-host refusal leaves actual native owner state unadmitted");
    Check(!Admission::IsClaimed(Admission::Category::PadUpdate), "wrong-host Begin never claims pad updater admission");
    Admission::Token clock;
    Check(Admission::TryAcquire(Admission::Category::Clock, clock) && Admission::Release(clock), "failed updater setup leaves clock admission available");
    Check(owner.End() && mta::game_de::NativePadUpdateControl::RecoverAbandoned(), "ordinary-host retirement has no hidden restoration obligation");
    std::printf("Passed %u ordinary-host pad updater refusal checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return 0;
}
