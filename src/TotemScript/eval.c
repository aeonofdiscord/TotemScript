//
//  eval.c
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <TotemScript/base.h>
#include <string.h>

#define TOTEM_EVAL_CHECKRETURN(exp) { totemEvalStatus status = exp; if(status != totemEvalStatus_Success) return status; }
#define TOTEM_EVAL_SETINSTRUCTIONBITS(ins, val, min, max, start) \
    if ((val) > (max) || (val) < (min)) \
    { \
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow); \
    } \
    TOTEM_SETBITS_OFFSET(ins, val, start);

typedef enum
{
    totemEvalLoopFlag_None = 0,
    totemEvalLoopFlag_StartingCondition = 1
}
totemEvalLoopFlag;

typedef struct totemEvalLoopPrototype
{
    size_t LoopBeginIndex;
    size_t ConditionFailIndex;
    size_t ConditionIndex;
    size_t BreakIndex;
    struct totemEvalLoopPrototype *Next;
    totemEvalLoopFlag Flags;
}
totemEvalLoopPrototype;

void totemEvalLoopPrototype_Begin(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    loop->Next = NULL;
    loop->ConditionFailIndex = 0;
    loop->ConditionIndex = 0;
    loop->Flags = totemEvalLoopFlag_None;
    loop->LoopBeginIndex = 0;
    loop->BreakIndex = 0;
}

void totemEvalLoopPrototype_SetStartPosition(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    loop->LoopBeginIndex = totemMemoryBuffer_GetNumObjects(&build->Instructions);
}

totemEvalStatus totemEvalLoopPrototype_SetCondition(totemEvalLoopPrototype *loop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemExpressionPrototype *condition)
{
    totemOperandRegisterPrototype conditionOp;
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(condition, build, scope, globals, &conditionOp, totemEvalVariableFlag_None));
    
    loop->ConditionIndex = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionSigned(build, &conditionOp, 0, totemOperationType_ConditionalGoto));
    TOTEM_SETBITS(loop->Flags, totemEvalLoopFlag_StartingCondition);
    
    return totemEvalStatus_Success;
}

void totemEvalLoopPrototype_SetConditionFailPosition(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    loop->ConditionFailIndex = totemMemoryBuffer_GetNumObjects(&build->Instructions);
}

totemEvalStatus totemEvalLoopPrototype_AddBreak(totemEvalLoopPrototype *child, totemBuildPrototype *build)
{
    // add goto to edit later
    child->BreakIndex = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAxxInstructionSigned(build, 0, totemOperationType_Goto));
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalLoopPrototype_AddBreakChild(totemEvalLoopPrototype *parent, totemEvalLoopPrototype *child, totemBuildPrototype *build)
{
    if(parent->Next != NULL)
    {
        child->Next = parent->Next;
    }
    
    parent->Next = child;
    
    return totemEvalLoopPrototype_AddBreak(child, build);
}

