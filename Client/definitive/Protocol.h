/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto - Definitive Edition experiment
 *  LICENSE:     See LICENSE in the top level directory
 *  PURPOSE:     Standalone, versioned transform protocol for one small server
 *
 *****************************************************************************/

#pragma once

#include "MtaMath.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace deonline
{
    inline constexpr std::size_t  kHeaderSize = 12;
    inline constexpr std::size_t  kMaxDatagramSize = 512;
    inline constexpr std::size_t  kMaxPlayers = 16;
    inline constexpr std::uint8_t kProtocolVersion = 1;
    inline constexpr float        kTransformLimit = 100000.0f;

    enum class MessageKind : std::uint8_t
    {
        Join = 1,
        Welcome = 2,
        Update = 3,
        Snapshot = 4,
        Leave = 5
    };

    struct Header
    {
        MessageKind   kind = MessageKind::Join;
        std::uint16_t bodyLength = 0;
        std::uint32_t sequence = 0;
    };

    struct Transform
    {
        CVector position;
        float   heading = 0.0f;
    };

    struct PlayerState
    {
        std::uint32_t id = 0;
        Transform     transform;
    };

    inline bool IsValidTransform(const Transform& transform)
    {
        return transform.position.IsValid() && std::isfinite(transform.heading) && std::abs(transform.position.fX) <= kTransformLimit &&
               std::abs(transform.position.fY) <= kTransformLimit && std::abs(transform.position.fZ) <= kTransformLimit &&
               std::abs(transform.heading) <= kTransformLimit;
    }

    inline bool IsNewerSequence(std::uint32_t candidate, std::uint32_t previous)
    {
        // Unsigned subtraction preserves ordering through wrap. Exactly half
        // the sequence space is ambiguous and must not displace known state.
        const std::uint32_t difference = candidate - previous;
        return difference != 0 && difference < 0x80000000u;
    }

    namespace detail
    {
        inline void Write16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value));
            bytes.push_back(static_cast<std::uint8_t>(value >> 8));
        }

        inline void Write32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
        {
            for (unsigned int shift = 0; shift < 32; shift += 8)
                bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        }

        inline void Write64(std::vector<std::uint8_t>& bytes, std::uint64_t value)
        {
            for (unsigned int shift = 0; shift < 64; shift += 8)
                bytes.push_back(static_cast<std::uint8_t>(value >> shift));
        }

        inline void WriteFloat(std::vector<std::uint8_t>& bytes, float value)
        {
            static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559,
                          "The experimental wire protocol uses IEEE-754 binary32.");
            // memcpy preserves float bits without aliasing or depending on
            // native structure padding; integer writers fix wire byte order.
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            Write32(bytes, bits);
        }

        inline std::uint16_t Read16(const std::uint8_t* bytes)
        {
            return static_cast<std::uint16_t>(bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8));
        }

        inline std::uint32_t Read32(const std::uint8_t* bytes)
        {
            std::uint32_t value = 0;
            for (unsigned int index = 0; index < 4; ++index)
                value |= static_cast<std::uint32_t>(bytes[index]) << (index * 8);
            return value;
        }

        inline std::uint64_t Read64(const std::uint8_t* bytes)
        {
            std::uint64_t value = 0;
            for (unsigned int index = 0; index < 8; ++index)
                value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
            return value;
        }

        inline float ReadFloat(const std::uint8_t* bytes)
        {
            const std::uint32_t bits = Read32(bytes);
            float               value = 0.0f;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }

        inline std::vector<std::uint8_t> StartPacket(MessageKind kind, std::uint16_t bodyLength, std::uint32_t sequence)
        {
            std::vector<std::uint8_t> bytes;
            bytes.reserve(kHeaderSize + bodyLength);
            bytes.insert(bytes.end(), {'D', 'E', 'S', '1', kProtocolVersion, static_cast<std::uint8_t>(kind)});
            Write16(bytes, bodyLength);
            Write32(bytes, sequence);
            return bytes;
        }
    }

    inline bool DecodeHeader(const std::uint8_t* bytes, std::size_t size, Header& output)
    {
        if (!bytes || size < kHeaderSize || size > kMaxDatagramSize || bytes[0] != 'D' || bytes[1] != 'E' || bytes[2] != 'S' || bytes[3] != '1' ||
            bytes[4] != kProtocolVersion || bytes[5] < static_cast<std::uint8_t>(MessageKind::Join) || bytes[5] > static_cast<std::uint8_t>(MessageKind::Leave))
            return false;

        Header candidate;
        candidate.kind = static_cast<MessageKind>(bytes[5]);
        candidate.bodyLength = detail::Read16(bytes + 6);
        candidate.sequence = detail::Read32(bytes + 8);
        if (size != kHeaderSize + candidate.bodyLength)
            return false;

        output = candidate;
        return true;
    }

    // An empty encoded datagram signals invalid local input and must never be
    // transmitted, so bad game-adapter data cannot enter the relay protocol.
    inline std::vector<std::uint8_t> EncodeJoin(std::uint64_t nonce, std::uint32_t sequence)
    {
        if (nonce == 0)
            return {};
        auto bytes = detail::StartPacket(MessageKind::Join, 8, sequence);
        detail::Write64(bytes, nonce);
        return bytes;
    }

    inline std::vector<std::uint8_t> EncodeUpdate(std::uint32_t id, std::uint64_t session, const Transform& transform, std::uint32_t sequence)
    {
        if (id == 0 || session == 0 || !IsValidTransform(transform))
            return {};
        auto bytes = detail::StartPacket(MessageKind::Update, 28, sequence);
        detail::Write32(bytes, id);
        detail::Write64(bytes, session);
        detail::WriteFloat(bytes, transform.position.fX);
        detail::WriteFloat(bytes, transform.position.fY);
        detail::WriteFloat(bytes, transform.position.fZ);
        detail::WriteFloat(bytes, transform.heading);
        return bytes;
    }

    inline std::vector<std::uint8_t> EncodeLeave(std::uint32_t id, std::uint64_t session, std::uint32_t sequence)
    {
        if (id == 0 || session == 0)
            return {};
        auto bytes = detail::StartPacket(MessageKind::Leave, 12, sequence);
        detail::Write32(bytes, id);
        detail::Write64(bytes, session);
        return bytes;
    }

    inline bool DecodeWelcome(const std::uint8_t* bytes, std::size_t size, std::uint64_t expectedNonce, std::uint32_t& idOutput, std::uint64_t& sessionOutput)
    {
        Header header;
        if (expectedNonce == 0 || !DecodeHeader(bytes, size, header) || header.kind != MessageKind::Welcome || header.bodyLength != 20)
            return false;

        const std::uint64_t nonce = detail::Read64(bytes + kHeaderSize);
        const std::uint32_t id = detail::Read32(bytes + kHeaderSize + 8);
        const std::uint64_t session = detail::Read64(bytes + kHeaderSize + 12);
        if (nonce != expectedNonce || id == 0 || session == 0)
            return false;

        idOutput = id;
        sessionOutput = session;
        return true;
    }

    inline bool DecodeSnapshot(const std::uint8_t* bytes, std::size_t size, std::uint32_t expectedId, std::uint64_t expectedSession,
                               std::uint32_t& sequenceOutput, std::vector<PlayerState>& playersOutput)
    {
        Header header;
        if (expectedId == 0 || expectedSession == 0 || !DecodeHeader(bytes, size, header) || header.kind != MessageKind::Snapshot || header.bodyLength < 14)
            return false;

        const auto*         body = bytes + kHeaderSize;
        const std::uint32_t recipientId = detail::Read32(body);
        const std::uint64_t session = detail::Read64(body + 4);
        const std::uint16_t count = detail::Read16(body + 12);
        if (recipientId != expectedId || session != expectedSession || count > kMaxPlayers || header.bodyLength != 14 + count * 20)
            return false;

        // Decode into temporary state. A malformed later record must not leave
        // a partially accepted snapshot or sequence in the client's world.
        std::vector<PlayerState> players;
        players.reserve(count);
        for (std::uint16_t index = 0; index < count; ++index)
        {
            const auto* record = body + 14 + index * 20;
            PlayerState player;
            player.id = detail::Read32(record);
            player.transform.position = CVector(detail::ReadFloat(record + 4), detail::ReadFloat(record + 8), detail::ReadFloat(record + 12));
            player.transform.heading = detail::ReadFloat(record + 16);
            if (player.id == 0 || !IsValidTransform(player.transform))
                return false;
            for (const auto& previous : players)
                if (previous.id == player.id)
                    return false;
            players.push_back(player);
        }

        playersOutput = std::move(players);
        sequenceOutput = header.sequence;
        return true;
    }
}
