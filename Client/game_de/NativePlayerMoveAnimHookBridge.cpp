/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded framed call bridges for the two proven movement decisions
 ****************************************************************************/
#include "NativePlayerMoveAnimHookBridge.h"
#include "NativePlayerMoveAnimHookProfile.h"
#include <cstring>
#include <limits>

namespace mta::game_de::NativePlayerMoveAnimHookBridge
{
    namespace
    {
        struct Writer
        {
            Program&      output;
            std::uint32_t cursor = 0;
            bool          good = true;
            void          Byte(unsigned char value) noexcept
            {
                if (cursor == output.bytes.size())
                    good = false;
                else
                    output.bytes[cursor++] = value;
            }
            void Word(std::uint16_t value) noexcept
            {
                Byte(static_cast<unsigned char>(value));
                Byte(static_cast<unsigned char>(value >> 8));
            }
            void Dword(std::uint32_t value) noexcept
            {
                for (unsigned shift = 0; shift != 32; shift += 8)
                    Byte(static_cast<unsigned char>(value >> shift));
            }
            void Qword(std::uintptr_t value) noexcept
            {
                for (unsigned shift = 0; shift != 64; shift += 8)
                    Byte(static_cast<unsigned char>(value >> shift));
            }
            void MoveSlot(bool restore, std::uint32_t reg, std::uint32_t offset) noexcept
            {
                Byte(static_cast<unsigned char>(0x48 | (reg >= 8 ? 4 : 0)));
                Byte(static_cast<unsigned char>(restore ? 0x8b : 0x89));
                const auto operand = static_cast<unsigned char>((reg & 7) << 3);
                Byte(static_cast<unsigned char>((offset <= 0x7f ? 0x44 : 0x84) | operand));
                Byte(0x24);
                if (offset <= 0x7f)
                    Byte(static_cast<unsigned char>(offset));
                else
                    Dword(offset);
            }
            void Fx(bool restore) noexcept
            {
                Byte(0x48);
                Byte(0x0f);
                Byte(0xae);
                Byte(static_cast<unsigned char>(restore ? 0x8c : 0x84));
                Byte(0x24);
                Dword(FxOffset);
            }
            void Counter(std::uintptr_t address, bool leave) noexcept
            {
                Byte(0x48);
                Byte(0xb8);
                Qword(address);
                Byte(0xf0);
                Byte(0xff);
                Byte(static_cast<unsigned char>(leave ? 0x08 : 0x00));
            }
        };
        bool Nonvolatile(std::uint32_t reg) noexcept
        {
            return reg == 3 || reg == 6 || reg == 7 || (reg >= 12 && reg <= 15);
        }
        bool Valid(Kind kind) noexcept
        {
            return kind == Kind::ProcessCompare || kind == Kind::ClothesCall;
        }
        void Unwind(Writer& writer) noexcept
        {
            auto& output = writer.output;
            while (writer.cursor & 15)
                writer.Byte(0);
            output.unwindOffset = writer.cursor;
            // Seven fixed nonvolatile saves consume two slots each. PUSHFQ
            // is an eight-byte allocation, not a register-restoration opcode.
            constexpr unsigned count = 19;
            writer.Byte(1);
            writer.Byte(static_cast<unsigned char>(output.layout.prologueEnd));
            writer.Byte(count);
            writer.Byte(0xf5);  // RBP, established at fixed RSP+F0.
            for (std::size_t index = GprSlots.size(); index != 0; --index)
            {
                const auto& slot = GprSlots[index - 1];
                if (!Nonvolatile(slot.reg))
                    continue;
                writer.Byte(static_cast<unsigned char>(output.layout.saveEnds[index - 1]));
                writer.Byte(static_cast<unsigned char>((slot.reg << 4) | 4));  // UWOP_SAVE_NONVOL
                writer.Word(static_cast<std::uint16_t>(slot.offset / 8));
            }
            writer.Byte(static_cast<unsigned char>(output.layout.frameSetEnd));
            writer.Byte(3);  // UWOP_SET_FPREG
            writer.Byte(static_cast<unsigned char>(output.layout.allocationEnd));
            writer.Byte(1);  // UWOP_ALLOC_LARGE, 16-bit operand scaled by eight
            writer.Word(static_cast<std::uint16_t>(FrameAllocation / 8));
            writer.Byte(static_cast<unsigned char>(output.layout.pushFlagsEnd));
            writer.Byte(2);  // UWOP_ALLOC_SMALL, eight bytes
            writer.Byte(static_cast<unsigned char>(output.layout.pushRbpEnd));
            writer.Byte(0x50);  // UWOP_PUSH_NONVOL RBP
            writer.Word(0);     // Odd slot count is padded to a DWORD boundary.
            output.unwindSize = writer.cursor - output.unwindOffset;
        }
    }