totemEvalStatus totemEvalLoopPrototype_Loop(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    size_t numInstructions = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    totemOperandXSigned resetLoopOffset = (totemOperandXSigned)loop->LoopBeginIndex - (totemOperandXSigned)numInstructions;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAxxInstructionSigned(build, resetLoopOffset, totemOperationType_Goto));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalLoopPrototype_EndChild(totemEvalLoopPrototype *loop, totemEvalLoopPrototype *child, totemBuildPrototype *build)
{
    size_t numInstructions = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    
    if(TOTEM_GETBITS(child->Flags, totemEvalLoopFlag_StartingCondition))
    {
        if(child->ConditionFailIndex != numInstructions - 1)
        {
            totemInstruction *gotoInstruction = totemMemoryBuffer_Get(&build->Instructions, child->ConditionIndex);
            totemOperandXUnsigned offset = (totemOperandXUnsigned)child->ConditionFailIndex - (totemOperandXUnsigned)child->ConditionIndex;
            TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxUnsigned(gotoInstruction, offset));
        }
    }
    
    if(child->BreakIndex != 0)
    {
        totemInstruction *gotoInstruction = totemMemoryBuffer_Get(&build->Instructions, child->BreakIndex);
        totemOperandXUnsigned offset = (totemOperandXUnsigned)numInstructions - (totemOperandXUnsigned)child->BreakIndex;
        TOTEM_EVAL_CHECKRETURN(totemInstruction_SetAxSigned(gotoInstruction, offset));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalLoopPrototype_End(totemEvalLoopPrototype *loop, totemBuildPrototype *build)
{
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_EndChild(loop, loop, build));
    
    for(totemEvalLoopPrototype *child = loop->Next; child != NULL; child = child->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_EndChild(loop, child, build));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemEvalStatus_Break(totemEvalStatus status)
{
    return status;
}

totemEvalStatus totemInstruction_SetRegister(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope, uint32_t start)
{
    uint32_t reg = scope;
    if(scope > 1)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    TOTEM_SETBITS_OFFSET(reg, index, 1);
    TOTEM_SETBITS_OFFSET(*instruction, reg, start);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetSignedValue(totemInstruction *instruction, int32_t value, uint32_t start, uint32_t numBits)
{
    uint32_t mask = 0;
    uint32_t isNegative = value < 0;
    uint32_t unsignedValue = *((uint32_t*)(&value));
    
    if(isNegative)
    {
        unsignedValue = ~unsignedValue;
        unsignedValue++;
    }
    
    // value
    TOTEM_SETBITS(mask, (unsignedValue & TOTEM_BITMASK(0, numBits - 1)));
    
    // signed bit
    TOTEM_SETBITS_OFFSET(mask, isNegative, numBits - 1);
    
    // add to instruction
    TOTEM_SETBITS_OFFSET(*instruction, mask, start);
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetRegisterA(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_A);
    return status;
}

totemEvalStatus totemInstruction_SetRegisterB(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope)
{
    totemEvalStatus status = totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_B);
    return status;
}

totemEvalStatus totemInstruction_SetRegisterC(totemInstruction *instruction, totemRegisterIndex index, totemRegisterScopeType scope)
{
    totemEvalStatus status =  totemInstruction_SetRegister(instruction, index, scope, totemInstructionStart_C);
    return status;
}

totemEvalStatus totemInstruction_SetOp(totemInstruction *instruction, totemOperationType op)
{
    TOTEM_EVAL_SETINSTRUCTIONBITS(*instruction, op, TOTEM_MINVAL_UNSIGNED(totemInstructionSize_Op), TOTEM_MAXVAL_UNSIGNED(totemInstructionSize_Op), totemInstructionStart_Op);
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetBxUnsigned(totemInstruction *instruction, totemOperandXUnsigned bx)
{
    TOTEM_EVAL_SETINSTRUCTIONBITS(*instruction, bx, TOTEM_MINVAL_UNSIGNED(totemInstructionSize_Bx), TOTEM_MAXVAL_UNSIGNED(totemInstructionSize_Bx), totemInstructionStart_B);
    return totemEvalStatus_Success;
}

totemEvalStatus totemInstruction_SetBxSigned(totemInstruction *instruction, totemOperandXSigned bx)
{
    totemOperandXSigned min = TOTEM_MINVAL_SIGNED(totemInstructionSize_Bx);
    totemOperandXSigned max = TOTEM_MAXVAL_SIGNED(totemInstructionSize_Bx);
    
    if(bx < min || bx > max)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    return totemInstruction_SetSignedValue(instruction, bx, totemInstructionStart_B, totemInstructionSize_Bx);
}

totemEvalStatus totemInstruction_SetAxSigned(totemInstruction *instruction, totemOperandXSigned ax)
{
    totemOperandXSigned min = TOTEM_MINVAL_SIGNED(totemInstructionSize_Ax);
    totemOperandXSigned max = TOTEM_MAXVAL_SIGNED(totemInstructionSize_Ax);
    
    if(ax < min || ax > max)
    {
        return totemEvalStatus_Break(totemEvalStatus_InstructionOverflow);
    }
    
    return totemInstruction_SetSignedValue(instruction, ax, totemInstructionStart_A, totemInstructionSize_Ax);
}

totemEvalStatus totemInstruction_SetAxUnsigned(totemInstruction *instruction, totemOperandXUnsigned ax)
{
    TOTEM_EVAL_SETINSTRUCTIONBITS(*instruction, ax, TOTEM_MINVAL_SIGNED(totemInstructionSize_Ax), TOTEM_MAXVAL_SIGNED(totemInstructionSize_Ax), totemInstructionStart_A);
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand)
{
    size_t index = totemMemoryBuffer_GetNumObjects(&list->Registers);
    
    if(index + 1 >= TOTEM_MAX_REGISTERS)
    {
        return totemEvalStatus_Break(totemEvalStatus_TooManyRegisters);
    }
    
    if(totemMemoryBuffer_Secure(&list->Registers, 1) != totemBool_True)
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }

    totemRegisterPrototype *reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, index);
    
    reg->Value.Data = 0;
    reg->DataType = totemDataType_Null;
    reg->Flags = totemRegisterPrototypeFlag_None;
    
    operand->RegisterIndex = index;
    operand->RegisterScopeType = list->Scope;
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_SetRegisterFlags(totemRegisterListPrototype *list, totemRegisterIndex index, totemRegisterPrototypeFlag flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg)
    {
        return totemBool_False;
    }
    
    reg->Flags |= flags;
    return totemBool_True;
}

