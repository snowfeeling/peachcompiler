#include "compiler.h"
#include "helpers/vector.h"

#// Wangss add - start.
char *TokenTypeStrings[] = {
    "TOKEN_TYPE_IDENTIFIER",
    "TOKEN_TYPE_KEYWORD",
    "TOKEN_TYPE_OPERATOR",
    "TOKEN_TYPE_SYMBOL",
    "TOKEN_TYPE_NUMBER",
    "TOKEN_TYPE_STRING",
    "TOKEN_TYPE_COMMENT",
    "TOKEN_TYPE_NEWLINE"};

char *NodeTypeStrings[] = {
    "NODE_TYPE_EXPRESSION",
    "NODE_TYPE_EXPRESSION_PARENTHESES",
    "NODE_TYPE_NUMBER",
    "NODE_TYPE_IDENTIFIER",
    "NODE_TYPE_STRING",
    "NODE_TYPE_VARIABLE",
    "NODE_TYPE_VARIABLE_LIST",
    "NODE_TYPE_FUNCTION",
    "NODE_TYPE_BODY",
    "NODE_TYPE_STATEMENT_RETURN",
    "NODE_TYPE_STATEMENT_IF",
    "NODE_TYPE_STATEMENT_ELSE",
    "NODE_TYPE_STATEMENT_WHILE",
    "NODE_TYPE_STATEMENT_DO_WHILE",
    "NODE_TYPE_STATEMENT_FOR",
    "NODE_TYPE_STATEMENT_BREAK",
    "NODE_TYPE_STATEMENT_CONTINUE",
    "NODE_TYPE_STATEMENT_SWITCH",
    "NODE_TYPE_STATEMENT_CASE",
    "NODE_TYPE_STATEMENT_DEFAULT",
    "NODE_TYPE_STATEMENT_GOTO",
    "NODE_TYPE_UNARY",
    "NODE_TYPE_TENARY",
    "NODE_TYPE_LABEL",
    "NODE_TYPE_STRUCT",
    "NODE_TYPE_UNION",
    "NODE_TYPE_BRACKET",
    "NODE_TYPE_CAST",
    "NODE_TYPE_BLANK"};

int showTokens(struct lex_process *process)
{
    struct token *token;
    printf("Lexica report starting...\nTOKEN COUNT: %d\n", process->token_vec->count);
    for (int i = 0; i < process->token_vec->count; i++)
    {
        token = vector_at(process->token_vec, i);
        printf("TOKEN[%3d] [%d-%-25s] ", i + 1, token->type, TokenTypeStrings[token->type]);
        switch (token->type)
        {
        case TOKEN_TYPE_NUMBER:
            printf("%d\n", token->llnum);
            break;
        case TOKEN_TYPE_STRING:
            printf("%s\n", token->sval);
            break;
        case TOKEN_TYPE_IDENTIFIER:
            printf("%s\n", token->sval);
            break;
        case TOKEN_TYPE_KEYWORD:
            printf("%s\n", token->sval);
            break;
        case TOKEN_TYPE_OPERATOR:
            printf("%s\n", token->sval);
            break;
        case TOKEN_TYPE_SYMBOL:
            printf("%c\n", token->cval);
            break;
        case TOKEN_TYPE_COMMENT:
            printf("%s\n", token->sval);
            break;
        case TOKEN_TYPE_NEWLINE:
            printf("%s\n", token->sval);
            break;
        default:
            printf("[Error:UNKNOWN]%d\n", token->type);
            break;
        }
    }
    printf("Lexical report end.\n");
}
void showOneNode(struct node *node)
{
    if (!node)
        return;
    // printf("Node[%d-%-25s]", node->type, NodeTypeStrings[node->type]);
    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        showOneNode(node->exp.left);
        printf("[%s]", node->exp.op);
        showOneNode(node->exp.right);
        printf("\n");
        break;
    case NODE_TYPE_IDENTIFIER:
        printf("[%s]", node->sval);
        printf("\n");
        break;
    case NODE_TYPE_STRING:
        break;
    case NODE_TYPE_NUMBER:
        printf("[%d]", node->llnum);
        break;
    default:
        break;
    }
}
void showNodesFromRoot(struct vector *node_vec)
{
    struct node *node = NULL;
    void **ptr = NULL;
    ptr = vector_at(node_vec, 0);
    node = *ptr;
    showOneNode(node);
}
// Wangss add - end.