    bool Emit(const Request& request, Program& output) noexcept
    {
        if (sizeof(void*) != 8 || !Valid(request.kind) || !request.decision || !request.inFlight ||
            reinterpret_cast<std::uintptr_t>(request.inFlight) % alignof(LONG) ||
            (request.kind == Kind::ClothesCall ? !request.originalConsumer : request.originalConsumer != 0))
            return false;
        static_assert(sizeof(LONG) == 4 && FxOffset % 16 == 0 && FxOffset + FxBytes <= FrameAllocation);
        static_assert(EpilogueFrameDisplacement == 0x1c0 && ReturnAddressOffset == 0x2b8);
        Program next;
        Writer  writer{next};
        auto&   layout = next.layout;
        writer.Byte(0x55);
        layout.pushRbpEnd = writer.cursor;
        writer.Byte(0x9c);
        layout.pushFlagsEnd = writer.cursor;
        writer.Byte(0x48);
        writer.Byte(0x81);
        writer.Byte(0xec);
        writer.Dword(FrameAllocation);
        layout.allocationEnd = writer.cursor;
        writer.Byte(0x48);
        writer.Byte(0x8d);
        writer.Byte(0xac);
        writer.Byte(0x24);
        writer.Dword(FrameRegisterOffset);
        layout.frameSetEnd = writer.cursor;
        for (std::size_t index = 0; index != GprSlots.size(); ++index)
        {
            writer.MoveSlot(false, GprSlots[index].reg, GprSlots[index].offset);
            layout.saveEnds[index] = writer.cursor;
        }
        layout.prologueEnd = writer.cursor;
        layout.fxSave = writer.cursor;
        writer.Fx(false);
        layout.enterCounter = writer.cursor;
        writer.Counter(reinterpret_cast<std::uintptr_t>(request.inFlight), false);
        // Native DF is preserved in the saved flags, but a normal Windows C++
        // callback must enter with the direction flag clear.
        writer.Byte(0xfc);
        if (request.kind == Kind::ProcessCompare)
        {
            // Enter-counter bookkeeping uses RAX only after saving it. Read
            // the incoming request from that saved slot, never its scratch.
            writer.Byte(0x8b);
            writer.Byte(0x54);
            writer.Byte(0x24);
            writer.Byte(0x20);  // EDX=[RSP+saved RAX].
        }
        else
        {
            writer.Byte(0x89);
            writer.Byte(0xca);  // EDX=the still-current incoming ECX.
        }
        writer.Byte(0x48);
        writer.Byte(0x89);
        writer.Byte(static_cast<unsigned char>(request.kind == Kind::ProcessCompare ? 0xd9 : 0xf9));  // RCX=RBX or RDI.
        writer.Byte(0x49);
        writer.Byte(0xb8);
        writer.Qword(reinterpret_cast<std::uintptr_t>(request.context));
        writer.Byte(0x48);
        writer.Byte(0xb8);
        writer.Qword(reinterpret_cast<std::uintptr_t>(request.decision));
        layout.callbackCall = writer.cursor;
        writer.Byte(0xff);
        writer.Byte(0xd0);
        // The only intentional register change is the selected 32-bit group.
        // Explicitly zero the upper half instead of borrowing a callback's
        // unspecified high return bits.
        writer.Byte(0x89);
        writer.Byte(0xc0);
        writer.MoveSlot(false, 0, request.kind == Kind::ProcessCompare ? GprSlots[0].offset : GprSlots[1].offset);
        layout.decisionStoreEnd = writer.cursor;
        layout.leaveCounter = writer.cursor;
        writer.Counter(reinterpret_cast<std::uintptr_t>(request.inFlight), true);
        layout.fxRestore = writer.cursor;
        writer.Fx(true);
        for (std::size_t index = 0; index != GprSlots.size(); ++index)
        {
            writer.MoveSlot(true, GprSlots[index].reg, GprSlots[index].offset);
            layout.restoreEnds[index] = writer.cursor;
        }
        layout.flagsPush = writer.cursor;
        writer.Byte(0xff);
        writer.Byte(0xb5);
        writer.Dword(SavedFlagsOffset - FrameRegisterOffset);
        layout.flagsPop = writer.cursor;
        writer.Byte(0x9d);
        if (request.kind == Kind::ProcessCompare)
        {
            layout.compare = writer.cursor;
            for (const auto byte : NativePlayerMoveAnimHookProfile::ProcessCompareBytes)
                writer.Byte(byte);
        }
        // RBP still anchors the full frame at the faultable CMP above. This
        // LEA/POP/RET-or-indirect-JMP sequence is a recognized x64 epilogue;
        // there is no stack-restored CMP with stale full-frame unwind rules.
        layout.epilogue = writer.cursor;
        writer.Byte(0x48);
        writer.Byte(0x8d);
        writer.Byte(0xa5);
        writer.Dword(EpilogueFrameDisplacement);
        layout.popRbp = writer.cursor;
        writer.Byte(0x5d);
        layout.tail = writer.cursor;
        if (request.kind == Kind::ProcessCompare)
            writer.Byte(0xc3);
        else
        {
            writer.Byte(0xff);
            writer.Byte(0x25);
            writer.Dword(0);
            next.codeSize = writer.cursor;
            layout.consumerLiteral = writer.cursor;
            writer.Qword(request.originalConsumer);
        }
        if (!next.codeSize)
            next.codeSize = writer.cursor;
        if (layout.prologueEnd > 255)
            return false;
        Unwind(writer);
        if (!writer.good || next.unwindSize != 44)
            return false;
        next.size = writer.cursor;
        next.function.BeginAddress = 0;
        next.function.EndAddress = next.codeSize;
        next.function.UnwindData = next.unwindOffset;
        output = next;
        return true;
    }

    bool EncodeCallPatch(std::uintptr_t site, std::uintptr_t stub, Kind kind, CallPatch& output) noexcept
    {
        if (sizeof(void*) != 8 || !site || !stub || !Valid(kind) || site > (std::numeric_limits<std::uintptr_t>::max)() - 5)
            return false;
        const auto    after = site + 5;
        std::uint32_t displacement = 0;
        if (stub >= after)
        {
            if (stub - after > 0x7fffffff)
                return false;
            displacement = static_cast<std::uint32_t>(stub - after);
        }
        else
        {
            if (after - stub > 0x80000000)
                return false;
            displacement = 0u - static_cast<std::uint32_t>(after - stub);
        }
        CallPatch next;
        next.bytes[0] = 0xe8;
        std::memcpy(next.bytes.data() + 1, &displacement, sizeof(displacement));
        next.size = kind == Kind::ProcessCompare ? 6u : 5u;
        if (next.size == 6)
            next.bytes[5] = 0x90;
        output = next;
        return true;
    }
}