totemBool totemRegisterListPrototype_GetRegisterFlags(totemRegisterListPrototype *list, totemRegisterIndex index, totemRegisterPrototypeFlag *flags)
{
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, index);
    if(!reg)
    {
        return totemBool_False;
    }
    
    *flags = reg->Flags;
    return totemBool_True;
}

totemEvalStatus totemRegisterListPrototype_AddNull(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand)
{
    if(list->HasNull)
    {
        operand->RegisterIndex = list->NullIndex;
        operand->RegisterScopeType = list->Scope;
        return totemEvalStatus_Success;
    }
    
    totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
    if(status != totemEvalStatus_Success)
    {
        return status;
    }
    
    totemRegisterPrototype *reg = totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
    reg->DataType = totemDataType_Null;
    list->HasNull = totemBool_True;
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddStringConstant(totemRegisterListPrototype *list, totemString *str, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Strings, str->Value, str->Length);
    
    totemRegisterPrototype *reg = NULL;
    if(searchResult != NULL)
    {
        operand->RegisterIndex = searchResult->Value;
        operand->RegisterScopeType = list->Scope;
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if(status != totemEvalStatus_Success)
        {
            return status;
        }
        
        reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
        reg->DataType = totemDataType_String;
        reg->Value.String.Index = (uint32_t)list->StringData.Length;
        reg->Value.String.Length = str->Length;
        
        if(!totemHashMap_Insert(&list->Strings, str->Value, str->Length, operand->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        if(!totemMemoryBuffer_Secure(&list->StringData, str->Length))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        memcpy(totemMemoryBuffer_Get(&list->StringData, reg->Value.String.Index), str->Value, str->Length);
    }
    
    return totemEvalStatus_Success;
}

totemBool totemRegisterListPrototype_GetVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Variables, name->Value, name->Length);
    if(searchResult != NULL)
    {
        operand->RegisterIndex = searchResult->Value;
        operand->RegisterScopeType = list->Scope;
        return totemBool_True;
    }
    
    return totemBool_False;
}

totemEvalStatus totemRegisterListPrototype_AddVariable(totemRegisterListPrototype *list, totemString *name, totemOperandRegisterPrototype *prototype)
{
    if(!totemRegisterListPrototype_GetVariable(list, name, prototype))
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, prototype);
        if(status != totemEvalStatus_Success)
        {
            return status;
        }
        
        if(!totemHashMap_Insert(&list->Variables, name->Value, name->Length, prototype->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemString *number, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Numbers, number->Value, number->Length);
    if(searchResult != NULL)
    {
        operand->RegisterIndex = searchResult->Value;
        operand->RegisterScopeType = list->Scope;
    }
    else
    {
        totemEvalStatus status = totemRegisterListPrototype_AddRegister(list, operand);
        if(status != totemEvalStatus_Success)
        {
            return status;
        }
        
        if(!totemHashMap_Insert(&list->Numbers, number->Value, number->Length, operand->RegisterIndex))
        {
            return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
        }
        
        totemRegisterPrototype *reg = (totemRegisterPrototype*)totemMemoryBuffer_Get(&list->Registers, operand->RegisterIndex);
        
        if(strnstr(number->Value, ".", number->Length) != NULL)
        {
            reg->Value.Float = atof(number->Value);
            reg->DataType = totemDataType_Float;
        }
        else
        {
            reg->Value.Int = atoi(number->Value);
            reg->DataType = totemDataType_Int;
        }
    }
    
    return totemEvalStatus_Success;
}

void totemRegisterListPrototype_Reset(totemRegisterListPrototype *list, totemRegisterScopeType scope)
{
    totemHashMap_Reset(&list->Numbers);
    totemHashMap_Reset(&list->Strings);
    totemHashMap_Reset(&list->Variables);
    totemMemoryBuffer_Reset(&list->Registers, sizeof(totemRegisterPrototype));
    totemMemoryBuffer_Reset(&list->StringData, sizeof(char));
    list->Scope = scope;
    list->NullIndex = 0;
    list->HasNull = totemBool_False;
}

