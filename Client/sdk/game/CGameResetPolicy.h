/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the actual per-call game reset sequence across backends
 *****************************************************************************/
#pragma once
namespace MtaGame
{
    class CGameResetPolicy final
    {
    public:
        template <class Extinguish, class CameraRestore, class CameraColor, class CameraFade, class Unpause, class HudDisable, class HudShow,
                  class ModelDummies, class ModelGroups, class ObjectProperties, class ModelWheels, class ModelTextures, class WorldRestore, class BuildingSize,
                  class DefaultBuildingSize, class ResizeBuildings>
        static void Reset(bool playing, Extinguish&& extinguish, CameraRestore&& cameraRestore, CameraColor&& cameraColor, CameraFade&& cameraFade,
                          Unpause&& unpause, HudDisable&& hudDisable, HudShow&& hudShow, ModelDummies&& modelDummies, ModelGroups&& modelGroups,
                          ObjectProperties&& objectProperties, ModelWheels&& modelWheels, ModelTextures&& modelTextures, WorldRestore&& worldRestore,
                          BuildingSize&& buildingSize, DefaultBuildingSize&& defaultBuildingSize, ResizeBuildings&& resizeBuildings)
        {
            if (!playing)
                return;
            extinguish();
            cameraRestore();
            cameraColor();
            cameraFade();
            unpause();
            // Both original calls are observable SDK effects. Do not fold the
            // repeated HUD restore or add a global reset-once/reconnect guard.
            hudDisable();
            hudDisable();
            hudShow();
            modelDummies();
            modelGroups();
            objectProperties();
            modelWheels();
            modelTextures();
            worldRestore();
            const auto currentCapacity = buildingSize();
            const auto defaultCapacity = defaultBuildingSize();
            if (currentCapacity != defaultCapacity)
                static_cast<void>(resizeBuildings(defaultCapacity));
            // Preserve the ignored classic resize result. Exceptions retain
            // the accepted prefix with the original backend, without replay,
            // rollback or synthesized success for any missing native service.
        }
    };
}
