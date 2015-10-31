//
//  parse.cpp
//  TotemScript
//
//  Created by Timothy Smale on 19/10/2013.
//  Copyright (c) 2013 Timothy Smale. All rights reserved.
//

#include <TotemScript/parse.h>
#include <TotemScript/base.h>
#include <string.h>
#include <ctype.h>

/**
 * Lex buffer into token list
 */
totemLexStatus totemTokenList_Lex(totemTokenList *list, const char *buffer, size_t bufferLength)
{
    size_t currentTokenLength = 0;
    size_t currentTokenStart = 0;
    size_t currentLine = 1;
    size_t currentLineChar = 0;
    const char *toCheck = NULL;
    totemToken* currentToken;
    TOTEM_LEX_ALLOC(currentToken, list);
    memset(currentToken, 0, sizeof(totemToken));
    
    for(size_t i = 0; i < bufferLength; ++i)
    {
        currentToken->Value.Value = NULL;
        currentToken->Value.Length = 0;
        currentToken->Type = totemTokenType_None;
        
        if(buffer[i] == '\r' || buffer[i] == '\t')
        {
            continue;
        }
        
        if(buffer[i] == '\n')
        {
            ++currentLine;
            currentLineChar = 0;
            continue;
            
        } else {
            ++currentLineChar;
        }
        
        if(currentTokenLength == 0)
        {
            currentTokenStart = i;
            toCheck = buffer + currentTokenStart;
            currentToken->Position.LineNumber = currentLine;
            currentToken->Position.CharNumber = currentLineChar;
        }
        
        if(buffer[i] == ' ' && currentTokenLength > 0)
        {
            if(!totemToken_LexReservedWordToken(currentToken, toCheck, currentTokenLength))
            {
                totemToken_LexNumberOrIdentifierToken(currentToken, toCheck, currentTokenLength);
            }
            
            currentTokenLength = 0;
            --i;
            continue;
        }
        
        ++currentTokenLength;
    
        if(totemToken_LexSymbolToken(currentToken, toCheck, 1))
        {
            currentTokenLength = 0;
            i += currentToken->Value.Length - 1;
        }
        else
        {
            for(size_t j = 1; j < currentTokenLength; ++j)
            {
                if(totemToken_LexSymbolToken(currentToken, toCheck + j, 1))
                {
                    currentToken->Position.CharNumber += j;
                    
                    totemToken *nextToken;
                    TOTEM_LEX_ALLOC(nextToken, list);
                    memcpy(nextToken, currentToken, sizeof(totemToken));
                    
                    currentToken->Position.CharNumber = currentLineChar;
                    currentToken->Position.LineNumber = currentLine;
                    if(!totemToken_LexReservedWordToken(currentToken, toCheck, j)) {
                        totemToken_LexNumberOrIdentifierToken(currentToken, toCheck, j);
                    }
                    
                    currentTokenLength = 0;
                }
            }
        }
    }
    
    totemToken *endToken;
    TOTEM_LEX_ALLOC(endToken, list);
    endToken->Type = totemTokenType_EndScript;
    endToken->Value.Value = NULL;
    endToken->Value.Length = 0;
    endToken->Position.LineNumber = currentLine;
    endToken->Position.CharNumber = currentLineChar;
    
    return totemLexStatus_Success;
}

void totemToken_LexNumberOrIdentifierToken(totemToken *currentToken, const char *toCheck, size_t length)
{
    currentToken->Type = totemTokenType_Number;
    currentToken->Value.Value = toCheck;
    currentToken->Value.Length = (uint32_t)length;
    
    for(size_t j = 0; j < length; ++j)
    {
        if(!isdigit(toCheck[j]))
        {
            currentToken->Type = totemTokenType_Identifier;
            break;
        }
    }
}

totemBool totemToken_LexReservedWordToken(totemToken *token, const char *toCheck, size_t length)
{
    for(size_t i = 0 ; i < TOTEM_ARRAYSIZE(s_reservedWordValues); ++i)
    {
        size_t tokenLength = strlen(s_reservedWordValues[i].Value);
        if(strncmp(toCheck, s_reservedWordValues[i].Value, strlen(s_reservedWordValues[i].Value)) == 0)
        {
            token->Type = s_reservedWordValues[i].Type;
            token->Value.Value = toCheck;
            token->Value.Length = (uint32_t)tokenLength;
            return totemBool_True;
        }
    }
    
    return totemBool_False;
}

