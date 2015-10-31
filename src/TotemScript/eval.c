//
//  eval.cpp
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/eval.h>
#include <string.h>

totemEvalStatus totemRegisterListPrototype_AddRegister(totemRegisterListPrototype *list, totemOperandRegisterPrototype *operand)
{
    size_t index = 0;

    if(list->NumRegisters >= list->MaxRegisters)
    {
        size_t newNum = list->MaxRegisters * 1.5;
        size_t newMem = newNum * sizeof(totemRegisterPrototype);
        totemRegisterPrototype *reg = totem_Malloc(newMem);
        if(!reg)
        {
            return totemEvalStatus_OutOfMemory;
        }
        
        memcpy(reg, list->Registers, list->NumRegisters * sizeof(totemRegisterPrototype));
        totem_Free(list->Registers);
        list->Registers = reg;
        list->MaxRegisters = newNum;
    }
    
    index = list->NumRegisters;
    list->NumRegisters++;
    totemRegisterPrototype *reg = &list->Registers[index];
    
    reg->ScopeType = list->Scope;
    reg->Index = index;
    reg->Value.Data = 0;
    reg->DataType = totemDataType_Number;
    
    operand->RegisterIndex = index;
    operand->RegisterScopeType = list->Scope;
    
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
        
        reg = &list->Registers[operand->RegisterIndex];
        reg->DataType = totemDataType_String;
        reg->Value.String.Index = (uint32_t)list->StringDataLength;
        reg->Value.String.Length = str->Length;
        
        totemHashMap_Insert(&list->Strings, str->Value, str->Length, reg->Index);
        
        if(list->StringDataLength + str->Length >= list->StringDataCapacity)
        {
            size_t newNum = list->StringDataCapacity * 1.5;
            char *data = totem_Malloc(newNum);
            if(!data)
            {
                return totemEvalStatus_OutOfMemory;
            }
            
            memcpy(data, list->StringData, list->StringDataLength);
            totem_Free(list->StringData);
            list->StringData = data;
            list->StringDataCapacity = newNum;
        }
        
        memcpy(list->StringData + list->StringDataLength, str->Value, str->Length);
        list->StringDataLength += str->Length;
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
        
        totemRegisterPrototype *reg = &list->Registers[prototype->RegisterIndex];
        totemHashMap_Insert(&list->Variables, name->Value, name->Length, reg->Index);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemRegisterListPrototype_AddNumberConstant(totemRegisterListPrototype *list, totemNumber *number, totemOperandRegisterPrototype *operand)
{
    totemHashMapEntry *searchResult = totemHashMap_Find(&list->Numbers, (const char*)number, sizeof(totemNumber));
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
        
        totemHashMap_Insert(&list->Numbers, (const char*)&number, sizeof(totemNumber), operand->RegisterIndex);
        
        totemRegisterPrototype *reg = &list->Registers[operand->RegisterIndex];
        reg->Value.Number = *number;
    }
    
    return totemEvalStatus_Success;
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
    totemFunctionPrototype *globalFunction;
    totemString globalFunctionName;
    totemString_FromLiteral(&globalFunctionName, "__init");
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, &globalFunctionName, &globalFunction));
    
    // $this
    totemOperandRegisterPrototype thisRegister;
    totemString thisVar;
    totemString_FromLiteral(&thisVar, "this");
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddVariable(&build->GlobalRegisters, &thisVar, &thisRegister));
    
    totemBlockPrototype *block = prototype->FirstBlock;
    
    while(block != NULL)
    {
        switch(block->Type)
        {
            case totemBlockType_Statement:
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(block->Statement, build, &build->GlobalRegisters, &build->GlobalRegisters));
                break;
                
            case totemBlockType_FunctionDeclaration:
                TOTEM_EVAL_CHECKRETURN(totemFunctionDeclarationPrototype_Eval(block->FuncDec, build, &build->GlobalRegisters));
                break;
        }
        
        block = block->Next;
        
    };

    totemBuildPrototype_EvalImplicitReturn(build);
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_AllocFunction(totemBuildPrototype *build, totemString *name, totemFunctionPrototype **functionOut)
{
    
}

totemEvalStatus totemBuildPrototype_AllocInstruction(totemBuildPrototype *build, totemInstruction **instructionOut)
{
    
}

