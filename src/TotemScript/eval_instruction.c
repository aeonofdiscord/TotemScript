//
//  eval_instruction.c
//  TotemScript
//
//  Created by Timothy Smale on 05/06/2016
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <TotemScript/exec.h>
#include <string.h>

totemEvalStatus totemInstruction_SetRegister(totemInstruction *instruction, totemOperandXUnsigned index, totemOperandType scope, uint32_t start)
{
    if (index > TOTEM_MAX_LOCAL_REGISTERS)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
#if TOTEM_VMOPT_GLOBAL_OPERANDS
    uint32_t reg = scope;
    if (scope > 1)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    TOTEM_SETBITS_OFFSET(reg, index, 1);
    TOTEM_SETBITS_OFFSET(*instruction, reg, start);
#else
    TOTEM_SETBITS_OFFSET(*instruction, index, start);
#endif
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetUnsignedValue(totemInstruction *ins, uint32_t val, uint32_t min, uint32_t max, uint32_t start)
{
    if (val > max || val < min)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    TOTEM_SETBITS_OFFSET(*ins, val, start);
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetSignedValue(totemInstruction *instruction, int32_t value, int32_t min, int32_t max, uint32_t start, uint32_t numBits)
{
    if (value < min || value > max)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    uint32_t mask = 0;
    uint32_t isNegative = value < 0;
    uint32_t unsignedValue = *((uint32_t*)(&value));
    
    if (isNegative)
    {
        unsignedValue = ~unsignedValue;
        unsignedValue++;
    }
    
    // value
    TOTEM_SETBITS(mask, (unsignedValue & TOTEM_BITMASK(uint32_t, 0, numBits - 1)));
    
    // signed bit
    TOTEM_SETBITS_OFFSET(mask, isNegative, numBits - 1);
    
    // add to instruction
    TOTEM_SETBITS_OFFSET(*instruction, mask, start);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetRegisterA(totemInstruction *instruction, totemOperandXUnsigned index, totemOperandType scope)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_A);
    return status;
}

totemEvalStatus totemInstruction_SetRegisterB(totemInstruction *instruction, totemOperandXUnsigned index, totemOperandType scope)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_B);
    return status;
}

totemEvalStatus totemInstruction_SetRegisterC(totemInstruction *instruction, totemOperandXUnsigned index, totemOperandType scope)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_C);
    return status;
}

totemEvalStatus totemInstruction_SetOp(totemInstruction *instruction, totemOperationType op)
{
    totemEvalStatus status = totemInstruction_SetUnsignedValue(
                                                               instruction,
                                                               op,
                                                               TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_Op),
                                                               TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_Op),
                                                               totemInstructionStart_Op);
    
    totem_assert(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_OP(*instruction) == op);
    return status;
}

totemEvalStatus totemInstruction_SetBxUnsigned(totemInstruction *instruction, totemOperandXUnsigned bx)
{
    totemEvalStatus status = totemInstruction_SetUnsignedValue(
                                                               instruction,
                                                               bx,
                                                               TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_Bx),
                                                               TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_Bx),
                                                               totemInstructionStart_B);
    
    totem_assert(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_BX_UNSIGNED(*instruction) == bx);
    return status;
}

totemEvalStatus totemInstruction_SetBxSigned(totemInstruction *instruction, totemOperandXSigned bx)
{
    totemEvalStatus status = totemInstruction_SetSignedValue(
                                                             instruction,
                                                             bx,
                                                             TOTEM_MINVAL_SIGNED(int32_t, totemInstructionSize_Bx),
                                                             TOTEM_MAXVAL_SIGNED(int32_t, totemInstructionSize_Bx),
                                                             totemInstructionStart_B,
                                                             totemInstructionSize_Bx);
    
    totem_assert(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_BX_SIGNED(*instruction) == bx);
    return status;
}

totemEvalStatus totemInstruction_SetAxSigned(totemInstruction *instruction, totemOperandXSigned ax)
{
    totemEvalStatus status = totemInstruction_SetSignedValue(
                                                             instruction,
                                                             ax,
                                                             TOTEM_MINVAL_SIGNED(int32_t, totemInstructionSize_Ax),
                                                             TOTEM_MAXVAL_SIGNED(int32_t, totemInstructionSize_Ax),
                                                             totemInstructionStart_A,
                                                             totemInstructionSize_Ax);
    
    totem_assert(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_AX_SIGNED(*instruction) == ax);
    return status;
}