void totemBuildPrototype_Init(totemBuildPrototype *build, totemRuntime *runtime)
{
    build->Functions.ObjectSize = sizeof(totemFunction);
    build->Instructions.ObjectSize = sizeof(totemInstruction);
    totemRegisterListPrototype_Reset(&build->GlobalRegisters, totemRegisterScopeType_Global);
    build->Runtime = runtime;
}

/*
    1. Build instruction prototypes for each function (treat global statements as a function called when script is first loaded into environment)
    2. Allocate registers for all variables
    3. Resolve function calls (how do we store functions?? one big lookup table, index -> index of script instruction block & instruction offset)
    4. Create actual instructions

  - save global values into global register / stack
  - global instructions - placed into "init" function & ran once after build
 
  - foreach function
    - save entry point in table (instruction offset)
    - resolve constants into global register / stack
    - assign variables to either local or global register
    - resolve function calls
    - build instructions
 */

totemEvalStatus totemBuildPrototype_Eval(totemBuildPrototype *build, totemParseTree *prototype)
{
    totemFunction *globalFunction;

    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &globalFunction));
    globalFunction->InstructionsStart = 0;
    totemString_FromLiteral(&globalFunction->Name, "__initGlobals");
    
    // globals
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_Statement:
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(block->Statement, build, &build->GlobalRegisters, &build->GlobalRegisters));
                break;
                
            default:
                break;
        }
    }
    
    globalFunction->RegistersNeeded = totemMemoryBuffer_GetNumObjects(&build->GlobalRegisters.Registers);
    totemBuildPrototype_EvalReturn(build, NULL);
    
    // all other functions
    for(totemBlockPrototype *block = prototype->FirstBlock; block != NULL; block = block->Next)
    {
        switch(block->Type)
        {
            case totemBlockType_FunctionDeclaration:
                TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(block->FuncDec, build, &build->GlobalRegisters));
                break;
                
            default:
                break;
        }
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemFunction **functionOut)
{
    size_t index = totemMemoryBuffer_GetNumObjects(&build->Functions);
    if(totemMemoryBuffer_Secure(&build->Functions, 1) == totemBool_True)
    {
        *functionOut = totemMemoryBuffer_Get(&build->Functions, index);
        return totemEvalStatus_Success;
    }
    
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemBuildPrototype_AllocInstruction(totemBuildPrototype *build, totemInstruction **instructionOut)
{
    size_t index = totemMemoryBuffer_GetNumObjects(&build->Instructions);
    if(totemMemoryBuffer_Secure(&build->Instructions, 1) == totemBool_True)
    {
        *instructionOut = totemMemoryBuffer_Get(&build->Instructions, index);
        return totemEvalStatus_Success;
    }
    
    return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
}

totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemRegisterListPrototype *globals)
{
    totemRegisterListPrototype localRegisters;
    memset(&localRegisters, 0, sizeof(totemRegisterListPrototype));
    totemRegisterListPrototype_Reset(&localRegisters, totemRegisterScopeType_Local);
    
    // TODO: Move function linking to runtime to better separate runtime from evaluation
    size_t addr = 0;
    if(totemRuntime_GetNativeFunctionAddress(build->Runtime, function->Identifier, &addr))
    {
        build->ErrorContext = function;
        return totemEvalStatus_NativeFunctionAlreadyDefined;
    }
    
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&build->Functions); i++)
    {
        totemFunction *existingFunc = totemMemoryBuffer_Get(&build->Functions, i);
        if(existingFunc != NULL && totemString_Equals(&existingFunc->Name, function->Identifier))
        {
            build->ErrorContext = function;
            return totemEvalStatus_ScriptFunctionAlreadyDefined;
        }
    }
    
    size_t functionIndex = totemMemoryBuffer_GetNumObjects(&build->Functions);
    totemFunction *functionPrototype;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &functionPrototype));
    memcpy(&functionPrototype->Name, function->Identifier, sizeof(totemString));
    
    if(!totemHashMap_Insert(&build->FunctionLookup, function->Identifier->Value, function->Identifier->Length, functionIndex))
    {
        return totemEvalStatus_Break(totemEvalStatus_OutOfMemory);
    }
    
    functionPrototype->InstructionsStart = totemMemoryBuffer_GetNumObjects(&build->Instructions);

    // parameters
    for(totemVariablePrototype *parameter = function->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype dummy;
        TOTEM_EVAL_CHECKRETURN(totemVariablePrototype_Eval(parameter, &localRegisters, globals, &dummy, totemEvalVariableFlag_LocalOnly));
    }
    
    // loop through statements & create instructions
    for(totemStatementPrototype *statement = function->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, &localRegisters, globals));
    }
    
    functionPrototype->RegistersNeeded = totemMemoryBuffer_GetNumObjects(&localRegisters.Registers);
    
    // do we need an implicit return?
    totemInstruction *lastInstruction = totemMemoryBuffer_Get(&build->Instructions, totemMemoryBuffer_GetNumObjects(&build->Instructions) - 1);
    if(lastInstruction == NULL || TOTEM_INSTRUCTION_GET_OP(*lastInstruction) != totemOperationType_Return)
    {
        totemBuildPrototype_EvalReturn(build, NULL);
    }

    return totemEvalStatus_Success;
}

totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemOperandRegisterPrototype result;

    switch(statement->Type)
    {
        case totemStatementType_DoWhileLoop:
            TOTEM_EVAL_CHECKRETURN(totemDoWhileLoopPrototype_Eval(statement->DoWhileLoop, build, scope, globals));
            break;
            
        case totemStatementType_ForLoop:
            TOTEM_EVAL_CHECKRETURN(totemForLoopPrototype_Eval(statement->ForLoop, build, scope, globals));
            break;
            
        case totemStatementType_IfBlock:
            TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_Eval(statement->IfBlock, build, scope, globals));
            break;

        case totemStatementType_WhileLoop:
            TOTEM_EVAL_CHECKRETURN(totemWhileLoopPrototype_Eval(statement->WhileLoop, build, scope, globals));
            break;
            
        case totemStatementType_Simple:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Expression, build, scope, globals, &result, totemEvalVariableFlag_None));
            break;
            
        case totemStatementType_Return:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Return, build, scope, globals, &result, totemEvalVariableFlag_None));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalReturn(build, &result));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *value, totemEvalVariableFlag flags)
{
    totemOperandRegisterPrototype lValue;
    
    // evaluate lvalue result to register
    switch(expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_Eval(expression->LValueArgument, build, scope, globals, &lValue, flags));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->LValueExpression, build, scope, globals, &lValue, flags));
            break;
    }
    
    // const assignments can only happen to vars that are being declared const right here, right now
    // so if the lvalue is const and this isn't its declaration, and an assignment op is happening on it...throw an error
    totemBool performConstCheck = totemBool_False;
    
    switch(expression->PreUnaryOperator)
    {
        case totemPreUnaryOperatorType_Dec:
        case totemPreUnaryOperatorType_Inc:
            performConstCheck = totemBool_True;
            break;
            
        default:
            break;
    }
    
    switch(expression->PostUnaryOperator)
    {
        case totemPostUnaryOperatorType_Dec:
        case totemPostUnaryOperatorType_Inc:
            performConstCheck = totemBool_True;
            break;
            
        default:
            break;
    }
    
    switch(expression->BinaryOperator)
    {
        case totemBinaryOperatorType_Assign:
        case totemBinaryOperatorType_DivideAssign:
        case totemBinaryOperatorType_MinusAssign:
        case totemBinaryOperatorType_MultiplyAssign:
        case totemBinaryOperatorType_PlusAssign:
            performConstCheck = totemBool_True;
            break;
            
        default:
            break;
    }
    
    if(performConstCheck)
    {
        totemRegisterListPrototype *list = NULL;
        
        switch(lValue.RegisterScopeType)
        {
            case totemRegisterScopeType_Global:
                list = globals;
                break;
                
            case totemRegisterScopeType_Local:
                list = scope;
                break;
        }
        
        // if already assigned, and is const, throw an error
        totemRegisterPrototypeFlag flags;
        if(totemRegisterListPrototype_GetRegisterFlags(list, lValue.RegisterIndex, &flags))
        {
            if(TOTEM_GETBITS(flags, totemRegisterPrototypeFlag_IsConst | totemRegisterPrototypeFlag_IsAssigned) == (totemRegisterPrototypeFlag_IsConst | totemRegisterPrototypeFlag_IsAssigned))
            {
                return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
            }
        }
        
        // this lvalue is assigned
        totemRegisterListPrototype_SetRegisterFlags(list, lValue.RegisterIndex, totemRegisterPrototypeFlag_IsAssigned);
    }
    
    totemOperandRegisterPrototype preUnaryRegister, preUnaryLValue;
    totemString preUnaryNumber;
    totemString_FromLiteral(&preUnaryNumber, "0");
    
    switch(expression->PreUnaryOperator)
    {
        case totemPreUnaryOperatorType_Dec:
            totemString_FromLiteral(&preUnaryNumber, "1");
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Subtract));
            break;
            
        case totemPreUnaryOperatorType_Inc:
            totemString_FromLiteral(&preUnaryNumber, "1");
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Add));
            break;
            
        case totemPreUnaryOperatorType_LogicalNegate:
            totemString_FromLiteral(&preUnaryNumber, "0");
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &preUnaryLValue));
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &preUnaryRegister, totemOperationType_Equals));
            memcpy(&lValue, &preUnaryLValue, sizeof(totemOperandRegisterPrototype));
            // A = B == C(0)
            break;
            
        case totemPreUnaryOperatorType_Negative:
            totemString_FromLiteral(&preUnaryNumber, "-1");
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &preUnaryLValue));
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &preUnaryLValue, &lValue, &preUnaryRegister, totemOperationType_Multiply));
            memcpy(&lValue, &preUnaryLValue, sizeof(totemOperandRegisterPrototype));
            // A = B * -1
            break;
            
        case totemPreUnaryOperatorType_None:
            break;
    }
    
    totemOperandRegisterPrototype postUnaryRegister;
    totemString postUnaryNumber;
    totemString_FromLiteral(&postUnaryNumber, "0");
        
    switch(expression->PostUnaryOperator)
    {
        case totemPostUnaryOperatorType_Dec:
            totemString_FromLiteral(&postUnaryNumber, "1");
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &postUnaryNumber, &postUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Subtract));
            break;
            
        case totemPostUnaryOperatorType_Inc:
            totemString_FromLiteral(&postUnaryNumber, "1");
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &postUnaryNumber, &postUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &postUnaryRegister, totemOperationType_Add));
            break;
            
        case totemPostUnaryOperatorType_None:
            break;
    }
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        totemOperandRegisterPrototype rValue;
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, scope, globals, &rValue, flags));
        
        switch(expression->BinaryOperator)
        {
            case totemBinaryOperatorType_Plus:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_Add));
                break;
                
            case totemBinaryOperatorType_PlusAssign:
                
                
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Add));
                break;
                
            case totemBinaryOperatorType_Minus:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_Subtract));
                break;
                
            case totemBinaryOperatorType_MinusAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Subtract));
                break;
                
            case totemBinaryOperatorType_Multiply:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_Multiply));
                break;
                
            case totemBinaryOperatorType_MultiplyAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Multiply));
                break;
                
            case totemBinaryOperatorType_Divide:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_Divide));
                break;
                
            case totemBinaryOperatorType_DivideAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &lValue, &rValue, totemOperationType_Divide));
                break;
                                
            case totemBinaryOperatorType_Assign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, &lValue, &rValue, &rValue, totemOperationType_Move));
                break;
                
            case totemBinaryOperatorType_Equals:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_Equals));
                break;
                
            case totemBinaryOperatorType_MoreThan:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_MoreThan));
                break;
                
            case totemBinaryOperatorType_LessThan:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_LessThan));
                break;
                
            case totemBinaryOperatorType_MoreThanEquals:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_MoreThanEquals));
                break;
                
            case totemBinaryOperatorType_LessThanEquals:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_LessThanEquals));
                break;
                
            case totemBinaryOperatorType_LogicalAnd:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_LogicalAnd));
                break;
                
            case totemBinaryOperatorType_LogicalOr:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbcInstruction(build, value, &lValue, &rValue, totemOperationType_LogicalOr));
                break;
                
            case totemBinaryOperatorType_None:
                break;
        }
    }
    else
    {
        // no binary operation - result is lValue
        memcpy(value, &lValue, sizeof(totemOperandRegisterPrototype));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *value, totemEvalVariableFlag flags)
{
    // evaluate argument to register
    switch(argument->Type)
    {
        case totemArgumentType_Variable:
            build->ErrorContext = argument;
            totemEvalStatus status = totemVariablePrototype_Eval(argument->Variable, scope, globals, value, flags);
            if(status == totemEvalStatus_Success)
            {
                build->ErrorContext = NULL;
            }
            
            return status;
            
        case totemArgumentType_Null:
            return totemRegisterListPrototype_AddNull(globals, value);
            
        case totemArgumentType_String:
            return totemRegisterListPrototype_AddStringConstant(globals, &argument->String, value);
            
        case totemArgumentType_Number:
            return totemRegisterListPrototype_AddNumberConstant(globals, &argument->Number, value);
            
        case totemArgumentType_FunctionCall:
            return totemFunctionCallPrototype_Eval(argument->FunctionCall, scope, globals, build, value);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *index, totemEvalVariableFlag flags)
{
    totemEvalStatus result = totemEvalStatus_Success;
    totemBool justCreated = totemBool_False;
    
    if(!totemRegisterListPrototype_GetVariable(globals, &variable->Identifier, index))
    {
        if(TOTEM_GETBITS(flags, totemEvalVariableFlag_MustBeDefined))
        {
            if(!totemRegisterListPrototype_GetVariable(scope, &variable->Identifier, index))
            {
                return totemEvalStatus_Break(totemEvalStatus_VariableNotDefined);
            }
        }
        else
        {
            justCreated = totemBool_True;
            result = totemRegisterListPrototype_AddVariable(scope, &variable->Identifier, index);
        }
    }
    else if(TOTEM_GETBITS(flags, totemEvalVariableFlag_LocalOnly))
    {
        return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
    }
    
    if(variable->IsConst)
    {
        if(!justCreated)
        {
            // can't declare a var const if it already exists
            return totemEvalStatus_Break(totemEvalStatus_VariableAlreadyDefined);
        }
        else
        {
            switch(index->RegisterScopeType)
            {
                case totemRegisterScopeType_Global:
                    totemRegisterListPrototype_SetRegisterFlags(globals, index->RegisterIndex, totemRegisterPrototypeFlag_IsConst);
                    break;
                    
                case totemRegisterScopeType_Local:
                    totemRegisterListPrototype_SetRegisterFlags(scope, index->RegisterIndex, totemRegisterPrototypeFlag_IsConst);
                    break;
            }
        }
    }
    
    return result;
}

/*
 for
 ---
 _Init (loop)
 Initialization statement
 _SetLoopStartPosition
 _SetCondition
 statements
 afterthought
 _SetConditionFailPosition
 _End
 */
totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemOperandRegisterPrototype initialisation, afterThought;
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->Initialisation, build, scope, globals, &initialisation, totemEvalVariableFlag_None));
    
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, scope, globals, forLoop->Condition));
    
    for(totemStatementPrototype *statement = forLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->AfterThought, build, scope, globals, &afterThought, totemEvalVariableFlag_None));
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    return totemEvalStatus_Success;
}

