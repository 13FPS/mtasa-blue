; Exercise the real generated bridges with native live state, independently of GTA.
option casemap:none
EXTERN g_MoveAnimBridgeSeed:BYTE
EXTERN g_MoveAnimBridgeAfter:BYTE
EXTERN g_MoveAnimBridgeConsumer:BYTE
EXTERN g_MoveAnimBridgeTarget:QWORD
EXTERN g_MoveAnimBridgeBeforeRsp:QWORD
EXTERN g_MoveAnimBridgeAfterRsp:QWORD
EXTERN g_MoveAnimBridgeConsumerRsp:QWORD
EXTERN g_MoveAnimBridgeConsumerCalls:DWORD
EXTERN g_MoveAnimBridgeClobberMxcsr:DWORD

; The C++ fixture statically checks this shared snapshot layout.
SNAP_RAX EQU 0
SNAP_RCX EQU 8
SNAP_RDX EQU 16
SNAP_RBX EQU 24
SNAP_RBP EQU 32
SNAP_RSI EQU 40
SNAP_RDI EQU 48
SNAP_R8 EQU 56
SNAP_R9 EQU 64
SNAP_R10 EQU 72
SNAP_R11 EQU 80
SNAP_R12 EQU 88
SNAP_R13 EQU 96
SNAP_R14 EQU 104
SNAP_R15 EQU 112
SNAP_FLAGS EQU 120
SNAP_FX EQU 128

CAPTURE MACRO destination
    mov qword ptr [destination+SNAP_RAX],rax
    mov qword ptr [destination+SNAP_RCX],rcx
    mov qword ptr [destination+SNAP_RDX],rdx
    mov qword ptr [destination+SNAP_RBX],rbx
    mov qword ptr [destination+SNAP_RBP],rbp
    mov qword ptr [destination+SNAP_RSI],rsi
    mov qword ptr [destination+SNAP_RDI],rdi
    mov qword ptr [destination+SNAP_R8],r8
    mov qword ptr [destination+SNAP_R9],r9
    mov qword ptr [destination+SNAP_R10],r10
    mov qword ptr [destination+SNAP_R11],r11
    mov qword ptr [destination+SNAP_R12],r12
    mov qword ptr [destination+SNAP_R13],r13
    mov qword ptr [destination+SNAP_R14],r14
    mov qword ptr [destination+SNAP_R15],r15
    pushfq
    pop qword ptr [destination+SNAP_FLAGS]
    fxsave64 [destination+SNAP_FX]
ENDM

.code
ExecuteMoveAnimHookFixture PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp,228h
    .allocstack 228h
    fxsave64 [rsp+20h]
    .savexmm128 xmm6,120h
    .savexmm128 xmm7,130h
    .savexmm128 xmm8,140h
    .savexmm128 xmm9,150h
    .savexmm128 xmm10,160h
    .savexmm128 xmm11,170h
    .savexmm128 xmm12,180h
    .savexmm128 xmm13,190h
    .savexmm128 xmm14,1A0h
    .savexmm128 xmm15,1B0h
    .endprolog
    mov qword ptr [g_MoveAnimBridgeBeforeRsp],rsp
    fxrstor64 [g_MoveAnimBridgeSeed+SNAP_FX]
    push qword ptr [g_MoveAnimBridgeSeed+SNAP_FLAGS]
    popfq
    mov rax,qword ptr [g_MoveAnimBridgeSeed+SNAP_RAX]
    mov rcx,qword ptr [g_MoveAnimBridgeSeed+SNAP_RCX]
    mov rdx,qword ptr [g_MoveAnimBridgeSeed+SNAP_RDX]
    mov rbx,qword ptr [g_MoveAnimBridgeSeed+SNAP_RBX]
    mov rbp,qword ptr [g_MoveAnimBridgeSeed+SNAP_RBP]
    mov rsi,qword ptr [g_MoveAnimBridgeSeed+SNAP_RSI]
    mov rdi,qword ptr [g_MoveAnimBridgeSeed+SNAP_RDI]
    mov r8,qword ptr [g_MoveAnimBridgeSeed+SNAP_R8]
    mov r9,qword ptr [g_MoveAnimBridgeSeed+SNAP_R9]
    mov r10,qword ptr [g_MoveAnimBridgeSeed+SNAP_R10]
    mov r11,qword ptr [g_MoveAnimBridgeSeed+SNAP_R11]
    mov r12,qword ptr [g_MoveAnimBridgeSeed+SNAP_R12]
    mov r13,qword ptr [g_MoveAnimBridgeSeed+SNAP_R13]
    mov r14,qword ptr [g_MoveAnimBridgeSeed+SNAP_R14]
    mov r15,qword ptr [g_MoveAnimBridgeSeed+SNAP_R15]
    ; The indirect call leaves every seeded register intact on site entry.
    call qword ptr [g_MoveAnimBridgeTarget]
    CAPTURE g_MoveAnimBridgeAfter
    mov qword ptr [g_MoveAnimBridgeAfterRsp],rsp
    fxrstor64 [rsp+20h]
    add rsp,228h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
ExecuteMoveAnimHookFixture ENDP

RecordMoveAnimConsumerEntry PROC
    ; Capture before doing anything that could hide argument/register damage.
    CAPTURE g_MoveAnimBridgeConsumer
    mov qword ptr [g_MoveAnimBridgeConsumerRsp],rsp
    ; A normal original consumer owns all four caller-provided home slots.
    mov qword ptr [rsp+8],rcx
    mov qword ptr [rsp+10h],rdx
    mov qword ptr [rsp+18h],r8
    mov qword ptr [rsp+20h],r9
    inc dword ptr [g_MoveAnimBridgeConsumerCalls]
    mov eax,12345678h
    ret
RecordMoveAnimConsumerEntry ENDP

ClobberMoveAnimHookVolatiles PROC
    ; A conforming callback may clobber all these registers and MXCSR status.
    ; The bridge must restore them because the replaced instruction did not.
    mov rax,0BAD0000000000001h
    mov rcx,0BAD0000000000002h
    mov rdx,0BAD0000000000003h
    mov r8,0BAD0000000000008h
    mov r9,0BAD0000000000009h
    mov r10,0BAD000000000000Ah
    mov r11,0BAD000000000000Bh
    pxor xmm0,xmm0
    pxor xmm1,xmm1
    pxor xmm2,xmm2
    pxor xmm3,xmm3
    pxor xmm4,xmm4
    pxor xmm5,xmm5
    ldmxcsr dword ptr [g_MoveAnimBridgeClobberMxcsr]
    ret
ClobberMoveAnimHookVolatiles ENDP
END