totemEvalStatus totemInstruction_SetAxUnsigned(totemInstruction *instruction, totemOperandXUnsigned ax)
{
    totemEvalStatus status = totemInstruction_SetUnsignedValue(
                                                               instruction,
                                                               ax,
                                                               TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_Ax),
                                                               TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_Ax),
                                                               totemInstructionStart_A);
    
    totem_assert(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_AX_UNSIGNED(*instruction) == ax);
    return status;
}

totemEvalStatus totemInstruction_SetCxUnsigned(totemInstruction *instruction, totemOperandXUnsigned cx)
{
    totemEvalStatus status = totemInstruction_SetUnsignedValue(
                                                               instruction,
                                                               cx,
                                                               TOTEM_MINVAL_UNSIGNED(uint32_t, totemInstructionSize_Cx),
                                                               TOTEM_MAXVAL_UNSIGNED(uint32_t, totemInstructionSize_Cx),
                                                               totemInstructionStart_C);
    
    totem_assert(status != totemEvalStatus_Success || TOTEM_INSTRUCTION_GET_CX_UNSIGNED(*instruction) == cx);
    return status;
}

totemEvalStatus totemBuildPrototype_AllocInstruction(totemBuildPrototype *build, totemInstruction **instructionOut)
{
    *instructionOut = totemMemoryBuffer_Secure(&build->Instructions, 1);
    if (*instructionOut)
    {
        return totemEvalStatus_Success;
    }
    
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemBuildPrototype_SecureDst(totemBuildPrototype *build, totemOperationType op, totemOperandRegisterPrototype *a, totemOperandRegisterPrototype *aSrc)
{
    switch (op)
    {
        case totemOperationType_ConditionalGoto:
        case totemOperationType_FunctionArg:
        case totemOperationType_Return:
        case totemOperationType_ComplexSet:
        case totemOperationType_PreInvoke:
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, a));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, a, aSrc->RegisterIndex, totemOperationType_MoveToLocal));
            break;
            
        case totemOperationType_MoveToLocal:
        case totemOperationType_MoveToGlobal:
            break;
            
        default:
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, a));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_RelinquishDst(totemBuildPrototype *build, totemOperationType op, totemOperandRegisterPrototype *a, totemOperandRegisterPrototype *aSrc)
{
    switch (op)
    {
        case totemOperationType_ConditionalGoto:
        case totemOperationType_FunctionArg:
        case totemOperationType_Return:
        case totemOperationType_PreInvoke:
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, a));
            break;
            
        case totemOperationType_MoveToLocal:
        case totemOperationType_MoveToGlobal:
            break;
            
        default:
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, a, aSrc->RegisterIndex, totemOperationType_MoveToGlobal));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, a));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAxxInstructionSigned(totemBuildPrototype *build, totemOperandXSigned ax, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetAxSigned(instruction, ax));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAxxInstructionUnsigned(totemBuildPrototype *build, totemOperandXUnsigned ax, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetAxUnsigned(instruction, ax));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbxInstructionSigned(totemBuildPrototype *build, totemOperandRegisterPrototype *aSrc, totemOperandXSigned bx, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    
#if TOTEM_EVALOPT_GLOBAL_CACHE || TOTEM_VMOPT_GLOBAL_OPERANDS
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, aSrc->RegisterIndex, aSrc->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxSigned(instruction, bx));
#else
    totemOperandRegisterPrototype a;
    
    if (aSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_SecureDst(build, operationType, &a, aSrc));
    }
    else
    {
        memcpy(&a, aSrc, sizeof(totemOperandRegisterPrototype));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a.RegisterIndex, a.RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxSigned(instruction, bx));
    
    if (aSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RelinquishDst(build, operationType, &a, aSrc));
    }
