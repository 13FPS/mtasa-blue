/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/game/CHandlingEntry.h
 *  PURPOSE:     Vehicle handling entry interface
 *
 *  Multi Theft Auto is available from http://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <cmath>
#include "enums/VehicleSoundType.h"

class CVehicleAudioSettingsEntry
{
public:
    // Destructor
    virtual ~CVehicleAudioSettingsEntry() {}

    // Get
    virtual VehicleSoundType::Enum GetSoundType() const noexcept = 0;
    virtual short                  GetEngineOnSoundBankID() const noexcept = 0;
    virtual short                  GetEngineOffSoundBankID() const noexcept = 0;
    virtual char                   GetBassSetting() const noexcept = 0;
    virtual float                  GetBassEq() const noexcept = 0;
    virtual float                  GetFieldC() const noexcept = 0;
    virtual char                   GetHornTon() const noexcept = 0;
    virtual float                  GetHornHign() const noexcept = 0;
    virtual char                   GetEngineUpgrade() const noexcept = 0;
    virtual char                   GetDoorSound() const noexcept = 0;
    virtual char                   GetRadioNum() const noexcept = 0;
    virtual char                   GetRadioType() const noexcept = 0;
    virtual char                   GetVehicleTypeForAudio() const noexcept = 0;
    virtual float                  GetHornVolumeDelta() const noexcept = 0;

    // Set
    virtual void SetSoundType(VehicleSoundType::Enum value) noexcept = 0;
    virtual void SetEngineOnSoundBankID(short value) noexcept = 0;
    virtual void SetEngineOffSoundBankID(short value) noexcept = 0;
    virtual void SetBassSetting(char value) noexcept = 0;
    virtual void SetBassEq(float value) noexcept = 0;
    virtual void SetFieldC(float value) noexcept = 0;
    virtual void SetHornTon(char value) noexcept = 0;
    virtual void SetHornHign(float value) noexcept = 0;
    virtual void SetEngineUpgrade(char value) noexcept = 0;
    virtual void SetDoorSound(char value) noexcept = 0;
    virtual void SetRadioNum(char value) noexcept = 0;
    virtual void SetRadioType(char value) noexcept = 0;
    virtual void SetVehicleTypeForAudio(char value) noexcept = 0;
    virtual void SetHornVolumeDelta(float value) noexcept = 0;

    // Entries from different backends may use different storage layouts.
    // Copy the configured radio station even when playback chooses it randomly.
    void CopyFrom(const CVehicleAudioSettingsEntry& source) noexcept
    {
        SetSoundType(source.GetSoundType());
        SetEngineOnSoundBankID(source.GetEngineOnSoundBankID());
        SetEngineOffSoundBankID(source.GetEngineOffSoundBankID());
        SetBassSetting(source.GetBassSetting());
        SetBassEq(source.GetBassEq());
        SetFieldC(source.GetFieldC());
        SetHornTon(source.GetHornTon());
        SetHornHign(source.GetHornHign());
        SetEngineUpgrade(source.GetEngineUpgrade());
        SetDoorSound(source.GetDoorSound());
        SetRadioNum(source.GetRadioNum());
        SetRadioType(source.GetRadioType());
        SetVehicleTypeForAudio(source.GetVehicleTypeForAudio());
        SetHornVolumeDelta(source.GetHornVolumeDelta());
    }

    bool HasSamePlaybackSettings(const CVehicleAudioSettingsEntry& other) const noexcept
    {
        constexpr char randomRadio = 2;
        const char     radio = GetRadioType();
        // A randomized station changes while the configuration stays the same.
        // Reinitializing in that case would interrupt the current vehicle audio.
        return radio == other.GetRadioType() && (radio == randomRadio || GetRadioNum() == other.GetRadioNum()) && GetSoundType() == other.GetSoundType() &&
               GetEngineOnSoundBankID() == other.GetEngineOnSoundBankID() && GetEngineOffSoundBankID() == other.GetEngineOffSoundBankID() &&
               GetBassSetting() == other.GetBassSetting() && EqualFloat(GetBassEq(), other.GetBassEq()) && EqualFloat(GetFieldC(), other.GetFieldC()) &&
               GetHornTon() == other.GetHornTon() && EqualFloat(GetHornHign(), other.GetHornHign()) && GetEngineUpgrade() == other.GetEngineUpgrade() &&
               GetDoorSound() == other.GetDoorSound() && GetVehicleTypeForAudio() == other.GetVehicleTypeForAudio() &&
               EqualFloat(GetHornVolumeDelta(), other.GetHornVolumeDelta());
    }

private:
    static bool EqualFloat(float left, float right) noexcept
    {
        // The SDK accepts arbitrary floats. Reapplying an unchanged NaN must
        // not continually restart the audio, and signed zero has identical gain.
        return left == right || (std::isnan(left) && std::isnan(right));
    }
};
