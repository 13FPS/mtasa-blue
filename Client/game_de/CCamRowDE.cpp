/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual vector addresses and release only the issued hold
 ****************************************************************************/
#include "CCamRowDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    CCamRowDE::CCamRowDE(std::uint32_t row, NativeCameraSphereVisibleAccess& provider) : CCamFovDE(row, provider), rowOwner(provider)
    {
        const auto result = rowOwner.AcquireCameraRowBorrow(row, token);
        if (result != ClockResult::Ok || !token)
            throw std::runtime_error("DE camera owner refused AcquireCameraRowBorrow: " +
                                     std::to_string(static_cast<unsigned>(result == ClockResult::Ok ? ClockResult::BindingChanged : result)));
        // The real provider's successful ledger commit is its final fallible
        // acquisition step. Nothing after it can fail before this client lives.
    }
    CCamRowDE::~CCamRowDE() noexcept
    {
        // This is a CPU-only producer release, including after Stop. A refused
        // release deliberately leaves the provider's row/module hold retained;
        // the token's own destructor neither retries nor frees native storage.
        if (token)
            rowOwner.ReleaseCameraRowBorrow(token);
    }
    CVector* CCamRowDE::Read(CameraRowVectorKind kind, const char* method) const
    {
        CVector*   pointer = nullptr;
        const auto result = rowOwner.ReadCameraRowVector(token, kind, pointer);
        if (result != ClockResult::Ok || !pointer)
            throw std::runtime_error(std::string("DE camera owner refused ") + method + ": " +
                                     std::to_string(static_cast<unsigned>(result == ClockResult::Ok ? ClockResult::BindingChanged : result)));
        // Preserve identity, writable aliasing and every component bit. The
        // current provider rechecks authority/storage on each pointer borrow;
        // a cached CPU vector or normalized replacement would change MTA's API.
        return pointer;
    }
    CVector* CCamRowDE::GetFront() const
    {
        return Read(CameraRowVectorKind::Front, "GetFront");
    }
    CVector* CCamRowDE::GetUp() const
    {
        return Read(CameraRowVectorKind::Up, "GetUp");
    }
    CVector* CCamRowDE::GetSource() const
    {
        return Read(CameraRowVectorKind::Source, "GetSource");
    }
    CVector* CCamRowDE::GetFixedModeSource() const
    {
        return Read(CameraRowVectorKind::FixedModeSource, "GetFixedModeSource");
    }
    CVector* CCamRowDE::GetFixedModeVector() const
    {
        return Read(CameraRowVectorKind::FixedModeVector, "GetFixedModeVector");
    }
}
