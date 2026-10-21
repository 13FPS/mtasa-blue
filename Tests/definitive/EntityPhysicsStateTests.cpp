/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual shared MTA physics policy, ordering and CPU state ownership
 ****************************************************************************/
#include <game/CEntityPhysicsState.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
    using State = MtaGame::CEntityPhysicsState;
    unsigned checks = 0;
    void     Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, message);
            std::exit(EXIT_FAILURE);
        }
    }
    bool Same(const CVector& a, const CVector& b)
    {
        return std::memcmp(&a, &b, sizeof(a)) == 0;
    }
    float Float(std::uint32_t bits)
    {
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    void Identity(const CMatrix& matrix, const CVector& position)
    {
        Check(Same(matrix.vRight, CVector(1, 0, 0)), "repair uses actual MTA identity right vector");
        Check(Same(matrix.vFront, CVector(0, 1, 0)), "repair uses actual MTA identity front vector");
        Check(Same(matrix.vUp, CVector(0, 0, 1)), "repair uses actual MTA identity up vector");
        Check(Same(matrix.vPos, position), "repair matrix remembers the exact selected position");
    }
    struct RepairFailure
    {
    };
}

int main()
{
    static_assert(sizeof(State) == sizeof(CVector) && alignof(State) == alignof(CVector));
    static_assert(std::is_same_v<decltype(std::declval<State&>().LastGoodPosition()), CVector&>);
    static_assert(std::is_same_v<decltype(std::declval<const State&>().LastGoodPosition()), const CVector&>);
    static_assert(noexcept(State::IsValidPosition(CVector())));
    // CMatrix's existing constructor is not noexcept; use an already-built
    // reference to verify the actual policy's signature independently.
    static_assert(noexcept(State::IsValidMatrix(std::declval<const CMatrix&>())));

    const float infinity = std::numeric_limits<float>::infinity();
    State       state, other;
    Check(Same(state.LastGoodPosition(), CVector()), "classic default last-good CVector remains zero initialized");
    Check(Same(other.LastGoodPosition(), CVector()), "independent wrappers start with independent zero state");

    for (std::size_t axis = 0; axis != 3; ++axis)
    {
        const float                low = axis == 2 ? -5000.0f : -16000.0f;
        const float                high = axis == 2 ? 100000.0f : 16000.0f;
        const std::array<float, 8> values = {
            low,       high,    std::nextafter(low, infinity), std::nextafter(high, -infinity), std::nextafter(low, -infinity), std::nextafter(high, infinity),
            -infinity, infinity};
        for (std::size_t index = 0; index != values.size(); ++index)
        {
            CVector candidate;
            if (axis == 0)
                candidate.fX = values[index];
            else if (axis == 1)
                candidate.fY = values[index];
            else
                candidate.fZ = values[index];
            Check(State::IsValidPosition(candidate) == (index < 4), "exact inclusive axis-specific MTA repair boundary");
        }
        for (std::uint32_t bits : {0x7fc00001u, 0x7fffffffu, 0xffc12345u, 0x7f800001u})
        {
            CVector candidate;
            if (axis == 0)
                candidate.fX = Float(bits);
            else if (axis == 1)
                candidate.fY = Float(bits);
            else
                candidate.fZ = Float(bits);
            Check(!State::IsValidPosition(candidate), "NaN at each vector axis is rejected independently");
            state.RememberPosition(candidate);
            Check(Same(state.LastGoodPosition(), candidate), "remembering copies raw invalid bits without clamping or repair");
        }
    }
    const CVector signedZeros(Float(0x80000000u), Float(0x00000000u), Float(0x80000000u));
    Check(State::IsValidPosition(signedZeros), "signed zeros remain ordinary valid coordinates");
    state.RememberPosition(signedZeros);
    Check(Same(state.LastGoodPosition(), signedZeros), "remembered signed zeros are not normalized");
    Check(Same(other.LastGoodPosition(), CVector()), "raw remembering does not alter another wrapper's state");

    CMatrix matrix;
    matrix.vRight = CVector(infinity, Float(0x7fc12345u), -infinity);
    matrix.vUp = matrix.vRight;
    Check(State::IsValidMatrix(matrix), "matrix policy intentionally ignores invalid right/up vectors");
    matrix.vFront.fZ = 100001;
    Check(!State::IsValidMatrix(matrix), "front vector uses the actual coordinate bounds, not a unit-length rule");
    matrix.vFront = CVector(16000, -16000, 100000);
    Check(State::IsValidMatrix(matrix), "large valid front vector is not newly normalized or rejected");
    matrix.vPos.fX = 16001;
    Check(!State::IsValidMatrix(matrix), "matrix position is still checked when its front is valid");

    unsigned randomCalls = 0;
    state.RememberPosition(signedZeros);
    auto repaired = state.PrepareRepair(
        [&]
        {
            ++randomCalls;
            return 999;
        });
    Check(randomCalls == 0, "valid last-good state consumes no random number");
    Identity(repaired, signedZeros);
    for (int draw : {0, 1999, 2000, RAND_MAX})
    {
        const CVector invalid(infinity, 5, 6);
        state.RememberPosition(invalid);
        const auto before = randomCalls;
        repaired = state.PrepareRepair(
            [&]
            {
                ++randomCalls;
                return draw;
            });
        const CVector expected(6000, static_cast<float>(draw % 2000), 1000);
        Check(randomCalls == before + 1, "invalid remembered position draws exactly once");
        Check(Same(state.LastGoodPosition(), expected), "fallback stores the actual modulo-selected CPU position");
        Identity(repaired, expected);
        repaired = state.PrepareRepair(
            [&]
            {
                ++randomCalls;
                return 17;
            });
        Check(randomCalls == before + 1, "selected fallback becomes valid without repeated RNG consumption");
        Identity(repaired, expected);
    }
    state.LastGoodPosition() = CVector(7, 8, 9);
    Check(Same(state.LastGoodPosition(), CVector(7, 8, 9)), "mutable CPU alias preserves the classic SetPosition input alias");

    // Exact getter policy: the callback names stand for real native work in
    // classic callers. This host fixture tests the shared ordering without
    // treating a CPU callback as proof that DE native repair is available.
    std::string   events;
    const CVector prior(1, 2, 3);
    CVector       position(4, 5, 6);
    state.RememberPosition(prior);
    auto readPosition = [&]() -> CVector*
    {
        events += 'R';
        return &position;
    };
    auto repairPosition = [&]
    {
        events += 'P';
        Check(Same(state.LastGoodPosition(), prior), "repair sees old last-good state before the final observation");
        position = CVector(20, 30, 40);
    };
    Check(state.GetPosition(readPosition, repairPosition) == &position, "valid position getter returns the real supplied alias");
    Check(events == "R" && Same(state.LastGoodPosition(), position), "valid position is remembered after one read without repair");
    state.RememberPosition(prior);
    position.fX = infinity;
    events.clear();
    Check(state.GetPosition(readPosition, repairPosition) == &position, "position getter preserves final native alias after repair");
    Check(events == "RPR" && Same(state.LastGoodPosition(), CVector(20, 30, 40)), "invalid position repairs once, rereads, then remembers");
    events.clear();
    position = CVector(infinity, 0, 0);
    Check(state.GetPosition(readPosition, [&] { events += 'P'; }) == &position, "second invalid position is still the original API's returned alias");
    Check(events == "RPR" && Same(state.LastGoodPosition(), position), "second invalid read is remembered without an invented retry loop");

    CMatrix output;
    CMatrix source(CVector(10, 11, 12));
    state.RememberPosition(prior);
    events.clear();
    auto readMatrix = [&](CMatrix* into) -> CMatrix*
    {
        events += 'R';
        if (!into)
            return nullptr;
        *into = source;
        return into;
    };
    auto repairMatrix = [&]
    {
        events += 'P';
        source = CMatrix(CVector(30, 31, 32));
    };
    Check(state.GetMatrix(nullptr, readMatrix, repairMatrix) == nullptr, "null matrix output preserves the classic null result");
    Check(events == "R" && Same(state.LastGoodPosition(), prior), "null matrix read does not repair or update last-good state");
    events.clear();
    Check(state.GetMatrix(&output, readMatrix, repairMatrix) == &output, "matrix getter returns the exact caller output address");
    Check(events == "R" && Same(state.LastGoodPosition(), source.vPos), "valid copied matrix remembers only its position after one read");
    source.vFront.fY = infinity;
    state.RememberPosition(prior);
    events.clear();
    Check(state.GetMatrix(&output, readMatrix, repairMatrix) == &output, "repaired matrix still uses caller storage");
    Check(events == "RPR" && Same(state.LastGoodPosition(), CVector(30, 31, 32)), "invalid matrix repairs once before reread and cache publication");
    state.RememberPosition(prior);
    events.clear();
    Check(state.GetMatrix(
              &output,
              [&](CMatrix*) -> CMatrix*
              {
                  events += 'R';
                  return nullptr;
              },
              repairMatrix) == nullptr,
          "absent native matrix is not fabricated as identity");
    Check(events == "R" && Same(state.LastGoodPosition(), prior), "absent matrix leaves remembered state untouched");

    for (bool angular : {false, true})
    {
        CVector velocity(8, 9, 10), into;
        state.RememberPosition(prior);
        events.clear();
        auto readVelocity = [&](CVector* target)
        {
            events += angular ? 'A' : 'L';
            *target = velocity;
        };
        auto repairVelocity = [&]
        {
            events += 'P';
            velocity = CVector();
        };
        Check(State::GetVelocity(&into, readVelocity, repairVelocity) == &into, "both velocity getters return caller storage");
        Check(events == (angular ? "A" : "L") && Same(into, velocity), "valid raw velocity is copied once without normalization");
        Check(Same(state.LastGoodPosition(), prior), "valid velocity never replaces remembered entity position");
        velocity = CVector(16001, 0, 0);
        events.clear();
        State::GetVelocity(&into, readVelocity, repairVelocity);
        Check(events == (angular ? "APA" : "LPL") && Same(into, CVector()), "invalid velocity invokes full repair once then re-reads");
        Check(Same(state.LastGoodPosition(), prior), "velocity read path itself still does not update last-good position");
    }

    state.RememberPosition(prior);
    position = CVector(infinity, 0, 0);
    events.clear();
    bool threw = false;
    try
    {
        state.GetPosition(readPosition,
                          [&]
                          {
                              events += 'P';
                              // A native repair callback can return or fail;
                              // model that runtime choice under /W4 /WX too.
                              volatile bool refuse = true;
                              if (refuse)
                                  throw RepairFailure{};
                          });
    }
    catch (const RepairFailure&)
    {
        threw = true;
    }
    Check(threw && events == "RP", "repair failure propagates without a second native read or hidden retry");
    Check(Same(state.LastGoodPosition(), prior), "failed repair callback does not manufacture a final successful observation");
    state.RememberPosition(position);
    threw = false;
    try
    {
        state.PrepareRepair(
            []() -> int
            {
                // Keep a possible return for the real template instantiation:
                // /WX must not turn this exception-path test into C4702 merely
                // because an always-throwing fixture makes its caller unreachable.
                volatile bool refuse = true;
                if (refuse)
                    throw RepairFailure{};
                return 0;
            });
    }
    catch (const RepairFailure&)
    {
        threw = true;
    }
    Check(threw && Same(state.LastGoodPosition(), position), "failed random source does not replace stored CPU position");
    Check(Same(other.LastGoodPosition(), CVector()), "all repair/getter activity remains instance local");

    std::printf("Passed %u shared MTA entity physics state checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return 0;
}