#endif
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbxInstructionUnsigned(totemBuildPrototype *build, totemOperandRegisterPrototype *aSrc, totemOperandXUnsigned bx, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    
#if TOTEM_EVALOPT_GLOBAL_CACHE || TOTEM_VMOPT_GLOBAL_OPERANDS
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, aSrc->RegisterIndex, aSrc->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxUnsigned(instruction, bx));
#else
    totemOperandRegisterPrototype a;
    
    if (aSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_SecureDst(build, operationType, &a, aSrc));
    }
    else
    {
        memcpy(&a, aSrc, sizeof(totemOperandRegisterPrototype));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a.RegisterIndex, a.RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxUnsigned(instruction, bx));
    
    if (aSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RelinquishDst(build, operationType, &a, aSrc));
    }
    
#endif
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbcxInstructionUnsigned(totemBuildPrototype *build, totemOperandRegisterPrototype *aSrc, totemOperandRegisterPrototype *bSrc, totemOperandXUnsigned cx, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    
#if TOTEM_EVALOPT_GLOBAL_CACHE || TOTEM_VMOPT_GLOBAL_OPERANDS
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, aSrc->RegisterIndex, aSrc->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterB(instruction, bSrc->RegisterIndex, bSrc->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetCxUnsigned(instruction, cx));
#else
    totemOperandRegisterPrototype a, b;
    
    if (aSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_SecureDst(build, operationType, &a, aSrc));
    }
    else
    {
        memcpy(&a, aSrc, sizeof(totemOperandRegisterPrototype));
    }
    
    if (bSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &b));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &b, bSrc->RegisterIndex, totemOperationType_MoveToLocal));
    }
    else
    {
        memcpy(&b, bSrc, sizeof(totemOperandRegisterPrototype));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a.RegisterIndex, a.RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterB(instruction, b.RegisterIndex, b.RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetCxUnsigned(instruction, cx));
    
    if (aSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RelinquishDst(build, operationType, &a, aSrc));
    }
    
    totemRegisterListPrototype *localScope = totemBuildPrototype_GetLocalScope(build);
    if (bSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &b));
    }
    
#endif
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbcInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *aSrc, totemOperandRegisterPrototype *bSrc, totemOperandRegisterPrototype *cSrc, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    
#if TOTEM_EVALOPT_GLOBAL_CACHE || TOTEM_VMOPT_GLOBAL_OPERANDS
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, aSrc->RegisterIndex, aSrc->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterB(instruction, bSrc->RegisterIndex, bSrc->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterC(instruction, cSrc->RegisterIndex, cSrc->RegisterScopeType));
#else
    totemOperandRegisterPrototype a, b, c;
    
    if (aSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_SecureDst(build, operationType, &a, aSrc));
    }
    else
    {
        memcpy(&a, aSrc, sizeof(totemOperandRegisterPrototype));
    }
    
    if (bSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &b));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &b, bSrc->RegisterIndex, totemOperationType_MoveToLocal));
    }
    else
    {
        memcpy(&b, bSrc, sizeof(totemOperandRegisterPrototype));
    }
    
    if (cSrc->RegisterScopeType == totemOperandType_GlobalRegister)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AddRegister(build, totemOperandType_LocalRegister, &c));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &c, cSrc->RegisterIndex, totemOperationType_MoveToLocal));
    }
    else
    {
        memcpy(&c, cSrc, sizeof(totemOperandRegisterPrototype));
    }
    
    if (aSrc->RegisterScopeType == totemOperandType_GlobalRegister && operationType == totemOperationType_Move)
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RelinquishDst(build, operationType, &b, aSrc));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &a));
        
        if (bSrc->RegisterScopeType == totemOperandType_GlobalRegister)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &b));
        }
        
        if (cSrc->RegisterScopeType == totemOperandType_GlobalRegister)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &c));
        }
    }
    else
    {
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
        TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
        
        TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a.RegisterIndex, a.RegisterScopeType));
        TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterB(instruction, b.RegisterIndex, b.RegisterScopeType));
        TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterC(instruction, c.RegisterIndex, c.RegisterScopeType));
        
        if (aSrc->RegisterScopeType == totemOperandType_GlobalRegister)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RelinquishDst(build, operationType, &a, aSrc));
        }
        
        if (bSrc->RegisterScopeType == totemOperandType_GlobalRegister)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &b));
        }
        
        if (cSrc->RegisterScopeType == totemOperandType_GlobalRegister)
        {
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_RecycleRegister(build, &c));
        }
    }
    
#endif
    
    return totemEvalStatus_Success;
}