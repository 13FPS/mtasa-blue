/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify real Windows unwind behavior, including partial bridge frames
 ****************************************************************************/
namespace
{
    void CheckBridgeUnwind(const Bridge::Program& program, std::uintptr_t base)
    {
        // This model is deliberately tied to the emitted instructions, not just
        // successful table registration. A different prologue needs a new model.
        constexpr std::array<unsigned char, 17> expectedPrologue{
            {0x55, 0x9c, 0x48, 0x81, 0xec, 0xa8, 0x02, 0x00, 0x00, 0x48, 0x8d, 0xac, 0x24, 0xf0, 0x00, 0x00, 0x00}};
        const bool validLayout = base && program.codeSize <= program.bytes.size() && program.layout.pushRbpEnd == 1 && program.layout.pushFlagsEnd == 2 &&
                                 program.layout.allocationEnd == 9 && program.layout.frameSetEnd == expectedPrologue.size() &&
                                 Bridge::FrameAllocation == 0x2a8 && Bridge::FrameRegisterOffset == 0xf0 && Bridge::ReturnAddressOffset == 0x2b8 &&
                                 Bridge::SavedRbpOffset == 0x2b0 && program.layout.popRbp > program.layout.epilogue &&
                                 program.layout.tail > program.layout.popRbp && program.layout.tail < program.codeSize &&
                                 std::memcmp(program.bytes.data(), expectedPrologue.data(), expectedPrologue.size()) == 0;
        Check(validLayout, "unwind model matches actual PUSH/PUSHFQ/SUB/FP prologue");
        if (!validLayout)
            return;

        auto gpr = [](CONTEXT& state, std::uint32_t reg) -> DWORD64&
        {
            switch (reg)
            {
                case 0:
                    return state.Rax;
                case 1:
                    return state.Rcx;
                case 2:
                    return state.Rdx;
                case 3:
                    return state.Rbx;
                case 4:
                    return state.Rsp;
                case 5:
                    return state.Rbp;
                case 6:
                    return state.Rsi;
                case 7:
                    return state.Rdi;
                case 8:
                    return state.R8;
                case 9:
                    return state.R9;
                case 10:
                    return state.R10;
                case 11:
                    return state.R11;
                case 12:
                    return state.R12;
                case 13:
                    return state.R13;
                case 14:
                    return state.R14;
                default:
                    return state.R15;
            }
        };
        auto nonvolatile = [](std::uint32_t reg) { return reg == 3 || reg == 6 || reg == 7 || (reg >= 12 && reg <= 15); };

        struct Point
        {
            std::uint32_t pc;
            const char*   reason;
        };
        std::array<Point, 64> points{};
        std::size_t           pointCount = 0;
        auto                  add = [&](std::uint32_t pc, const char* reason)
        {
            for (std::size_t index = 0; index < pointCount; ++index)
                if (points[index].pc == pc)
                    return;
            Check(pc < program.codeSize && pointCount < points.size(), "unwind test PC stays inside registered function");
            if (pc < program.codeSize && pointCount < points.size())
                points[pointCount++] = {pc, reason};
        };
        add(0, "unwind at entry before PUSH RBP");
        add(program.layout.pushRbpEnd, "unwind after PUSH RBP");
        add(program.layout.pushFlagsEnd, "unwind after PUSHFQ");
        add(program.layout.allocationEnd, "unwind after SUB before FP establishment");
        add(program.layout.frameSetEnd, "unwind after FP establishment before saves");
        for (const auto pc : program.layout.saveEnds)
            add(pc, "unwind at each partial-prologue saveEnd");
        add(program.layout.prologueEnd, "unwind at complete prologue");
        add(program.layout.fxSave, "unwind at FXSAVE body entry");
        add(program.layout.enterCounter, "unwind at body counter");
        add(program.layout.callbackCall, "unwind immediately before callback CALL");
        add(program.layout.callbackCall + 2, "unwind immediately after callback CALL");
        add(program.layout.decisionStoreEnd, "unwind after selected group store");
        add(program.layout.leaveCounter, "unwind at leaving counter");
        add(program.layout.fxRestore, "unwind at FXRSTOR");
        for (const auto pc : program.layout.restoreEnds)
            add(pc, "unwind at each explicit register restoreEnd");
        add(program.layout.flagsPush, "unwind before temporary saved-flags PUSH");
        add(program.layout.flagsPop, "unwind after temporary PUSH before POPFQ");
        add(program.layout.flagsPop + 1, "unwind after POPFQ with FP still established");
        if (program.layout.compare)
            add(program.layout.compare, "unwind at faultable final CMP");
        add(program.layout.epilogue, "unwind at standard LEA epilogue");
        add(program.layout.popRbp, "unwind at POP RBP after LEA");
        add(program.layout.tail, "unwind at final RET or indirect JMP");

        for (std::size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
        {
            const auto point = points[pointIndex];
            // Keep incorrect FP/stack interpretations inside owned storage, so
            // a wrong unwind produces a failed assertion rather than reading an
            // arbitrary address. The real native stack is never synthesized.
            alignas(16) std::array<DWORD64, 1024> stack{};
            for (std::size_t index = 0; index < stack.size(); ++index)
                stack[index] = 0xdead000000000000ull + index * 0x101ull;
            const auto              stackBegin = reinterpret_cast<DWORD64>(stack.data());
            const auto              entrySp = stackBegin + 0x1008;
            const auto              frameSp = entrySp - Bridge::ReturnAddressOffset;
            const auto              callerRbp = stackBegin + 0x800;
            const auto              callerRip = reinterpret_cast<DWORD64>(&CheckBridgeUnwind);
            constexpr DWORD         incomingFlags = 0x647;
            constexpr DWORD         bodyFlags = 0x202;
            std::array<DWORD64, 16> incoming{};
            for (std::size_t index = 0; index < incoming.size(); ++index)
                incoming[index] = 0x1020304050607000ull + index * 0x10101ull;
            incoming[4] = entrySp;
            incoming[5] = callerRbp;
            incoming[9] = 0xa5a51234fedc9876ull;

            auto write = [&](DWORD64 address, DWORD64 value)
            {
                const bool inStack =
                    address >= stackBegin && address + sizeof(value) <= stackBegin + sizeof(stack) && (address - stackBegin) % alignof(DWORD64) == 0;
                Check(inStack, "synthetic unwind writes stay in aligned owned stack");
                if (inStack)
                    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
            };
            write(entrySp, callerRip);
            if (point.pc >= program.layout.pushRbpEnd)
                write(entrySp - 8, callerRbp);
            if (point.pc >= program.layout.pushFlagsEnd)
                write(entrySp - 16, incomingFlags);

            CONTEXT state{};
            state.ContextFlags = CONTEXT_FULL;
            state.MxCsr = 0x1f80;
            for (std::uint32_t reg = 0; reg < incoming.size(); ++reg)
                gpr(state, reg) = incoming[reg];
            state.Rip = base + point.pc;
            state.Rsp = entrySp;
            if (point.pc >= program.layout.pushRbpEnd)
                state.Rsp -= 8;
            if (point.pc >= program.layout.pushFlagsEnd)
                state.Rsp -= 8;
            if (point.pc >= program.layout.allocationEnd)
                state.Rsp -= Bridge::FrameAllocation;
            if (point.pc >= program.layout.frameSetEnd)
                state.Rbp = frameSp + Bridge::FrameRegisterOffset;

            for (std::size_t index = 0; index < Bridge::GprSlots.size(); ++index)
            {
                const auto slot = Bridge::GprSlots[index];
                const bool saved = point.pc >= program.layout.saveEnds[index];
                const bool restored = point.pc >= program.layout.restoreEnds[index];
                if (saved)
                    write(frameSp + slot.offset, incoming[slot.reg]);
                // Poison eligible saved nonvolatiles independently of their
                // stack values. Otherwise missing/early partial unwind codes
                // could pass simply because a live register equals the caller.
                if (saved && !restored && (nonvolatile(slot.reg) || point.pc >= program.layout.callbackCall + 2))
                    gpr(state, slot.reg) = 0xbad0000000000000ull + slot.reg * 0x10001ull;
                if (restored)
                    gpr(state, slot.reg) = incoming[slot.reg];
            }
            if (point.pc >= program.layout.decisionStoreEnd)
            {
                const auto        selectedReg = program.layout.compare ? 0u : 1u;
                const auto        selectedSlot = program.layout.compare ? 0u : 1u;
                constexpr DWORD64 selected = 0x83;
                write(frameSp + Bridge::GprSlots[selectedSlot].offset, selected);
                if (point.pc >= program.layout.restoreEnds[selectedSlot])
                    gpr(state, selectedReg) = selected;
            }

            if (point.pc == program.layout.flagsPop)
            {
                write(frameSp - 8, incomingFlags);
                state.Rsp = frameSp - 8;
            }
            if (point.pc >= program.layout.popRbp)
                state.Rsp = entrySp - 8;
            if (point.pc >= program.layout.tail)
            {
                state.Rsp = entrySp;
                state.Rbp = callerRbp;
            }
            state.EFlags = point.pc <= program.layout.flagsPush || point.pc > program.layout.flagsPop ? incomingFlags : bodyFlags;
            const auto currentR9 = state.R9;
            const auto currentFlags = state.EFlags;

            DWORD64    lookupBase = 0;
            const auto function = RtlLookupFunctionEntry(state.Rip, &lookupBase, nullptr);
            const bool tableMatches = function && lookupBase == base && function->BeginAddress == program.function.BeginAddress &&
                                      function->EndAddress == program.function.EndAddress && function->UnwindData == program.function.UnwindData;
            Check(tableMatches, "real lookup resolves the registered bridge table");
            if (!tableMatches)
                continue;
            PVOID      handlerData = nullptr;
            DWORD64    establisherFrame = 0;
            const auto handler = RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, state.Rip, function, &state, &handlerData, &establisherFrame, nullptr);
            bool       callerMatches = state.Rip == callerRip && state.Rsp == entrySp + 8 && state.Rbp == callerRbp;
            for (const auto slot : Bridge::GprSlots)
                if (nonvolatile(slot.reg))
                    callerMatches = callerMatches && gpr(state, slot.reg) == incoming[slot.reg];
            Check(handler == nullptr && callerMatches, point.reason);
            Check(state.R9 == currentR9, "virtual unwind does not invent restoration of volatile R9");
            if (point.pc <= program.layout.callbackCall || point.pc >= program.layout.restoreEnds[7])
                Check(state.R9 == incoming[9], "incoming full-width R9 survives before callback and after explicit restore");
            Check(state.EFlags == currentFlags, "PUSHFQ allocation is not a fabricated flags-restoration unwind code");
            if (point.pc == program.layout.flagsPop)
                Check(establisherFrame == frameSp, "temporary RSP-8 flags-pop frame uses stable FP anchor");
        }
    }
}