totemBool totemToken_LexSymbolToken(totemToken *token, const char *toCheck, size_t length)
{
    for(size_t i = 0 ; i < TOTEM_ARRAYSIZE(s_symbolTokenValues); ++i)
    {
        size_t tokenLength = strlen(s_symbolTokenValues[i].Value);
        if(strncmp(toCheck, s_symbolTokenValues[i].Value, strlen(s_symbolTokenValues[i].Value)) == 0)
        {
            token->Type = s_symbolTokenValues[i].Type;
            token->Value.Value = toCheck;
            token->Value.Length = (uint32_t)tokenLength;
            return totemBool_True;
        }
    }
    
    return totemBool_False;
}

void *totemParseTree_Alloc(totemParseTree *tree, size_t objectSize)
{
    size_t allocSize = objectSize;
    size_t extra = allocSize % sizeof(uintptr_t);
    if(extra != 0)
    {
        allocSize += sizeof(uintptr_t) - extra;
    }
    
    totemMemoryBlock *chosenBlock = NULL;
    
    for(totemMemoryBlock *block = tree->LastMemBlock; block != NULL; block = block->Prev)
    {
        if(block->Remaining > allocSize)
        {
            chosenBlock = block;
            break;
        }
    }
    
    if(chosenBlock == NULL)
    {
        // alloc new
        chosenBlock = (totemMemoryBlock*)totem_Malloc(sizeof(totemMemoryBlock));
        if(chosenBlock == NULL)
        {
            return NULL;
        }
        chosenBlock->Remaining = TOTEM_MEMORYBLOCK_DATASIZE;
        chosenBlock->Prev = tree->LastMemBlock;
        tree->LastMemBlock = chosenBlock;
    }
    
    void *ptr = chosenBlock->Data + (TOTEM_MEMORYBLOCK_DATASIZE - chosenBlock->Remaining);
    memset(ptr, 0, objectSize);
    chosenBlock->Remaining -= allocSize;
    return ptr;
}

void totemParseTree_Cleanup(totemParseTree *tree)
{
    while(tree->LastMemBlock)
    {
        void *ptr = tree->LastMemBlock;
        tree->LastMemBlock = tree->LastMemBlock->Prev;
        totem_Free(ptr);
    }
    
    tree->FirstBlock = NULL;
    tree->CurrentToken = NULL;
}

/**
 * Create parse tree from token list
 */
totemParseStatus totemParseTree_Parse(totemParseTree *tree, totemTokenList *tokens)
{
    totemBlockPrototype *lastBlock = NULL;
    
    for(tree->CurrentToken = tokens->Token; tree->CurrentToken->Type != totemTokenType_EndScript; )
    {
        totemBlockPrototype *block;
        TOTEM_PARSE_ALLOC(block, totemBlockPrototype, tree);
        
        totemParseStatus status = totemBlockPrototype_Parse(block, &tree->CurrentToken, tree);
        if(status != totemParseStatus_Success)
        {
            return status;
        }
        
        if(lastBlock == NULL)
        {
            tree->FirstBlock = block;
        }
        else
        {
            lastBlock->Next = block;
        }
        
        lastBlock = block;
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemBlockPrototype_Parse(totemBlockPrototype *block, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, block);
    
    switch((*token)->Type)
    {
        case totemTokenType_Function:
        {
            block->Type = totemBlockType_FunctionDeclaration;
            TOTEM_PARSE_ALLOC(block->FuncDec, totemFunctionDeclarationPrototype, tree);
            return totemFunctionDeclarationPrototype_Parse(block->FuncDec, token, tree);
        }
            
        default:
        {
            block->Type = totemBlockType_Statement;
            TOTEM_PARSE_ALLOC(block->Statement, totemStatementPrototype, tree);
            return totemStatementPrototype_Parse(block->Statement, token, tree);
        }
    }
}

totemParseStatus totemStatementPrototype_Parse(totemStatementPrototype *statement, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, statement);
    
    memcpy(&statement->Position, &(*token)->Position, sizeof(totemBufferPositionInfo));
    
    switch((*token)->Type)
    {
        case totemTokenType_While:
        {
            statement->Type = totemStatementType_WhileLoop;
            TOTEM_PARSE_ALLOC(statement->WhileLoop, totemWhileLoopPrototype, tree);
            return totemWhileLoopPrototype_Parse(statement->WhileLoop, token, tree);
        }
            
        case totemTokenType_Do:
        {
            statement->Type = totemStatementType_DoWhileLoop;
            TOTEM_PARSE_ALLOC(statement->DoWhileLoop, totemDoWhileLoopPrototype, tree);
            return totemDoWhileLoopPrototype_Parse(statement->DoWhileLoop, token, tree);
        }
            
        case totemTokenType_For:
        {
            statement->Type = totemStatementType_ForLoop;
            TOTEM_PARSE_ALLOC(statement->ForLoop, totemForLoopPrototype, tree);
            return totemForLoopPrototype_Parse(statement->ForLoop, token, tree);
        }
            
        case totemTokenType_If:
        {
            statement->Type = totemStatementType_IfBlock;
            TOTEM_PARSE_ALLOC(statement->IfBlock, totemIfBlockPrototype, tree);
            return totemIfBlockPrototype_Parse(statement->IfBlock, token, tree);
        }
            
        case totemTokenType_Switch:
        {
            statement->Type = totemStatementType_SwitchBlock;
            TOTEM_PARSE_ALLOC(statement->SwitchBlock, totemSwitchBlockPrototype, tree);
            return totemSwitchBlockPrototype_Parse(statement->SwitchBlock, token, tree);
        }
            
        default:
        {
            statement->Type = totemStatementType_Simple;
            TOTEM_PARSE_ALLOC(statement->Expression, totemExpressionPrototype, tree);
            
            TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(statement->Expression, token, tree));
            
            TOTEM_PARSE_SKIPWHITESPACE(token);
            TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            return totemParseStatus_Success;
        }
    }
}