totemEvalStatus totemFunctionDeclarationPrototype_Eval(totemFunctionDeclarationPrototype *function, totemBuildPrototype *build, totemRegisterListPrototype *globals)
{
    totemRegisterListPrototype localRegisters;
    localRegisters.Scope = totemRegisterScopeType_Local;
    
    // TODO: Ensure function name doesn't already exist in runtime
    
    size_t functionIndex = build->NumFunctions;
    totemFunctionPrototype *functionPrototype;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocFunction(build, function->Identifier, &functionPrototype));
    totemHashMap_Insert(&build->FunctionLookup, function->Identifier->Value, function->Identifier->Length, functionIndex);
    
    for(totemVariablePrototype *parameter = function->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype dummy;
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddVariable(&localRegisters, &parameter->Identifier, &dummy));
    }
    
    // loop through statements & create instructions
    for(totemStatementPrototype *statement = function->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, &localRegisters, globals));
    }
    
    totemBuildPrototype_EvalImplicitReturn(build);
    return totemEvalStatus_Success;
}

totemEvalStatus totemStatementPrototype_Eval(totemStatementPrototype *statement, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemOperandRegisterPrototype empty;

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
            
        case totemStatementType_SwitchBlock:
            TOTEM_EVAL_CHECKRETURN(totemSwitchBlockPrototype_Eval(statement->SwitchBlock, build, scope, globals));
            break;
            
        case totemStatementType_WhileLoop:
            TOTEM_EVAL_CHECKRETURN(totemWhileLoopPrototype_Eval(statement->WhileLoop, build, scope, globals));
            break;
            
        case totemStatementType_Simple:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(statement->Expression, build, scope, globals, &empty));
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemExpressionPrototype_Eval(totemExpressionPrototype *expression, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *value)
{
    totemOperandRegisterPrototype lValue;
    
    // evaluate lvalue result to register
    switch(expression->LValueType)
    {
        case totemLValueType_Argument:
            TOTEM_EVAL_CHECKRETURN(totemArgumentPrototype_Eval(expression->LValueArgument, build, scope, globals, &lValue));
            break;
            
        case totemLValueType_Expression:
            TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->LValueExpression, build, scope, globals, &lValue));
            break;
    }
    
    totemOperandRegisterPrototype preUnaryRegister, preUnaryLValue;
    totemNumber preUnaryNumber = 0;
    
    switch(expression->PreUnaryOperator)
    {
        case totemPreUnaryOperatorType_Dec:
            preUnaryNumber = 1;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &preUnaryRegister, &lValue, totemOperation_Subtract));
            break;
            
        case totemPreUnaryOperatorType_Inc:
            preUnaryNumber = 1;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &preUnaryRegister, &lValue, totemOperation_Add));
            break;
            
        case totemPreUnaryOperatorType_LogicalNegate:
            preUnaryNumber = 0;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &preUnaryLValue));
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &preUnaryRegister, &lValue, totemOperation_Equals));
            memcpy(&lValue, &preUnaryLValue, sizeof(totemOperandRegisterPrototype));
            // A = B == C(0)
            break;
            
        case totemPreUnaryOperatorType_Negative:
            preUnaryNumber = -1;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &preUnaryLValue));
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &preUnaryNumber, &preUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &preUnaryLValue, &preUnaryRegister, &lValue, totemOperation_Multiply));
            memcpy(&lValue, &preUnaryLValue, sizeof(totemOperandRegisterPrototype));
            // A = B * -1
            break;
            
        case totemPreUnaryOperatorType_None:
            break;
    }
    
    totemOperandRegisterPrototype postUnaryRegister;
    totemNumber postUnaryNumber = 0;
        
    switch(expression->PostUnaryOperator)
    {
        case totemPostUnaryOperatorType_Dec:
            postUnaryNumber = 1;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &postUnaryNumber, &postUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &postUnaryRegister, &lValue, totemOperation_Subtract));
            break;
            
        case totemPostUnaryOperatorType_Inc:
            postUnaryNumber = 1;
            TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddNumberConstant(globals, &postUnaryNumber, &postUnaryRegister));
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &postUnaryRegister, &lValue, totemOperation_Add));
            break;
            
        case totemPostUnaryOperatorType_None:
            break;
    }
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        totemOperandRegisterPrototype rValue;
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(expression->RValue, build, scope, globals, &rValue));
        
        switch(expression->BinaryOperator)
        {
            case totemBinaryOperatorType_Plus:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_Add));
                break;
                
            case totemBinaryOperatorType_PlusAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &lValue, &rValue, totemOperation_Add));
                break;
                
            case totemBinaryOperatorType_Minus:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_Subtract));
                break;
                
            case totemBinaryOperatorType_MinusAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &lValue, &rValue, totemOperation_Subtract));
                break;
                
            case totemBinaryOperatorType_Multiply:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_Multiply));
                break;
                
            case totemBinaryOperatorType_MultiplyAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &lValue, &rValue, totemOperation_Multiply));
                break;
                
            case totemBinaryOperatorType_Divide:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_Divide));
                break;
                
            case totemBinaryOperatorType_DivideAssign:
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &lValue, &rValue, totemOperation_Divide));
                break;
                                
            case totemBinaryOperatorType_Assign:
            {
                totemRegisterPrototype *lValueReg = &scope->Registers[lValue.RegisterIndex];
                totemRegisterPrototype *rValueReg = NULL;
                switch(rValue.RegisterScopeType)
                {
                    case totemRegisterScopeType_Global:
                        rValueReg = &globals->Registers[rValue.RegisterIndex];
                        break;
                        
                    case totemRegisterScopeType_Local:
                        lValueReg = &globals->Registers[rValue.RegisterIndex];
                        break;
                }
                
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &lValue, &rValue, &rValue, totemOperation_Move));
                break;
            }
                
            case totemBinaryOperatorType_Equals:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_Equals));
                break;
                
            case totemBinaryOperatorType_MoreThan:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_MoreThan));
                break;
                
            case totemBinaryOperatorType_LessThan:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_LessThan));
                break;
                
            case totemBinaryOperatorType_MoreThanEquals:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_MoreThanEquals));
                break;
                
            case totemBinaryOperatorType_LessThanEquals:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_LessThanEquals));
                break;
                
            case totemBinaryOperatorType_LogicalAnd:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_LogicalAnd));
                break;
                
            case totemBinaryOperatorType_LogicalOr:
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, value));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, value, &lValue, &rValue, totemOperation_LogicalOr));
                break;
                
            case totemBinaryOperatorType_None:
                break;
        }
    }
    
    // TODO: operator precedence reordering
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemArgumentPrototype_Eval(totemArgumentPrototype *argument, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *value)
{
    // evaluate argument to register
    switch(argument->Type)
    {
        case totemArgumentType_Variable:
            build->ErrorContext = argument;
            totemEvalStatus status = totemVariablePrototype_Eval(argument->Variable, scope, globals, value, scope->Scope);
            if(status == totemEvalStatus_Success)
            {
                build->ErrorContext = NULL;
            }
            
            return status;
            
        case totemArgumentType_String:
            return totemRegisterListPrototype_AddStringConstant(globals, &argument->String, value);
            
        case totemArgumentType_Number:
            return totemRegisterListPrototype_AddNumberConstant(globals, &argument->Number, value);
            
        case totemArgumentType_FunctionCall:
            return totemFunctionCallPrototype_Eval(argument->FunctionCall, scope, globals, build, value);
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemVariablePrototype_Eval(totemVariablePrototype *variable, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemOperandRegisterPrototype *index, totemRegisterScopeType type)
{
    switch(type)
    {
        case totemRegisterScopeType_Local:
            if(!totemRegisterListPrototype_GetVariable(globals, &variable->Identifier, index))
            {
                return totemRegisterListPrototype_AddVariable(scope, &variable->Identifier, index);
            }
            break;
            
        case totemRegisterScopeType_Global:
            if(!totemRegisterListPrototype_GetVariable(globals, &variable->Identifier, index))
            {
                return totemEvalStatus_InvalidArgument;
            }
            break;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemForLoopPrototype_Eval(totemForLoopPrototype *forLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemOperandRegisterPrototype initialisation, condition, afterThought;//, *loop = NULL;
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->Initialisation, build, scope, globals, &initialisation));
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->Initialisation, build, scope, globals, &condition));
    
    size_t gotoInstructionIndex = build->NumFunctions;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABxInstruction(build, &condition, 0, totemOperation_ConditionalGoto));
    
    for(totemStatementPrototype *statement = forLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(forLoop->AfterThought, build, scope, globals, &afterThought));
    totemInstruction *gotoInstruction = &build->Instructions[gotoInstructionIndex];
    totemOperandX offset = (totemOperandX)(build->NumInstructions - gotoInstructionIndex);
    gotoInstruction->Abx.OperandBx = offset;
    
    totemOperandX resetLoopOffset = (totemOperandX)(gotoInstructionIndex - build->NumInstructions - 1);
    totemBuildPrototype_EvalAxxInstruction(build, resetLoopOffset, totemOperation_Goto);
    return totemEvalStatus_Success;
}

