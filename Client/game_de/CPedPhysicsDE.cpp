/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual MTA getter ordering without inventing native repair
 ****************************************************************************/
#include "CPedPhysicsDE.h"
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    void CPedPhysicsDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped physics owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    void CPedPhysicsDE::Repair()
    {
        const CMatrix matrix = physics.PrepareRepair([] { return std::rand(); });
        Require(RepairLastGoodPhysicsState(matrix), "RestoreLastGoodPhysicsState");
    }
    CVector* CPedPhysicsDE::ReadPosition()
    {
        CVector* position = nullptr;
        Require(observation.BorrowReferencedPedPosition(ReferenceToken(), position), "GetPosition");
        if (!position)
            Require(ClockResult::ReadFailure, "GetPosition");
        // This is the actual native address, only within the admitted callback
        // before another native operation can replace/recycle its matrix.
        return position;
    }
    CMatrix* CPedPhysicsDE::ReadMatrix(CMatrix* output, bool& requireMatrix)
    {
        if (!output)
            return nullptr;
        NativePedTransformReader::Value value;
        Require(observation.ReadReferencedPedTransform(ReferenceToken(), value), "GetMatrix");
        if (!value.matrix)
        {
            // An initially absent matrix matches classic's null result. Losing
            // it across a repair invalidates that borrow; do not dereference a
            // null second result or fabricate a copied identity matrix.
            if (requireMatrix)
                Require(ClockResult::BindingChanged, "GetMatrix");
            return nullptr;
        }
        requireMatrix = true;
        CMatrix matrix;
        matrix.vRight = value.matrixRows[0];
        matrix.vFront = value.matrixRows[1];
        matrix.vUp = value.matrixRows[2];
        matrix.vPos = value.matrixRows[3];
        // Native padded rows are copied into the genuine existing SDK type;
        // no native CMatrix layout or deferred pointer is manufactured.
        *output = matrix;
        return output;
    }
    void CPedPhysicsDE::ReadVelocity(NativePedVelocityKind kind, CVector* output)
    {
        if (!output)
            Require(ClockResult::InvalidArgument, "GetVelocity");
        CVector value;
        Require(observation.ReadReferencedPedVelocity(ReferenceToken(), kind, value), "GetVelocity");
        *output = value;
    }
    CVector* CPedPhysicsDE::GetPosition()
    {
        return physics.GetPosition([this] { return ReadPosition(); }, [this] { Repair(); });
    }
    CMatrix* CPedPhysicsDE::GetMatrix(CMatrix* matrix)
    {
        bool requireMatrix = false;
        return physics.GetMatrix(matrix, [this, &requireMatrix](CMatrix* output) { return ReadMatrix(output, requireMatrix); }, [this] { Repair(); });
    }
    CVector* CPedPhysicsDE::GetMoveSpeed(CVector* velocity)
    {
        return MtaGame::CEntityPhysicsState::GetVelocity(
            velocity, [this](CVector* output) { ReadVelocity(NativePedVelocityKind::Linear, output); }, [this] { Repair(); });
    }
    CVector* CPedPhysicsDE::GetTurnSpeed(CVector* velocity)
    {
        return MtaGame::CEntityPhysicsState::GetVelocity(
            velocity, [this](CVector* output) { ReadVelocity(NativePedVelocityKind::Angular, output); }, [this] { Repair(); });
    }
}