totemParseStatus totemStatementPrototype_ParseInSet(totemStatementPrototype **first, totemStatementPrototype **last, totemToken **token, totemParseTree *tree)
{
    totemStatementPrototype *statement = NULL;
    TOTEM_PARSE_ALLOC(statement, totemStatementPrototype, tree);
    
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_Parse(statement, token, tree))
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if(*first == NULL)
    {
        *first = statement;
    }
    else
    {
        (*last)->Next = statement;
    }
    
    *last = statement;
    return totemParseStatus_Success;
}

totemParseStatus totemStatementPrototype_ParseSet(totemStatementPrototype **first, totemStatementPrototype **last, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_LCBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
	while((*token)->Type != totemTokenType_RCBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseInSet(first, last, token, tree));
    }
    
    ++token;
    return totemParseStatus_Success;
}

totemParseStatus totemVariablePrototype_ParseParameterList(totemVariablePrototype **first, totemVariablePrototype **last, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_LBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    while((*token)->Type != totemTokenType_RBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_ParseParameterInList(first, last, token, tree));
    }
    
    ++token;
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_ParseParameterList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_LBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    while((*token)->Type != totemTokenType_RBracket)
    {
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterInList(first, last, token, tree));
    }
    
    ++token;
    return totemParseStatus_Success;
}