/*
if
--
_Init (no loop)
_SetLoopStartPosition
_SetCondition
statements
_AddBreakChild
_SetConditionFailPosition
.. do again for each if...else...
_End
 */
totemEvalStatus totemIfBlockPrototype_EvalBlock(totemIfBlockPrototype *ifBlock, totemEvalLoopPrototype *loop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype_Begin(loop, build);
    totemEvalLoopPrototype_SetStartPosition(loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(loop, build, scope, globals, ifBlock->Expression));
    
    for(totemStatementPrototype *statement = ifBlock->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemIfBlockPrototype_EvalChild(totemEvalLoopPrototype *root, totemEvalLoopPrototype *parent, totemIfBlockPrototype *ifBlock, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype loop;
    TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalBlock(ifBlock, &loop, build, scope, globals));
    
    totemEvalLoopPrototype_AddBreakChild(parent, &loop, build);
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    
    switch(ifBlock->ElseType)
    {
        case totemIfElseBlockType_Else:
            for(totemStatementPrototype *statement = ifBlock->ElseBlock->StatementsStart; statement != NULL; statement = statement->Next)
            {
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
            }
            
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(root, build));
            break;
            
        case totemIfElseBlockType_ElseIf:
            TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalChild(root, parent, ifBlock->IfElseBlock, build, scope, globals));
            break;
            
        case totemIfElseBlockType_None:
            TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(root, build));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype loop;
    TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalBlock(ifBlock, &loop, build, scope, globals));
    if(ifBlock->IfElseBlock != NULL)
    {
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_AddBreak(&loop, build));
        totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
        TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_EvalChild(&loop, &loop, ifBlock->IfElseBlock, build, scope, globals));
    }
    else
    {
        totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
        TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    }

    return totemEvalStatus_Success;
}

