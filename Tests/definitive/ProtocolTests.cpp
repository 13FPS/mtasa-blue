/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto - Definitive Edition experiment
 *  LICENSE:     See LICENSE in the top level directory
 *  PURPOSE:     Exercise the small wire protocol independently of GTA and netc
 *
 *****************************************************************************/

#include "../../Client/definitive/Protocol.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    int checks = 0;

    void Check(bool condition, const std::string& message)
    {
        ++checks;
        // These tests also run in Release: assert would silently remove the
        // malformed-packet checks precisely where published binaries need them.
        if (!condition)
            throw std::runtime_error(message);
    }

    using Bytes = std::vector<std::uint8_t>;

    constexpr std::uint64_t nonce = 0x0807060504030201ull;
    constexpr std::uint32_t recipientId = 0x44332211u;
    constexpr std::uint64_t session = 0x1817161514131211ull;

    const Bytes welcome = {'D', 'E', 'S', '1', 1,    2,    20,   0,    0x11, 0x22, 0x33, 0x44, 1,    2,    3,    4,
                           5,   6,   7,   8,   0x11, 0x22, 0x33, 0x44, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};

    // Wire fixtures are literal bytes, not built with the implementation's
    // encoder, so a shared byte-order or layout mistake cannot pass a roundtrip.
    const Bytes snapshot = {'D',  'E',  'S', '1', 1, 4, 54, 0, 0xfe, 0xff, 0xff, 0xff, 0x11, 0x22, 0x33, 0x44, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                            0x17, 0x18, 2,   0,   1, 0, 0,  0, 0,    0,    0x80, 0x3f, 0,    0,    0,    0xc0, 0,    0,    0x60, 0x40, 0,    0,
                            0x80, 0x3e, 2,   0,   0, 0, 0,  0, 0x80, 0x40, 0,    0,    0xa0, 0x40, 0,    0,    0xc0, 0x40, 0,    0,    0xe0, 0x40};

    void TestEncoders()
    {
        const Bytes expectedJoin = {'D', 'E', 'S', '1', 1, 1, 8, 0, 0x11, 0x22, 0x33, 0x44, 1, 2, 3, 4, 5, 6, 7, 8};
        Check(deonline::EncodeJoin(nonce, 0x44332211u) == expectedJoin, "JOIN byte layout");
        Check(deonline::EncodeJoin(0, 0).empty(), "JOIN rejects zero nonce");

        const deonline::Transform transform{CVector(1.0f, -2.0f, 3.5f), 0.25f};
        const Bytes expectedUpdate = {'D',  'E',  'S',  '1',  1, 3, 28,   0,    0xdd, 0xcc, 0xbb, 0xaa, 0x11, 0x22, 0x33, 0x44, 0x11, 0x12, 0x13, 0x14,
                                      0x15, 0x16, 0x17, 0x18, 0, 0, 0x80, 0x3f, 0,    0,    0,    0xc0, 0,    0,    0x60, 0x40, 0,    0,    0x80, 0x3e};
        Check(deonline::EncodeUpdate(recipientId, session, transform, 0xaabbccddu) == expectedUpdate, "UPDATE byte layout");
        Check(deonline::EncodeUpdate(0, session, transform, 0).empty(), "UPDATE rejects zero player ID");
        Check(deonline::EncodeUpdate(recipientId, 0, transform, 0).empty(), "UPDATE rejects zero session");

        auto invalid = transform;
        invalid.position.fX = std::numeric_limits<float>::quiet_NaN();
        Check(deonline::EncodeUpdate(recipientId, session, invalid, 0).empty(), "UPDATE rejects NaN position");
        invalid = transform;
        invalid.heading = std::numeric_limits<float>::infinity();
        Check(deonline::EncodeUpdate(recipientId, session, invalid, 0).empty(), "UPDATE rejects infinite heading");
        invalid = transform;
        invalid.position.fZ = 100001.0f;
        Check(deonline::EncodeUpdate(recipientId, session, invalid, 0).empty(), "UPDATE rejects excessive position");
        invalid = transform;
        invalid.heading = -100001.0f;
        Check(deonline::EncodeUpdate(recipientId, session, invalid, 0).empty(), "UPDATE rejects excessive heading");
        const deonline::Transform limits{CVector(100000.0f, -100000.0f, 100000.0f), -100000.0f};
        Check(!deonline::EncodeUpdate(recipientId, session, limits, 0).empty(), "UPDATE accepts exact limits");

        const Bytes expectedLeave = {'D',  'E',  'S',  '1',  1,    5,    12,   0,    0x11, 0x22, 0x33, 0x44,
                                     0x11, 0x22, 0x33, 0x44, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
        Check(deonline::EncodeLeave(recipientId, session, 0x44332211u) == expectedLeave, "LEAVE byte layout");
        Check(deonline::EncodeLeave(0, session, 0).empty(), "LEAVE rejects zero player ID");
        Check(deonline::EncodeLeave(recipientId, 0, 0).empty(), "LEAVE rejects zero session");
    }

    void RejectWelcome(const Bytes& bytes, const std::string& message, std::uint64_t expectedNonce = nonce)
    {
        std::uint32_t id = 123;
        std::uint64_t token = 456;
        Check(!deonline::DecodeWelcome(bytes.data(), bytes.size(), expectedNonce, id, token), message);
        Check(id == 123 && token == 456, message + " preserves outputs");
    }

    void TestWelcome()
    {
        std::uint32_t id = 0;
        std::uint64_t token = 0;
        Check(deonline::DecodeWelcome(welcome.data(), welcome.size(), nonce, id, token), "WELCOME accepts independent fixture");
        Check(id == recipientId && token == session, "WELCOME decodes ID and session");
        RejectWelcome(welcome, "WELCOME rejects different nonce", nonce + 1);
        RejectWelcome(welcome, "WELCOME rejects zero expected nonce", 0);

        auto changed = welcome;
        changed[0] = 'X';
        RejectWelcome(changed, "WELCOME rejects magic");
        changed = welcome;
        changed[4] = 2;
        RejectWelcome(changed, "WELCOME rejects version");
        changed = welcome;
        changed[5] = 3;
        RejectWelcome(changed, "WELCOME rejects wrong kind");
        changed = welcome;
        changed[6] = 19;
        RejectWelcome(changed, "WELCOME rejects body length mismatch");
        changed = welcome;
        changed.insert(changed.end(), 0);
        RejectWelcome(changed, "WELCOME rejects trailing bytes");
        changed = welcome;
        changed.resize(deonline::kMaxDatagramSize + 1);
        RejectWelcome(changed, "WELCOME rejects oversized packet");
        changed = welcome;
        std::fill(changed.begin() + 20, changed.begin() + 24, std::uint8_t{0});
        RejectWelcome(changed, "WELCOME rejects zero assigned ID");
        changed = welcome;
        std::fill(changed.begin() + 24, changed.end(), std::uint8_t{0});
        RejectWelcome(changed, "WELCOME rejects zero assigned session");

        for (std::size_t size = 0; size < welcome.size(); ++size)
            RejectWelcome(Bytes(welcome.begin(), welcome.begin() + size), "WELCOME rejects truncation " + std::to_string(size));

        id = 123;
        token = 456;
        Check(!deonline::DecodeWelcome(nullptr, welcome.size(), nonce, id, token), "WELCOME rejects null data");
        Check(id == 123 && token == 456, "null WELCOME preserves outputs");
    }

    void RejectSnapshot(const Bytes& bytes, const std::string& message, std::uint32_t expectedId = recipientId, std::uint64_t expectedSession = session)
    {
        std::uint32_t                      sequence = 123;
        std::vector<deonline::PlayerState> players{{999, {CVector(9.0f, 8.0f, 7.0f), 6.0f}}};
        Check(!deonline::DecodeSnapshot(bytes.data(), bytes.size(), expectedId, expectedSession, sequence, players), message);
        Check(sequence == 123 && players.size() == 1 && players[0].id == 999 && players[0].transform.position == CVector(9.0f, 8.0f, 7.0f) &&
                  players[0].transform.heading == 6.0f,
              message + " preserves outputs");
    }

    void TestSnapshot()
    {
        std::uint32_t                      sequence = 0;
        std::vector<deonline::PlayerState> players;
        Check(deonline::DecodeSnapshot(snapshot.data(), snapshot.size(), recipientId, session, sequence, players), "SNAPSHOT accepts independent fixture");
        Check(sequence == 0xfffffffeu && players.size() == 2, "SNAPSHOT decodes sequence and count");
        Check(players[0].id == 1 && players[0].transform.position == CVector(1.0f, -2.0f, 3.5f) && players[0].transform.heading == 0.25f,
              "SNAPSHOT decodes first transform");
        Check(players[1].id == 2 && players[1].transform.position == CVector(4.0f, 5.0f, 6.0f) && players[1].transform.heading == 7.0f,
              "SNAPSHOT decodes second transform");
        RejectSnapshot(snapshot, "SNAPSHOT rejects different recipient", recipientId + 1);
        RejectSnapshot(snapshot, "SNAPSHOT rejects different session", recipientId, session + 1);
        RejectSnapshot(snapshot, "SNAPSHOT rejects zero expected recipient", 0);
        RejectSnapshot(snapshot, "SNAPSHOT rejects zero expected session", recipientId, 0);

        auto changed = snapshot;
        changed[4] = 2;
        RejectSnapshot(changed, "SNAPSHOT rejects version");
        changed = snapshot;
        changed[5] = 2;
        RejectSnapshot(changed, "SNAPSHOT rejects wrong kind");
        changed = snapshot;
        changed[24] = 17;
        RejectSnapshot(changed, "SNAPSHOT rejects excessive count");
        changed = snapshot;
        changed[24] = 1;
        RejectSnapshot(changed, "SNAPSHOT rejects count and length mismatch");
        changed = snapshot;
        changed[46] = 1;
        RejectSnapshot(changed, "SNAPSHOT rejects duplicate IDs");
        changed = snapshot;
        changed[26] = 0;
        RejectSnapshot(changed, "SNAPSHOT rejects zero record ID");
        changed = snapshot;
        changed[30] = 0;
        changed[31] = 0;
        changed[32] = 0xc0;
        changed[33] = 0x7f;
        RejectSnapshot(changed, "SNAPSHOT rejects NaN position");
        changed = snapshot;
        changed[62] = 0;
        changed[63] = 0;
        changed[64] = 0x80;
        changed[65] = 0x7f;
        RejectSnapshot(changed, "SNAPSHOT rejects infinite heading in later record");
        changed = snapshot;
        // IEEE-754 131072.0 is 0x48000000, above the documented transform limit.
        changed[30] = 0;
        changed[31] = 0;
        changed[32] = 0;
        changed[33] = 0x48;
        RejectSnapshot(changed, "SNAPSHOT rejects excessive position");
        changed = snapshot;
        changed.push_back(0);
        RejectSnapshot(changed, "SNAPSHOT rejects trailing bytes");
        changed = snapshot;
        changed.resize(deonline::kMaxDatagramSize + 1);
        RejectSnapshot(changed, "SNAPSHOT rejects oversized packet");

        for (std::size_t size = 0; size < snapshot.size(); ++size)
            RejectSnapshot(Bytes(snapshot.begin(), snapshot.begin() + size), "SNAPSHOT rejects truncation " + std::to_string(size));

        changed = snapshot;
        changed.resize(26);
        changed[6] = 14;
        changed[24] = 0;
        Check(deonline::DecodeSnapshot(changed.data(), changed.size(), recipientId, session, sequence, players), "SNAPSHOT accepts zero players");
        Check(players.empty(), "empty SNAPSHOT clears previous players");

        changed[6] = 0x4e;
        changed[7] = 1;
        changed[24] = 16;
        for (std::uint8_t id = 1; id <= 16; ++id)
        {
            changed.insert(changed.end(), snapshot.begin() + 26, snapshot.begin() + 46);
            changed[26 + (id - 1) * 20] = id;
        }
        Check(deonline::DecodeSnapshot(changed.data(), changed.size(), recipientId, session, sequence, players), "SNAPSHOT accepts maximum player count");
        Check(players.size() == deonline::kMaxPlayers && players.back().id == 16, "maximum SNAPSHOT decodes all players");

        changed = snapshot;
        changed.resize(12);
        changed[6] = 0;
        RejectSnapshot(changed, "SNAPSHOT rejects short body despite valid envelope");
    }

    void TestHeader()
    {
        deonline::Header header;
        Check(deonline::DecodeHeader(welcome.data(), welcome.size(), header), "header accepts envelope");
        Check(header.kind == deonline::MessageKind::Welcome && header.bodyLength == 20 && header.sequence == 0x44332211u, "header reads little endian values");
        for (const std::uint8_t kind : {std::uint8_t{0}, std::uint8_t{6}, std::uint8_t{255}})
        {
            auto changed = welcome;
            changed[5] = kind;
            Check(!deonline::DecodeHeader(changed.data(), changed.size(), header), "header rejects unrecognized type");
            Check(header.kind == deonline::MessageKind::Welcome && header.bodyLength == 20 && header.sequence == 0x44332211u,
                  "invalid header preserves output");
        }
    }

    void TestSequenceAndMath()
    {
        Check(deonline::IsNewerSequence(11, 10), "newer sequence accepted");
        Check(!deonline::IsNewerSequence(10, 10), "duplicate sequence rejected");
        Check(!deonline::IsNewerSequence(9, 10), "older sequence rejected");
        Check(deonline::IsNewerSequence(0, 0xffffffffu), "sequence wrap accepted");
        Check(!deonline::IsNewerSequence(0xffffffffu, 0), "pre-wrap sequence rejected after wrap");
        Check(!deonline::IsNewerSequence(0x80000000u, 0), "ambiguous half-space sequence rejected");
        Check(deonline::IsNewerSequence(0x7fffffffu, 0), "largest unambiguous sequence accepted");

        const CVector from(1.0f, 2.0f, 3.0f);
        const CVector to(5.0f, 6.0f, 7.0f);
        Check(deonline::InterpolatePosition(from, to, 0.5f) == CVector(3.0f, 4.0f, 5.0f), "MTA vector interpolation midpoint");
        Check(deonline::InterpolatePosition(from, to, -0.5f) == from, "MTA vector interpolation clamps below zero");
        Check(deonline::InterpolatePosition(from, to, 1.5f) == to, "MTA vector interpolation clamps above one");
        Check(deonline::InterpolatePosition(from, to, std::numeric_limits<float>::quiet_NaN()) == from, "MTA vector interpolation handles NaN timing");
        Check(deonline::InterpolatePosition(from, to, std::numeric_limits<float>::infinity()) == to, "MTA vector interpolation clamps infinite timing");
    }
}

int main()
{
    try
    {
        TestEncoders();
        TestWelcome();
        TestSnapshot();
        TestHeader();
        TestSequenceAndMath();
        std::cout << "Passed " << checks << " protocol and MTA math checks.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