totemParseStatus totemVariablePrototype_ParseParameterInList(totemVariablePrototype **first, totemVariablePrototype **last, totemToken **token, totemParseTree *tree)
{
    totemVariablePrototype *parameter = NULL;
    TOTEM_PARSE_ALLOC(parameter, totemVariablePrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_Parse(parameter, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if((*token)->Type != totemTokenType_RBracket && (*token)->Type != totemTokenType_Comma)
    {
        return totemParseStatus_UnexpectedToken;
    }
    
    if(*first == NULL)
    {
        *first = parameter;
    }
    else
    {
        (*last)->Next = parameter;
    }
    
    *last = parameter;
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_ParseParameterInList(totemExpressionPrototype **first, totemExpressionPrototype **last, totemToken **token, totemParseTree *tree)
{
    totemExpressionPrototype *parameter = NULL;
    TOTEM_PARSE_ALLOC(parameter, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(parameter, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if((*token)->Type != totemTokenType_RBracket && (*token)->Type != totemTokenType_Comma)
    {
        return totemParseStatus_UnexpectedToken;
    }
    
    if(*first == NULL)
    {
        *first = parameter;
    }
    else
    {
        (*last)->Next = parameter;
    }
    
    *last = parameter;
    return totemParseStatus_Success;
}

totemParseStatus totemWhileLoopPrototype_Parse(totemWhileLoopPrototype *loop, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, loop);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_While);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(loop->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Expression, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    loop->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemDoWhileLoopPrototype_Parse(totemDoWhileLoopPrototype *loop, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, loop);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Do);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    loop->StatementsStart = firstStatement;
    
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_While);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(loop->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Expression, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
    
    ++token;
    return totemParseStatus_Success;
}

totemParseStatus totemIfBlockPrototype_Parse(totemIfBlockPrototype *block, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, block);
    block->ElseType = totemIfElseBlockType_None;
    
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_If);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_ALLOC(block->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(block->Expression, token, tree));
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    block->StatementsStart = firstStatement;
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if((*token)->Type == totemTokenType_Else)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        TOTEM_PARSE_SKIPWHITESPACE(token);
        
        switch((*token)->Type)
        {
            case totemTokenType_If:
            {
                block->ElseType = totemIfElseBlockType_ElseIf;
                TOTEM_PARSE_ALLOC(block->IfElseBlock, totemIfBlockPrototype, tree);
                TOTEM_PARSE_CHECKRETURN(totemIfBlockPrototype_Parse(block->IfElseBlock, token, tree));
                break;
            }
                
            case totemTokenType_LCBracket:
            {
                block->ElseType = totemIfElseBlockType_Else;
                TOTEM_PARSE_ALLOC(block->ElseBlock, totemElseBlockPrototype, tree);
                TOTEM_PARSE_COPYPOSITION(token, block->ElseBlock);
                
                totemStatementPrototype *firstElseStatement = NULL, *lastElseStatement = NULL;
                TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstElseStatement, &lastElseStatement, token, tree));
                block->ElseBlock->StatementsStart = firstElseStatement;
                break;
            }
                
            default:
                return totemParseStatus_UnexpectedToken;
        }
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemForLoopPrototype_Parse(totemForLoopPrototype *loop, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, loop);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_For);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if((*token)->Type == totemTokenType_LBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    
    TOTEM_PARSE_ALLOC(loop->Initialisation, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Initialisation, token, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(loop->Condition, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->Condition, token, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(loop->AfterThought, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(loop->AfterThought, token, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    if((*token)->Type == totemTokenType_RBracket)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    loop->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemSwitchBlockPrototype_Parse(totemSwitchBlockPrototype *block, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, block);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Switch);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    TOTEM_PARSE_ALLOC(block->Expression, totemExpressionPrototype, tree);
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(block->Expression, token, tree));
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_LCBracket);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    totemSwitchCasePrototype *first = NULL, *last = NULL;
    while((*token)->Type != totemTokenType_RCBracket)
    {
        totemSwitchCasePrototype *switchCase = NULL;
        TOTEM_PARSE_ALLOC(switchCase, totemSwitchCasePrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemSwitchCasePrototype_Parse(switchCase, token, tree));
        TOTEM_PARSE_SKIPWHITESPACE(token);
        
        if(first == NULL)
        {
            first = switchCase;
        }
        else
        {
            last->Next = switchCase;
        }
        
        last = switchCase;
    }
    ++token;
    block->CasesStart = first;
    
    return totemParseStatus_Success;
}

totemParseStatus totemSwitchCasePrototype_Parse(totemSwitchCasePrototype *block, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, block);
    
    if((*token)->Type != totemTokenType_Default)
    {
        TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Case);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        TOTEM_PARSE_SKIPWHITESPACE(token);
        
        block->Type = totemSwitchCaseType_Expression;
        TOTEM_PARSE_ALLOC(block->Expression, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(block->Expression, token, tree));
    }
    else
    {
        block->Type = totemSwitchCaseType_Default;
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Colon);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    
    while((*token)->Type != totemTokenType_Break && (*token)->Type != totemTokenType_Case && (*token)->Type != totemTokenType_Default)
    {
        TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseInSet(&firstStatement, &lastStatement, token, tree));
    }
    
    block->StatementsStart = firstStatement;
    
    if((*token)->Type == totemTokenType_Break)
    {
        block->HasBreak = totemBool_True;
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        TOTEM_PARSE_SKIPWHITESPACE(token);
        TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Semicolon);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    else
    {
        block->HasBreak = totemBool_False;
    }
    
    return totemParseStatus_Success;
}

