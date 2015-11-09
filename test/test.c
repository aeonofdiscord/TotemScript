//
//  test
//  TotemScript
//
//  Created by Timothy Smale on 05/11/2015.
//  Copyright (c) 2015 Timothy Smale. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <TotemScript/totem.h>

int main(int argc, const char * argv[])
{
    // load file
    FILE *file = fopen("test.totem", "r");
    if(!file)
    {
        printf("Can't open file\n");
        return 1;
    }
    
    fseek(file, 0, SEEK_END);
    long fSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *str = (char*)malloc(fSize + 1);
    fread(str, fSize, 1, file);
    fclose(file);
    str[fSize] = 0;
    
    // init runtime
    totemRuntime runtime;
    memset(&runtime, 0, sizeof(totemRuntime));
    totemRuntime_Reset(&runtime);
    
    // lex
    totemTokenList tokens;
    memset(&tokens, 0, sizeof(totemTokenList));
    totemTokenList_Reset(&tokens);
    totemLexStatus lexStatus = totemTokenList_Lex(&tokens, str, strlen(str));
    if(lexStatus != totemLexStatus_Success)
    {
        printf("Lex error\n");
        return 1;
    }
    
    for(size_t i = 0; i < totemMemoryBuffer_GetNumObjects(&tokens.Tokens); i++)
    {
        totemToken *token = totemMemoryBuffer_Get(&tokens.Tokens, i);
        printf("%s: %.*s\n", totemTokenType_Describe(token->Type), token->Value.Length, token->Value.Value);
    }
    
    // parse
    totemParseTree parseTree;
    memset(&parseTree, 0, sizeof(totemParseTree));
    totemParseStatus parseStatus = totemParseTree_Parse(&parseTree, &tokens);
    if(parseStatus != totemParseStatus_Success)
    {
        printf("Parse error %s (%s) at %.*s \n", totemParseStatus_Describe(parseStatus), totemTokenType_Describe(parseTree.CurrentToken->Type), 50, parseTree.CurrentToken->Value.Value);
        return 1;
    }
    
    // eval
    totemBuildPrototype build;
    memset(&build, 0, sizeof(totemBuildPrototype));
    totemBuildPrototype_Init(&build, &runtime);
    totemEvalStatus evalStatus = totemBuildPrototype_Eval(&build, &parseTree);
    if(evalStatus != totemEvalStatus_Success)
    {
        printf("Eval error %s\n", totemEvalStatus_Describe(evalStatus));
        return 1;
    }
    
    totemString scriptName;
    totemString_FromLiteral(&scriptName, "TotemTest");
    size_t scriptAddr = 0;
    
    if(!totemRuntime_RegisterScript(&runtime, &build, &scriptName, &scriptAddr))
    {
        printf("Could not register script\n");
        return 1;
    }
    
    totemActor actor;
    memset(&actor, 0, sizeof(totemActor));
    if(totemActor_Init(&actor, &runtime, scriptAddr) != totemExecStatus_Continue)
    {
        printf("Could not create actor\n");
        return 1;
    }
    
    totemExecState execState;
    memset(&execState, 0, sizeof(totemExecState));
    if(!totemExecState_Init(&execState, &runtime, 4096))
    {
        printf("Could not create exec state\n");
        return 1;
    }
    
    totemRegister returnRegister;
    memset(&returnRegister, 0, sizeof(totemRegister));
    totemExecStatus execStatus = totemExecState_Exec(&execState, &actor, 0, &returnRegister);
    if(execStatus != totemExecStatus_Return)
    {
        printf("exec error %s\n", totemExecStatus_Describe(execStatus));
        return 1;
    }
    
    return 0;
}