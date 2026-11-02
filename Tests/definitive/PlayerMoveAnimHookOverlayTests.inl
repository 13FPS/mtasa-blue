/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact overlay checks using actual controller authority and owned bytes
 ****************************************************************************/
namespace
{
    namespace MoveAnimOverlayFixture = mta::game_de::NativePlayerMoveAnimHookOverlay;
    namespace MoveAnimProfileFixture = mta::game_de::NativePlayerMoveAnimHookProfile;

    static_assert(!std::is_copy_constructible<MoveAnimOverlayFixture::Patch>::value && !std::is_copy_assignable<MoveAnimOverlayFixture::Patch>::value &&
                      !std::is_move_constructible<MoveAnimOverlayFixture::Patch>::value && !std::is_move_assignable<MoveAnimOverlayFixture::Patch>::value,
                  "Acknowledged ownership must remain in the original controller proof");

    // Matchers close over the real controller's acknowledged proof and current
    // admission bookends. Tests need no new raw-proof accessor or fake friend.
    template <class ProcessMatches, class RebuildMatches>
    void CheckMoveAnimHookOverlayBuffers(std::uintptr_t image, const unsigned char* installedProcess, const unsigned char* installedRebuild,
                                         ProcessMatches processMatches, RebuildMatches rebuildMatches)
    {
        Check(image && installedProcess && installedRebuild, "overlay fixture receives actual owned installed spans");
        if (!image || !installedProcess || !installedRebuild)
            return;
        std::array<unsigned char, MoveAnimProfileFixture::ProcessContextBytes>         process{};
        std::array<unsigned char, mta::game_de::NativePedRebuildProfile::Entry.size()> rebuild{};
        std::memcpy(process.data(), installedProcess, process.size());
        std::memcpy(rebuild.data(), installedRebuild, rebuild.size());
        Check(processMatches(image, process.data(), process.size()), "current acknowledged process overlay matches");
        Check(rebuildMatches(image, rebuild.data(), rebuild.size()), "current acknowledged clothes overlay matches");

        MoveAnimOverlayFixture::Patch absent;
        Check(!MoveAnimOverlayFixture::MatchesProcess(absent, image, process.data(), process.size()), "default process proof refuses patched bytes");
        Check(!MoveAnimOverlayFixture::MatchesRebuild(absent, image, rebuild.data(), rebuild.size()), "default clothes proof refuses patched bytes");
        Check(!MoveAnimOverlayFixture::MatchesProcess(absent, image, MoveAnimProfileFixture::ProcessContext.data(), process.size()),
              "stock process bytes do not manufacture acknowledgement");
        Check(!MoveAnimOverlayFixture::MatchesRebuild(absent, image, mta::game_de::NativePedRebuildProfile::Entry.data(), rebuild.size()),
              "stock rebuild bytes do not manufacture acknowledgement");
        Check(!processMatches(image, MoveAnimProfileFixture::ProcessContext.data(), process.size()), "live process proof refuses restored stock span");
        Check(!rebuildMatches(image, mta::game_de::NativePedRebuildProfile::Entry.data(), rebuild.size()), "live clothes proof refuses restored stock span");

        Check(!processMatches(0, process.data(), process.size()) && !rebuildMatches(0, rebuild.data(), rebuild.size()), "zero image refuses both overlays");
        Check(!processMatches(image + 1, process.data(), process.size()) && !rebuildMatches(image + 1, rebuild.data(), rebuild.size()),
              "foreign image refuses both overlays");
        Check(!processMatches(image, nullptr, process.size()) && !rebuildMatches(image, nullptr, rebuild.size()), "null spans refuse before reading");
        Check(!processMatches(image, process.data(), process.size() - 1) && !processMatches(image, process.data(), process.size() + 1),
              "process requires the exact 611-byte span");
        Check(!rebuildMatches(image, rebuild.data(), rebuild.size() - 1) && !rebuildMatches(image, rebuild.data(), rebuild.size() + 1),
              "rebuild requires the exact 2188-byte span");
        Check(!processMatches(image, rebuild.data(), rebuild.size()) && !rebuildMatches(image, process.data(), process.size()),
              "process and rebuild domains cannot substitute for one another");

        // Flip every individual byte, including every displacement/NOP byte and
        // every stock byte around the overlay. An ignored span or broad mask
        // cannot pass this bounded test, even when the opcode still looks valid.
        for (std::size_t index = 0; index < process.size(); ++index)
        {
            process[index] ^= 1;
            Check(!processMatches(image, process.data(), process.size()), "each process patch or stock byte is exact");
            process[index] ^= 1;
        }
        for (std::size_t index = 0; index < rebuild.size(); ++index)
        {
            rebuild[index] ^= 1;
            Check(!rebuildMatches(image, rebuild.data(), rebuild.size()), "each clothes patch or remaining stock byte is exact");
            rebuild[index] ^= 1;
        }
        Check(processMatches(image, process.data(), process.size()) && rebuildMatches(image, rebuild.data(), rebuild.size()),
              "byte checks leave the original acknowledgement and buffers intact");
    }

    // The host calls this after the real controller withdraws/invalidates its
    // original proof, while retaining only old byte copies in owned buffers.
    template <class ProcessMatches, class RebuildMatches>
    void CheckMoveAnimHookOverlayWithdrawn(std::uintptr_t image, const unsigned char* oldProcess, const unsigned char* oldRebuild,
                                           ProcessMatches processMatches, RebuildMatches rebuildMatches)
    {
        Check(oldProcess && oldRebuild, "withdrawal check retains owned old byte spans");
        if (!oldProcess || !oldRebuild)
            return;
        Check(!processMatches(image, oldProcess, MoveAnimProfileFixture::ProcessContextBytes), "withdrawn process proof cannot admit old patched bytes");
        Check(!rebuildMatches(image, oldRebuild, mta::game_de::NativePedRebuildProfile::Entry.size()),
              "withdrawn clothes proof cannot admit old patched bytes");
    }
}