totemParseStatus totemFunctionDeclarationPrototype_Parse(totemFunctionDeclarationPrototype *func, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, func);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Function);
    
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    TOTEM_PARSE_ALLOC(func->Identifier, totemString, tree);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(func->Identifier, token, tree));
    
    totemVariablePrototype *firstParameter = NULL, *lastParameter = NULL;
    TOTEM_PARSE_CHECKRETURN(totemVariablePrototype_ParseParameterList(&firstParameter, &lastParameter, token, tree));
    func->ParametersStart = firstParameter;
    
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    totemStatementPrototype *firstStatement = NULL, *lastStatement = NULL;
    TOTEM_PARSE_CHECKRETURN(totemStatementPrototype_ParseSet(&firstStatement, &lastStatement, token, tree));
    func->StatementsStart = firstStatement;
    
    return totemParseStatus_Success;
}

totemParseStatus totemExpressionPrototype_Parse(totemExpressionPrototype *expression, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, expression);
    TOTEM_PARSE_CHECKRETURN(totemPreUnaryOperatorType_Parse(&expression->PreUnaryOperator, token, tree));
    
    if((*token)->Type == totemTokenType_LBracket)
    {
        TOTEM_PARSE_SKIPWHITESPACE(token);
        expression->LValueType = totemLValueType_Expression;
        TOTEM_PARSE_ALLOC(expression->LValueExpression, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(expression->LValueExpression, token, tree));
        
        TOTEM_PARSE_SKIPWHITESPACE(token);
        TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_RBracket);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    else
    {
        expression->LValueType = totemLValueType_Argument;
        TOTEM_PARSE_ALLOC(expression->LValueArgument, totemArgumentPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemArgumentPrototype_Parse(expression->LValueArgument, token, tree));
    }
    
    TOTEM_PARSE_CHECKRETURN(totemPostUnaryOperatorType_Parse(&expression->PostUnaryOperator, token, tree));
    TOTEM_PARSE_CHECKRETURN(totemBinaryOperatorType_Parse(&expression->BinaryOperator, token, tree));
    
    if(expression->BinaryOperator != totemBinaryOperatorType_None)
    {
        TOTEM_PARSE_ALLOC(expression->RValue, totemExpressionPrototype, tree);
        TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_Parse(expression->RValue, token, tree));
    }
    
    // TODO: reorder with operator precedence
    
    return totemParseStatus_Success;
}

totemParseStatus totemPreUnaryOperatorType_Parse(totemPreUnaryOperatorType *type, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    switch((*token)->Type)
    {
        case totemTokenType_Plus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Plus);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            *type = totemPreUnaryOperatorType_Inc;
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            if((*token)->Type == totemTokenType_Minus)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                *type = totemPreUnaryOperatorType_Dec;
                return totemParseStatus_Success;
            }
            
            *type = totemPreUnaryOperatorType_Negative;
            return totemParseStatus_Success;
            
        case totemTokenType_Not:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            *type = totemPreUnaryOperatorType_LogicalNegate;
            return totemParseStatus_Success;
            
        default:
            *type = totemPreUnaryOperatorType_None;
            return totemParseStatus_Success;
    }
}

totemParseStatus totemPostUnaryOperatorType_Parse(totemPostUnaryOperatorType *type, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    switch((*token)->Type)
    {
        case totemTokenType_Plus:
            if((*token + 1)->Type == totemTokenType_Plus)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                *type = totemPostUnaryOperatorType_Inc;
                return totemParseStatus_Success;
            }
            
            *type = totemPostUnaryOperatorType_None;
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            if((*token + 1)->Type == totemTokenType_Minus)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                *type = totemPostUnaryOperatorType_Dec;
                return totemParseStatus_Success;
            }
            
            *type  = totemPostUnaryOperatorType_None;
            return totemParseStatus_Success;
            
        default:
            *type  = totemPostUnaryOperatorType_None;
            return totemParseStatus_Success;
    }
}

totemParseStatus totemBinaryOperatorType_Parse(totemBinaryOperatorType *type, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    switch((*token)->Type)
    {
        case totemTokenType_Assign:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_Equals;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Assign;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_MoreThan:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_MoreThanEquals;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_MoreThan;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_LessThan:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_LessThanEquals;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_LessThan;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Divide:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_DivideAssign;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Divide;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Minus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_MinusAssign;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Minus;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Multiply:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_MultiplyAssign;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Multiply;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_Plus:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            
            switch((*token)->Type)
            {
                case totemTokenType_Assign:
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                    *type = totemBinaryOperatorType_PlusAssign;
                    break;
                    
                default:
                    *type = totemBinaryOperatorType_Plus;
                    break;
            }
            
            return totemParseStatus_Success;
            
        case totemTokenType_And:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_And);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            *type = totemBinaryOperatorType_LogicalAnd;
            return totemParseStatus_Success;
            
        case totemTokenType_Or:
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Or);
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            *type = totemBinaryOperatorType_LogicalOr;
            return totemParseStatus_Success;
            
        default:
            *type = totemBinaryOperatorType_None;
            return totemParseStatus_Success;
    }
}

