/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared MTA entity validation and last-good-position CPU policy
 ****************************************************************************/
#pragma once
#include <SharedUtil.Defines.h>
#include <CMatrix.h>
#include <cmath>
#include <type_traits>

namespace MtaGame
{
    // This owns only the same CPU position that classic CEntitySA remembers.
    // Selecting a repair matrix never writes an engine or claims that native
    // placement, world/RW refresh, or physical collision repair has happened.
    class CEntityPhysicsState final
    {
    public:
        static bool IsValidPosition(const CVector& value) noexcept
        {
            // These are the existing SDK repair bounds, including their use
            // for velocities/front vectors. A generic finite-vector filter or
            // normal-length test would change established MTA behavior.
            if (value.fX < -16000 || value.fX > 16000 || std::isnan(value.fX) || value.fY < -16000 || value.fY > 16000 || std::isnan(value.fY) ||
                value.fZ < -5000 || value.fZ > 100000 || std::isnan(value.fZ))
                return false;
            return true;
        }

        static bool IsValidMatrix(const CMatrix& value) noexcept
        {
            // Classic deliberately validates position/front only; right/up
            // are not normalized or newly rejected by this extraction.
            return IsValidPosition(value.vPos) && IsValidPosition(value.vFront);
        }

        void           RememberPosition(const CVector& position) noexcept { lastGoodPosition = position; }
        const CVector& LastGoodPosition() const noexcept { return lastGoodPosition; }
        // The mutable CPU alias preserves the classic SetPosition call's
        // original input alias. It is never a borrowed native vector address.
        CVector& LastGoodPosition() noexcept { return lastGoodPosition; }

        template <class Read, class Repair>
        CVector* GetPosition(Read&& read, Repair&& repair)
        {
            CVector* position = read();
            if (!IsValidPosition(*position))
            {
                repair();
                position = read();
            }
            // Keep the classic single repair/re-read and unconditional final
            // observation. It does not poll until an engine becomes valid.
            RememberPosition(*position);
            return position;
        }

        template <class Read, class Repair>
        CMatrix* GetMatrix(CMatrix* output, Read&& read, Repair&& repair)
        {
            CMatrix* matrix = read(output);
            if (matrix)
            {
                if (!IsValidMatrix(*matrix))
                {
                    repair();
                    matrix = read(output);
                }
                RememberPosition(matrix->vPos);
            }
            return matrix;
        }

        template <class Read, class Repair>
        static CVector* GetVelocity(CVector* output, Read&& read, Repair&& repair)
        {
            read(output);
            if (!IsValidPosition(*output))
            {
                repair();
                read(output);
            }
            // Unlike entity position/matrix reads, the physical velocity
            // getter never replaces the remembered last-good position.
            return output;
        }

        template <class NextRandom>
        CMatrix PrepareRepair(NextRandom&& nextRandom)
        {
            // Draw only when the remembered value is invalid. Eagerly drawing
            // would perturb the existing game's random sequence on every
            // otherwise ordinary repair of a bad matrix or velocity.
            if (!IsValidPosition(lastGoodPosition))
                lastGoodPosition = CVector(6000, static_cast<float>(nextRandom() % 2000), 1000);
            CMatrix matrix;
            matrix.vPos = lastGoodPosition;
            return matrix;
        }

    private:
        CVector lastGoodPosition;
    };
    // Replacing the classic private CVector must not change wrapper layout or
    // its constructor's existing zero-vector initialization.
    static_assert(std::is_standard_layout_v<CEntityPhysicsState> && sizeof(CEntityPhysicsState) == sizeof(CVector) &&
                  alignof(CEntityPhysicsState) == alignof(CVector));
}
