//
//  exec.h
//  TotemScript
//
//  Created by Timothy Smale on 02/11/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#ifndef TOTEMSCRIPT_EXEC_H
#define TOTEMSCRIPT_EXEC_H

#include <stdint.h>
#include <TotemScript/base.h>
#include <TotemScript/eval.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef enum
    {
        totemExecStatus_Continue,
        totemExecStatus_Return,
        totemExecStatus_Stop,
        totemExecStatus_ScriptNotFound,
        totemExecStatus_ScriptFunctionNotFound,
        totemExecStatus_NativeFunctionNotFound,
        totemExecStatus_UnexpectedDataType,
        totemExecStatus_UnrecognisedOperation,
        totemExecStatus_RegisterOverflow,
        totemExecStatus_InstructionOverflow,
        totemExecStatus_OutOfMemory,
        totemExecStatus_IndexOutOfBounds,
        totemExecStatus_RefCountOverflow,
        totemExecStatus_FailedAssertion,
        totemExecStatus_DivideByZero,
        totemExecStatus_InternalBufferOverrun
    }
    totemExecStatus;
    
    const char *totemExecStatus_Describe(totemExecStatus status);
    
    typedef enum
    {
        totemInterpreterStatus_Success,
        totemInterpreterStatus_FileError,
        totemInterpreterStatus_LexError,
        totemInterpreterStatus_ParseError,
        totemInterpreterStatus_EvalError,
        totemInterpreterStatus_LinkError
    }
    totemInterpreterStatus;
    
    typedef enum
    {
        totemLinkStatus_Success,
        totemLinkStatus_OutOfMemory,
        totemLinkStatus_FunctionAlreadyDeclared,
        totemLinkStatus_FunctionNotDeclared,
        totemLinkStatus_InvalidNativeFunctionAddress,
        totemLinkStatus_InvalidNativeFunctionName,
        totemLinkStatus_TooManyNativeFunctions,
        totemLinkStatus_UnexpectedValueType
    }
    totemLinkStatus;
    
    const char *totemLinkStatus_Describe(totemLinkStatus status);
    
    typedef struct
    {
        totemHashMap FunctionNameLookup;
        totemMemoryBuffer GlobalRegisters;
        totemMemoryBuffer Functions;
        totemMemoryBuffer FunctionNames;
        totemMemoryBuffer Instructions;
    }
    totemScript;
    
    typedef struct
    {
        totemScript *Script;
        totemMemoryBuffer GlobalRegisters;
    }
    totemActor;
    
    typedef struct
    {
        size_t InstructionsStart;
        uint8_t RegistersNeeded;
    }
    totemScriptFunction;
    
    typedef enum
    {
        totemFunctionCallFlag_None = 0,
        totemFunctionCallFlag_FreeStack = 1,
        totemFunctionCallFlag_IsCoroutine = 2
    }
    totemFunctionCallFlag;
    
    typedef struct totemFunctionCall
    {
        struct totemFunctionCall *Prev;
        totemActor *CurrentActor;
        totemRegister *ReturnRegister;
        totemRegister *PreviousFrameStart;
        totemRegister *FrameStart;
        totemInstruction *ResumeAt;
        totemOperandXUnsigned FunctionHandle;
        totemFunctionType Type;
        totemFunctionCallFlag Flags;
        uint8_t NumRegisters;
        uint8_t NumArguments;
    }
    totemFunctionCall;
    
    typedef struct
    {
        totemMemoryBuffer NativeFunctions;
        totemMemoryBuffer NativeFunctionNames;
        totemHashMap NativeFunctionsLookup;
        totemHashMap InternedStrings;
    }
    totemRuntime;
    
    typedef struct totemExecState
    {
        totemFunctionCall *CallStack;
        totemInstruction *CurrentInstruction;
        totemRuntime *Runtime;
        totemRegister *Registers[2];
        size_t MaxLocalRegisters;
        size_t UsedLocalRegisters;
        
        totemFunctionCall *FunctionCallFreeList;
    }
    totemExecState;
    
    typedef struct
    {
        totemScriptFile Script;
        totemTokenList TokenList;
        totemParseTree ParseTree;
        totemBuildPrototype Build;
    }
    totemInterpreter;
    
    typedef struct
    {
        size_t ErrorLine;
        size_t ErrorChar;
        size_t ErrorLength;
        totemInterpreterStatus Status;
        union
        {
            totemLoadScriptStatus FileStatus;
            totemLexStatus LexStatus;
            totemParseStatus ParseStatus;
            totemEvalStatus EvalStatus;
            totemLinkStatus LinkStatus;
        };
    }
    totemInterpreterResult;
    
    typedef totemExecStatus(*totemNativeFunction)(totemExecState*);
    
    void totemActor_Init(totemActor *actor);
    void totemActor_Reset(totemActor *actor);
    void totemActor_Cleanup(totemActor *actor);
    
    void totemScript_Init(totemScript *script);
    void totemScript_Reset(totemScript *script);
    void totemScript_Cleanup(totemScript *script);
    totemLinkStatus totemScript_LinkActor(totemScript *script, totemActor *actor);
    
    void totemRuntime_Init(totemRuntime *runtime);
    void totemRuntime_Reset(totemRuntime *runtime);
    void totemRuntime_Cleanup(totemRuntime *runtime);
    totemLinkStatus totemRuntime_LinkExecState(totemRuntime *runtime, totemExecState *state, size_t numRegisters);
    totemLinkStatus totemRuntime_LinkBuild(totemRuntime *runtime, totemBuildPrototype *build, totemScript *scriptOut);
    totemLinkStatus totemRuntime_LinkNativeFunction(totemRuntime *runtime, totemNativeFunction func, totemString *name, totemOperandXUnsigned *addressOut);
    totemLinkStatus totemRuntime_InternString(totemRuntime *runtime, totemString *str, totemRegisterValue *valOut, totemPrivateDataType *typeOut);
    totemBool totemRuntime_GetNativeFunctionAddress(totemRuntime *runtime, totemString *name, totemOperandXUnsigned *addressOut);
    
    enum
    {
        totemGCObjectType_Array,
        totemGCObjectType_Coroutine
    };
    typedef uint8_t totemGCObjectType;
    
    typedef struct
    {
        uint32_t NumRegisters;
        totemRegister Registers[1];
    }
    totemArray;
    
    typedef struct totemGCObject
    {
        union
        {
            totemArray *Array;
            totemFunctionCall *Coroutine;
        };
        uint32_t RefCount;
        totemGCObjectType Type;
    }
    totemGCObject;
    
    totemGCObject *totemExecState_CreateGCObject(totemExecState *state, totemGCObjectType type);
    void totemExecState_DestroyGCObject(totemExecState *state, totemGCObject *gc);
    totemExecStatus totemExecState_CreateCoroutine(totemExecState *state, totemOperandXUnsigned functionAddress, totemGCObject **objOut);
    totemExecStatus totemExecState_CreateArray(totemExecState *state, uint32_t numRegisters, totemGCObject **objOut);
    totemExecStatus totemExecState_CreateArrayFromExisting(totemExecState *state, totemRegister *registers, uint32_t numRegisters, totemGCObject **objOut);
    void totemExecState_DecRefCount(totemExecState *state, totemGCObject *gc);
    void totemExecState_DestroyArray(totemExecState *state, totemArray *arr);
    void totemExecState_DestroyCoroutine(totemExecState *state, totemFunctionCall *co);
    
    /**
     * Execute bytecode
     */
    void totemExecState_Init(totemExecState *state);
    void totemExecState_Cleanup(totemExecState *state);
    totemExecStatus totemExecState_Exec(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionAddress, totemRegister *returnRegister);
    totemExecStatus totemExecState_ExecNative(totemExecState *state, totemActor *actor, totemOperandXUnsigned functionHandle, totemRegister *returnRegister);
    totemExecStatus totemExecState_ExecInstruction(totemExecState *state);
    totemExecStatus totemExecState_ExecMove(totemExecState *state);
    totemExecStatus totemExecState_ExecMoveToGlobal(totemExecState *state);
    totemExecStatus totemExecState_ExecMoveToLocal(totemExecState *state);
    totemExecStatus totemExecState_ExecAdd(totemExecState *state);
    totemExecStatus totemExecState_ExecSubtract(totemExecState *state);
    totemExecStatus totemExecState_ExecMultiply(totemExecState *state);
    totemExecStatus totemExecState_ExecDivide(totemExecState *state);
    totemExecStatus totemExecState_ExecPower(totemExecState *state);
    totemExecStatus totemExecState_ExecEquals(totemExecState *state);
    totemExecStatus totemExecState_ExecNotEquals(totemExecState *state);
    totemExecStatus totemExecState_ExecLessThan(totemExecState *state);
    totemExecStatus totemExecState_ExecLessThanEquals(totemExecState *state);
    totemExecStatus totemExecState_ExecMoreThan(totemExecState *state);
    totemExecStatus totemExecState_ExecMoreThanEquals(totemExecState *state);
    totemExecStatus totemExecState_ExecReturn(totemExecState *state);
    totemExecStatus totemExecState_ExecGoto(totemExecState *state);
    totemExecStatus totemExecState_ExecConditionalGoto(totemExecState *state);
    totemExecStatus totemExecState_ExecNativeFunction(totemExecState *state);
    totemExecStatus totemExecState_ExecScriptFunction(totemExecState *state);
    totemExecStatus totemExecState_ExecNewArray(totemExecState *state);
    totemExecStatus totemExecState_ExecArrayGet(totemExecState *state);
    totemExecStatus totemExecState_ExecArraySet(totemExecState *state);
    totemExecStatus totemExecState_ExecIs(totemExecState *state);
    totemExecStatus totemExecState_ExecAs(totemExecState *state);
    totemExecStatus totemExecState_ExecFunctionPointer(totemExecState *state);
    totemExecStatus totemExecState_ExecAssert(totemExecState *state);
    
    void totemRegister_Print(FILE *file, totemRegister *reg);
    void totemRegister_PrintList(FILE *file, totemRegister *regs, size_t numRegisters);
    
#ifdef __cplusplus
}
#endif

#endif