totemEvalStatus totemIfBlockPrototype_Eval(totemIfBlockPrototype *ifBlock, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemOperandRegisterPrototype condition;
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(ifBlock->Expression, build, scope, globals, &condition));
    
    size_t gotoInstructionIndex = build->NumFunctions;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABxInstruction(build, &condition, 0, totemOperation_ConditionalGoto));
    
    for(totemStatementPrototype *statement = ifBlock->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    totemInstruction *gotoInstruction = &build->Instructions[gotoInstructionIndex];
    totemOperandX offset = (totemOperandX)(build->NumInstructions - gotoInstructionIndex);
    gotoInstruction->Abx.OperandBx = offset;
    
    if(ifBlock->ElseType == totemIfElseBlockType_None)
    {
        return totemEvalStatus_Success;
    }
    
    // skip over next block
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAxxInstruction(build, 0, totemOperation_Goto));
    gotoInstructionIndex = build->NumInstructions - 1;
    
    switch(ifBlock->ElseType)
    {
        case totemIfElseBlockType_Else:
            for(totemStatementPrototype *statement = ifBlock->ElseBlock->StatementsStart; statement != NULL; statement = statement->Next)
            {
                TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
            }
            break;
            
        case totemIfElseBlockType_ElseIf:
            TOTEM_EVAL_CHECKRETURN(totemIfBlockPrototype_Eval(ifBlock->IfElseBlock, build, scope, globals));
            break;
            
        case totemIfElseBlockType_None:
            break;
    }
    
    gotoInstruction = &build->Instructions[gotoInstructionIndex];
    offset = (totemOperandX)(build->NumInstructions - gotoInstructionIndex);
    gotoInstruction->Axx.OperandAxx = offset;
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemSwitchBlockPrototype_Eval(totemSwitchBlockPrototype *switchBlock, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemOperandRegisterPrototype expression, caseCondition, caseEquals;
    
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(switchBlock->Expression, build, scope, globals, &expression));
    
    size_t firstBreak = 0, prevBreak = 0;
    
    for(totemSwitchCasePrototype *switchCase = switchBlock->CasesStart; switchCase != NULL; switchCase = switchCase->Next)
    {
        switch(switchCase->Type)
        {
            case totemSwitchCaseType_Expression:
                TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(switchCase->Expression, build, scope, globals, &caseCondition));
                TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &caseEquals));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABCInstruction(build, &caseEquals, &caseCondition, &expression, totemOperation_Equals));
                TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABxInstruction(build, &caseEquals, 0, totemOperation_ConditionalGoto));
                break;
                
            case totemSwitchCaseType_Default:
                break;
        }
        
        for(totemStatementPrototype *statement = switchCase->StatementsStart; statement != NULL; statement = statement->Next)
        {
            TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
        }
        
        if(switchCase->HasBreak)
        {
            size_t thisBreak = build->NumInstructions - 1;
            TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAxxInstruction(build, 0, totemOperation_Goto));
            if(firstBreak == 0)
            {
                firstBreak = thisBreak;
            }
            
            if(prevBreak != 0)
            {
                totemInstruction *instruction = &build->Instructions[prevBreak];
                instruction->Axx.OperandAxx = (totemOperandX)thisBreak;
            }
            
            prevBreak = thisBreak;
        }
    }
    
    for(size_t br = firstBreak; firstBreak != 0; )
    {
        totemInstruction *instruction = &build->Instructions[br];
        size_t next = instruction->Axx.OperandAxx;
        instruction->Axx.OperandAxx = (totemOperandX)(build->NumInstructions - 1 - br);
        br = next;
    }
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemDoWhileLoopPrototype_Eval(totemDoWhileLoopPrototype *doWhileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    size_t gotoInstructionIndex = build->NumInstructions - 1;
    
    for(totemStatementPrototype *statement = doWhileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    totemOperandRegisterPrototype condition;
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(doWhileLoop->Expression, build, scope, globals, &condition));
    
    totemOperandX offset = (totemOperandX)(gotoInstructionIndex - build->NumInstructions);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABxInstruction(build, &condition, 2, totemOperation_ConditionalGoto));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAxxInstruction(build, offset, totemOperation_Goto));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemWhileLoopPrototype_Eval(totemWhileLoopPrototype *whileLoop, totemBuildPrototype *build, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals)
{
    totemOperandRegisterPrototype condition;
    TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(whileLoop->Expression, build, scope, globals, &condition));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABxInstruction(build, &condition, 0, totemOperation_ConditionalGoto));
    size_t gotoInstructionIndex = build->NumInstructions - 1;
    
    for(totemStatementPrototype *statement = whileLoop->StatementsStart; statement != NULL; statement = statement->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemStatementPrototype_Eval(statement, build, scope, globals));
    }
    
    totemInstruction *gotoInstruction = &build->Instructions[gotoInstructionIndex];
    totemOperandX offset = (totemOperandX)(build->NumInstructions - gotoInstructionIndex);
    gotoInstruction->Abx.OperandBx = offset;
    
    totemOperandX resetLoopOffset = (totemOperandX)(gotoInstructionIndex - build->NumInstructions);
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalAxxInstruction(build, resetLoopOffset, totemOperation_Goto));
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAxxInstruction(totemBuildPrototype *build, totemOperandX ax, totemOperation operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    
    instruction->Axx.Operation = operationType;
    instruction->Axx.OperandAxx = ax;
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbxInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandX bx, totemOperation operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    
    instruction->Abx.Operation = operationType;
    instruction->Abx.OperandAIndex = a->RegisterIndex;
    instruction->Abx.OperandAType = a->RegisterScopeType;
    instruction->Abx.OperandBx = bx;
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalAbcInstruction(totemBuildPrototype *build, totemOperandRegisterPrototype *a, totemOperandRegisterPrototype *b, totemOperandRegisterPrototype *c, totemOperation operationType)
{
    totemInstruction *instruction = NULL;
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_AllocInstruction(build, &instruction));
    
    instruction->Abc.Operation = operationType;
    instruction->Abc.OperandAIndex = a->RegisterIndex;
    instruction->Abc.OperandAType = a->RegisterScopeType;
    instruction->Abc.OperandBIndex = b->RegisterIndex;
    instruction->Abc.OperandBType = b->RegisterScopeType;
    instruction->Abc.OperandCIndex = c->RegisterIndex;
    instruction->Abc.OperandCType = c->RegisterScopeType;
    
    return totemEvalStatus_Success;
}