totemParseStatus totemVariablePrototype_Parse(totemVariablePrototype *variable, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, variable);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Variable);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(&variable->Identifier, token, tree));
    TOTEM_PARSE_SKIPWHITESPACE(token);
    
    return totemParseStatus_Success;
}

totemParseStatus totemFunctionCallPrototype_Parse(totemFunctionCallPrototype *call, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_CHECKRETURN(totemString_ParseIdentifier(&call->Identifier, token, tree));
    
    totemExpressionPrototype *first = NULL, *last = NULL;
    TOTEM_PARSE_CHECKRETURN(totemExpressionPrototype_ParseParameterList(&first, &last, token, tree));
    call->ParametersStart = first;
    return totemParseStatus_Success;
}

totemParseStatus totemArgumentPrototype_Parse(totemArgumentPrototype *argument, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_COPYPOSITION(token, argument);
    
    // variable
    if((*token)->Type == totemTokenType_Variable)
    {
        argument->Type = totemArgumentType_Variable;
        TOTEM_PARSE_ALLOC(argument->Variable, totemVariablePrototype, tree);
        return totemVariablePrototype_Parse(argument->Variable, token, tree);
    }
    
    // number constant
    if((*token)->Type == totemTokenType_Number)
    {
        char numberBuffer[128];
        memset(numberBuffer, 0, sizeof(numberBuffer));
        size_t size = 0;
        
        while((*token)->Type == totemTokenType_Number || (*token)->Type == totemTokenType_Dot)
        {
            if(size > sizeof(numberBuffer) - 1)
            {
                return totemParseStatus_ValueTooLarge;
            }
            
            numberBuffer[size] = (*token)->Value.Value[0];
            TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        }
        
        (*token)--;
        TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Number);
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        
        numberBuffer[size] = '\0';
        argument->Number = atof(numberBuffer);
        argument->Type = totemArgumentType_Number;
        
        return totemParseStatus_Success;
    }
    
    // string constant
    if((*token)->Type == totemTokenType_DoubleQuote)
    {
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        
        argument->Type = totemArgumentType_String;
        const char *begin = (*token)->Value.Value;
        uint32_t len = 0;
        
        while((*token)->Type != totemTokenType_DoubleQuote)
        {
            len += (*token)->Value.Length;
            
            if((*token)->Type == totemTokenType_Backslash)
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                
                if((*token)->Type == totemTokenType_DoubleQuote)
                {
                    len += (*token)->Value.Length;
                    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
                }
            }
            else
            {
                TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
            }
        }
        
        argument->String.Value = begin;
        argument->String.Length = len;
        
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        return totemParseStatus_Success;
    }
    
    // function call
    if((*token)->Type == totemTokenType_Identifier)
    {
        argument->Type = totemArgumentType_FunctionCall;
        TOTEM_PARSE_ALLOC(argument->FunctionCall, totemFunctionCallPrototype, tree);
        return totemFunctionCallPrototype_Parse(argument->FunctionCall, token, tree);
    }
    
    // bool constant
    if((*token)->Type == totemTokenType_True || (*token)->Type == totemTokenType_False)
    {
        argument->Type = totemArgumentType_Number;
        argument->Number = (*token)->Type == totemTokenType_True;
        
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
        TOTEM_PARSE_SKIPWHITESPACE(token);
        return totemParseStatus_Success;
    }
    
    return totemParseStatus_UnexpectedToken;
}

totemParseStatus totemString_ParseIdentifier(totemString *string, totemToken **token, totemParseTree *tree)
{
    TOTEM_PARSE_SKIPWHITESPACE(token);
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Identifier);
    
    const char *start = (*token)->Value.Value;
    uint32_t length = 0;
    
    while((*token)->Type == totemTokenType_Identifier || (*token)->Type == totemTokenType_Colon)
    {
        length += (*token)->Value.Length;
        TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    }
    
    string->Length = length;
    string->Value = start;
    
    (*token)--;
    TOTEM_PARSE_ENFORCETOKEN(token, totemTokenType_Identifier);
    TOTEM_PARSE_INC_NOT_ENDSCRIPT(token);
    
    return totemParseStatus_Success;
}