/*
dowhile
-------
_Init (loop)
_SetLoopStartPosition
statements
_SetCondition
_SetConditionFailPosition
_End*/
totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    
    for(totemStatementPrototype *statement = doWhileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, scope, globals, doWhileLoop->Expression));
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
    
    return totemEvalStatus_Success;
}

/*
 while
 -----
 _Init (loop)
 _SetLoopStartPosition
 _SetCondition
 statements
 _SetConditionFailPosition
 _End*/
totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemEvalLoopPrototype loop;
    totemEvalLoopPrototype_Begin(&loop, build);
    totemEvalLoopPrototype_SetStartPosition(&loop, build);
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_SetCondition(&loop, build, scope, globals, whileLoop->Expression));
    
    for(totemStatementPrototype *statement = whileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_Loop(&loop, build));
    totemEvalLoopPrototype_SetConditionFailPosition(&loop, build);
    TOTEM_EVAL_CHECKRETURN(totemEvalLoopPrototype_End(&loop, build));
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

totemEvalStatus totemBuildPrototype_EvalAbxInstructionSigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXSigned bx, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a->RegisterIndex, a->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxSigned(instruction, bx));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbxInstructionUnsigned(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandXUnsigned bx, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a->RegisterIndex, a->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetBxUnsigned(instruction, bx));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbcInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandRegisterPrototype *b, totemOperandRegisterPrototype *c, totemOperationType operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetOp(instruction, operationType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterA(instruction, a->RegisterIndex, a->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterB(instruction, b->RegisterIndex, b->RegisterScopeType));
    TOTEM_EVAL_CHECKRETURN(totemInstruction_SetRegisterC(instruction, c->RegisterIndex, c->RegisterScopeType));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalReturn(totemBuildPrototype *build, totemOperandRegisterPrototype *dest)
{
    totemOperandXUnsigned option = totemReturnOption_Register;
    totemOperandRegisterPrototype def;
    def.RegisterScopeType = totemRegisterScopeType_Local;
    def.RegisterIndex = 0;
    
    // implicit return
    if(dest == NULL)
    {
        dest = &def;
        option = totemReturnOption_Implicit;
    }
    
    return totemBuildPrototype_EvalAbxInstructionUnsigned(build, dest, option, totemOperationType_Return);
}

totemEvalStatus totemFunctionCallPrototype_Eval(totemFunctionCallPrototype *functionCall, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemBuildPrototype *build, totemOperandRegisterPrototype *result)
{
    size_t address;
    totemOperationType opType;

    // check for native function
    if(totemRuntime_GetNativeFunctionAddress(build->Runtime, &functionCall->Identifier, &address))
    {
        opType = totemOperationType_NativeFunction;
    }
    else
    {
        totemHashMapEntry *searchResult = totemHashMap_Find(&build->FunctionLookup, functionCall->Identifier.Value, functionCall->Identifier.Length);
        if(searchResult)
        {
            address = searchResult->Value;
            opType = totemOperationType_ScriptFunction;
        }
        else
        {
            build->ErrorContext = functionCall;
            return totemEvalStatus_Break(totemEvalStatus_FunctionNotDefined);
        }
    }
    
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, result));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, result, (totemOperandXUnsigned)address, opType));
    
    for(totemExpressionPrototype *parameter = functionCall->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype operand;
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(parameter, build, scope, globals, &operand, totemEvalVariableFlag_MustBeDefined));
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAbxInstructionUnsigned(build, &operand, 0, totemOperationType_FunctionArg));
    }
    
    return totemEvalStatus_Success;
}

const char *totemEvalStatus_Describe(totemEvalStatus status)
{
    switch(status)
    {
        TOTEM_STRINGIFY_CASE(totemEvalStatus_ScriptFunctionAlreadyDefined);
        TOTEM_STRINGIFY_CASE(totemEvalStatus_NativeFunctionAlreadyDefined);
        TOTEM_STRINGIFY_CASE(totemEvalStatus_FunctionNotDefined);
        TOTEM_STRINGIFY_CASE(totemEvalStatus_InvalidArgument);
        TOTEM_STRINGIFY_CASE(totemEvalStatus_OutOfMemory);
        TOTEM_STRINGIFY_CASE(totemEvalStatus_Success);
        TOTEM_STRINGIFY_CASE(totemEvalStatus_TooManyRegisters);
        TOTEM_STRINGIFY_CASE(totemEvalStatus_InstructionOverflow);
        TOTEM_STRINGIFY_CASE(totemEvalStatus_VariableAlreadyDefined);
        TOTEM_STRINGIFY_CASE(totemEvalStatus_VariableNotDefined);
        default: return "UNKNOWN";
    }
}