totemEvalStatus totemBuildPrototype_EvalImplicitReturn(totemBuildPrototype *build)
{
    totemOperandRegisterPrototype operands[3];
    memset(operands, 0, sizeof(totemOperandRegisterPrototype) * 3);
    
    operands[0].RegisterScopeType = totemRegisterScopeType_Local;
    operands[9].RegisterIndex = 0;
    
    return totemBuildPrototype_EvalAbcInstruction(build, &operands[0], &operands[1], &operands[2], totemOperation_Return);
}

totemEvalStatus totemFunctionCallPrototype_Eval(totemFunctionCallPrototype *functionCall, totemRegisterListPrototype *scope, totemRegisterListPrototype *globals, totemBuildPrototype *build, totemOperandRegisterPrototype *index)
{
    size_t address;
    totemOperation opType;
    
    // check for native function
    if(col::script::getNativeFunctionAddress(functionCall->Identifier, address))
    {
        opType = totemOperation_NativeFunction;
    }
    // check for script function
    else
    {
        size_t *searchResult = build.m_functionNames.find(functionCall->Identifier);
        if(searchResult != NULL)
        {
            address = *searchResult;
            opType = totemOperation_ScriptFunction;
        }
        else
        {
            return false;
        }
    }
    
    size_t numArgs = 0;
    totemOperandRegisterPrototype arg, result;
    totemRegisterIndex firstIndex = 0;
    for(totemExpressionPrototype *parameter = functionCall->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &arg));
        if(numArgs == 0)
        {
            firstIndex = arg.RegisterIndex;
        }
        numArgs++;
    }
    
    size_t currentArg;
    for(totemExpressionPrototype *parameter = functionCall->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype paramRegister;
        paramRegister.RegisterIndex = firstIndex + currentArg;
        paramRegister.RegisterScopeType = scope->Scope;
        TOTEM_EVAL_CHECKRETURN(totemExpressionPrototype_Eval(parameter, build, scope, globals, &paramRegister));
        currentArg++;
    }
    
    TOTEM_EVAL_CHECKRETURN(totemRegisterListPrototype_AddRegister(scope, &result));
    TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABxInstruction(build, &result, (totemOperandX)address, opType));
    
    currentArg = 0;
    for(totemExpressionPrototype *parameter = functionCall->ParametersStart; parameter != NULL; parameter = parameter->Next)
    {
        totemOperandRegisterPrototype paramRegister;
        paramRegister.RegisterIndex = firstIndex + currentArg;
        paramRegister.RegisterScopeType = scope->Scope;
        TOTEM_EVAL_CHECKRETURN(totemBuildPrototype_EvalABxInstruction(build, &paramRegister, (totemOperandX)(currentArg == numArgs - 1), totemOperation_FunctionArg));
        currentArg++;
    }
    
    return totemEvalStatus_Success;
}