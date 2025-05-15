#include "compiler.h"
#include "helpers/vector.h"
#include "helpers/buffer.h"
#include <string.h>
#include <assert.h>
#include <ctype.h>

#define LEX_GETC_IF(buffer, c, exp)     \
    for (c = peekc(); exp; c = peekc()) \
    {                                   \
        buffer_write(buffer, c);        \
        nextc();                        \
    }

struct token *read_next_token();
bool lex_is_in_expression();

static struct lex_process *lex_process;
static struct token tmp_token;

static char peekc()
{
    return lex_process->function->peek_char(lex_process);
}
static char nextc()
{
    char c = lex_process->function->next_char(lex_process);
    if (lex_is_in_expression())
    {
        buffer_write(lex_process->parentheses_buffer, c);
    }
    lex_process->pos.col += 1;
    if (c == '\n')
    {
        lex_process->pos.line += 1;
        lex_process->pos.col = 1;
    }
    return c;
}
static void pushc(char c)
{
    lex_process->function->push_char(lex_process, c);
}
bool is_keyword(const char *str)
{
    return S_EQ(str, "unsigned") ||
           S_EQ(str, "signed") ||
           S_EQ(str, "char") ||
           S_EQ(str, "short") ||
           S_EQ(str, "int") ||
           S_EQ(str, "float") ||
           S_EQ(str, "double") ||
           S_EQ(str, "long") ||
           S_EQ(str, "void") ||
           S_EQ(str, "struct") ||
           S_EQ(str, "union") ||
           S_EQ(str, "static") ||
           S_EQ(str, "__ignore_typecheck__") ||
           S_EQ(str, "return") ||
           S_EQ(str, "include") ||
           S_EQ(str, "sizeof") ||
           S_EQ(str, "if") ||
           S_EQ(str, "else") ||
           S_EQ(str, "while") ||
           S_EQ(str, "for") ||
           S_EQ(str, "do") ||
           S_EQ(str, "break") ||
           S_EQ(str, "continue") ||
           S_EQ(str, "switch") ||
           S_EQ(str, "case") ||
           S_EQ(str, "default") ||
           S_EQ(str, "goto") ||
           S_EQ(str, "typedef") ||
           S_EQ(str, "const") ||
           S_EQ(str, "extern") ||
           S_EQ(str, "restrict");
}
static struct pos lex_file_position()
{
    return lex_process->pos;
}

static char assert_next_char(char expected_char)
{
    char c = nextc();
    assert(c == expected_char);
    return c;
}
struct token *token_create(struct token *_token)
{
    memcpy(&tmp_token, _token, sizeof(struct token));
    tmp_token.pos = lex_file_position();
    if (lex_is_in_expression())
    {
        tmp_token.between_brackets = buffer_ptr(lex_process->parentheses_buffer);
    }
    return &tmp_token;
};
static struct token *lexer_last_token()
{
    return vector_back_or_null(lex_process->token_vec);
}
static struct token *handle_whitespace()
{
    struct token *last_token = lexer_last_token();
    if (!last_token)
    {
        last_token->whitespace = true;
    }
    nextc();
    return read_next_token();
}

const char *read_number_str()
{
    const char *num = NULL;
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9'));

    buffer_write(buffer, 0x00);
    return buffer_ptr(buffer);
}

unsigned long long read_number()
{
    const char *s = read_number_str();
    return atoll(s);
}

int lexer_number_type(char c)
{
    int res = NUMBER_TYPE_NORMAL;
    c = toupper(c);
    if (c == 'L')
    {
        res = NUMBER_TYPE_LONG;
    }
    else if (c == 'F')
    {
        res = NUMBER_TYPE_FLOAT;
    }

    return res;
}
struct token *token_make_number_for_value(unsigned long number)
{
    int number_type = lexer_number_type(peekc());
    if (number_type != NUMBER_TYPE_NORMAL)
    {
        nextc();
    }
    return token_create(&(struct token){.type = TOKEN_TYPE_NUMBER, .llnum = number, .num.type = number_type});
}

struct token *tokern_make_number()
{
    return token_make_number_for_value(read_number());
}

static void lex_handle_escape_number(struct buffer *buf)
{
    long long number = read_number();
    if (number > 255)
    {
        compiler_error(lex_process->compiler, "Characters must be betwene 0-255 wide chars are not yet supported");
    }
    buffer_write(buf, number);
}

char lex_get_escaped_char(char c)
{
    char co = 0x00;
    switch (c)
    {
    case 'n':
        // New line escape?
        co = '\n';
        break;
    case '\\':
        co = '\\';
    case 't':
        co = '\t';
        break;
    case '\'':
        co = '\'';
        break;
    default:
        compiler_error(lex_process->compiler, "Unknown escape token %c\n", c);
        break;
    }
    return co;
}
static void lex_handle_escape(struct buffer *buf)
{
    char c = peekc();
    if (isdigit(c))
    {
        // We have a number?
        lex_handle_escape_number(buf);
        return;
    }

    char co = lex_get_escaped_char(c);

    buffer_write(buf, co);
    // Pop off the char
    nextc();
}
static struct token *token_make_string(char start_delim, char end_delim)
{
    struct buffer *buf = buffer_create();
    assert(nextc() == start_delim);
    char c = nextc();
    for (; c != end_delim && c != EOF; c = nextc())
    {
        if (c == '\\')
        {
            // We need handle an escape character
            lex_handle_escape(buf);
            continue;
        }
        buffer_write(buf, c);
    }
    buffer_write(buf, 0x00);
    return token_create(&(struct token){.type = TOKEN_TYPE_STRING, .sval = buffer_ptr(buf)});
}
static void lex_new_expression()
{
    lex_process->current_expression_count++;
    if (lex_process->current_expression_count == 1)
    {
        lex_process->parentheses_buffer = buffer_create();
    }
}
void lex_finish_expression()
{
    lex_process->current_expression_count--;
    if (lex_process->current_expression_count < 0)
    {
        compiler_error(lex_process->compiler, "You closed an expression that you never opened\n");
    }
}
bool lex_is_in_expression()
{
    return lex_process->current_expression_count > 0;
}
bool keyword_is_datatype(const char *str)
{
    // Wangss changed - start
    bool b = S_EQ(str, "void") ||
           S_EQ(str, "char") ||
           S_EQ(str, "int") ||
           S_EQ(str, "short") ||
           S_EQ(str, "float") ||
           S_EQ(str, "double") ||
           S_EQ(str, "long") ||
           S_EQ(str, "struct") ||
           S_EQ(str, "union");
    return b;
    // Wangss changed - end
}
static bool op_treated_as_one(char op)
{
    return op == '(' || op == '[' || op == ',' || op == '.' || op == '*' || op == '?';
}

static bool is_single_operator(char op)
{
    return op == '+' ||
           op == '-' ||
           op == '/' ||
           op == '*' ||
           op == '=' ||
           op == '>' ||
           op == '<' ||
           op == '|' ||
           op == '&' ||
           op == '^' ||
           op == '%' ||
           op == '~' ||
           op == '!' ||
           op == '(' ||
           op == '[' ||
           op == ',' ||
           op == '.' ||
           op == '~' ||
           op == '?';
}
static bool op_valid(const char *op)
{
    return S_EQ(op, "+") ||
           S_EQ(op, "-") ||
           S_EQ(op, "*") ||
           S_EQ(op, "/") ||
           S_EQ(op, "!") ||
           S_EQ(op, "^") ||
           S_EQ(op, "+=") ||
           S_EQ(op, "-=") ||
           S_EQ(op, "*=") ||
           S_EQ(op, ">>=") ||
           S_EQ(op, "<<=") ||
           S_EQ(op, "/=") ||
           S_EQ(op, ">>") ||
           S_EQ(op, "<<") ||
           S_EQ(op, ">=") ||
           S_EQ(op, "<=") ||
           S_EQ(op, ">") ||
           S_EQ(op, "<") ||
           S_EQ(op, "||") ||
           S_EQ(op, "&&") ||
           S_EQ(op, "|") ||
           S_EQ(op, "&") ||
           S_EQ(op, "++") ||
           S_EQ(op, "--") ||
           S_EQ(op, "=") ||
           S_EQ(op, "*=") ||
           S_EQ(op, "^=") ||
           S_EQ(op, "==") ||
           S_EQ(op, "!=") ||
           S_EQ(op, "->") ||
           S_EQ(op, "**") ||
           S_EQ(op, "(") ||
           S_EQ(op, "[") ||
           S_EQ(op, ",") ||
           S_EQ(op, ".") ||
           S_EQ(op, "...") ||
           S_EQ(op, "~") ||
           S_EQ(op, "?") ||
           S_EQ(op, "%");
}
/**
 * Flushes the buffer back to the character stream, except the first operator.
 * Ignores the null terminator
 */
void read_op_flush_back_keep_first(struct buffer *buffer)
{
    const char *data = buffer_ptr(buffer);
    int len = buffer->len;
    for (int i = len - 1; i >= 1; i--)
    {
        if (data[i] == 0x00)
            continue;
        pushc(data[i]);
    }
}
static const char *read_op()
{
    bool single_operator = true;
    char op = nextc();
    struct buffer *buffer = buffer_create();
    buffer_write(buffer, op);
    if (op == '*' && peekc() == '=')
    {
        // *= so it  may be that * is a single operator but we will give it a special use case.
        buffer_write(buffer, peekc());
        // Skip "=" as we just peeked at it.
        nextc();
        single_operator = false;
    }
    else if (!op_treated_as_one(op))
    {
        for (int i = 0; i < 2; i++)
        {
            op = peekc();
            if (is_single_operator(op))
            {
                buffer_write(buffer, op);
                nextc();
                single_operator = false;
            }
        }
    }

    // NULL TERMINATOR
    buffer_write(buffer, 0x00);
    char *ptr = buffer_ptr(buffer);
    if (!single_operator)
    {
        if (!op_valid(ptr))
        {
            read_op_flush_back_keep_first(buffer);
            ptr[1] = 0x00;
        }
    }
    else if (!op_valid(ptr))
    {
        compiler_error(lex_process->compiler, "The operator %s is not valid\n", ptr);
    }

    return ptr;
}

static struct token *token_make_operator_for_value(const char *val)
{
    return token_create(&(struct token){TOKEN_TYPE_OPERATOR, .sval = val});
}

static struct token *token_make_operator_or_string()
{
    char op = peekc();
    if (op == '<')
    {
        // It's possible we have an include statement lets just check that if so we will have a string
        struct token *last_token = lexer_last_token();
        if (token_is_keyword(last_token, "include"))
        {
            // Aha so we have something like this "include <stdio.h>"
            // We are at the "stdio.h>" bit so we need to treat this as a string
            return token_make_string('<', '>');
        }
    }
    struct token *token = token_create(&(struct token){TOKEN_TYPE_OPERATOR, .sval = read_op()});
    if (op == '(')
    {
        lex_new_expression();
    }

    return token;
};
static struct token *token_make_symbol()
{
    char c = nextc();
    if (c == ')')
    {
        lex_finish_expression();
    }
    nextc();
    struct token *token = token_create(&(struct token){TOKEN_TYPE_SYMBOL, .cval = c});

    return token;
}

static struct token *token_make_identifier_or_keyword()
{
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
    // Null terminator.
    buffer_write(buffer, 0x00);

    if (is_keyword(buffer_ptr(buffer)))
    {
        return token_create(&(struct token){TOKEN_TYPE_KEYWORD, .sval = buffer_ptr(buffer)});
    }

    return token_create(&(struct token){TOKEN_TYPE_IDENTIFIER, .sval = buffer_ptr(buffer)});
}
static struct token *read_token_special()
{
    char c = peekc();
    if (isalpha(c) || c == '_')
    {
        return token_make_identifier_or_keyword();
    }

    return NULL;
}

struct token *token_make_one_line_comment()
{
    struct buffer *buffer = buffer_create();
    char c = 0;
    LEX_GETC_IF(buffer, c, c != '\n' && c != EOF);

    return token_create(&(struct token){TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}
static struct token *token_make_multiline_comment()
{
    struct buffer *buffer = buffer_create();
    char c = 0;
    while (1)
    {
        LEX_GETC_IF(buffer, c, c != '*' && c != EOF);
        if (c == EOF)
        {
            // End of file.. but this comment is not closed.
            // Something bad here, it should not be allowed
            compiler_error(lex_process->compiler, "You did not close the mulitiline comment.\n");
        }
        if (c == '*')
        {
            // Skip the *
            nextc();

            // We must check to see if this is truly the end of this multi-line comment
            // if it is then we should see a forward slash as the next character.
            if (peekc() == '/')
            {
                // Da, it is
                nextc();
                break;
            }
        }
    }

    return token_create(&(struct token){TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}
static struct token *handle_comment()
{
    char c = peekc();
    if (c == '/')
    {
        nextc();
        if (peekc() == '/')
        {
            nextc();
            // This is a comment token.
            return token_make_one_line_comment();
        }
        else if (peekc() == '*')
        {
            nextc();
            return token_make_multiline_comment();
        }

        // This is not a comment it must just be a normal division operator
        // Let's push back to the input stream the division symbol
        // so that it can be proceesed correctly by the operator token function
        pushc('/');

        return token_make_operator_or_string();
    }

    return NULL;
}
static struct token *token_make_newline()
{
    nextc();
    return token_create(&(struct token){.type = TOKEN_TYPE_NEWLINE});
}
void lexer_pop_token()
{
    vector_pop(lex_process->token_vec);
}
bool is_hex_char(char c)
{
    c = tolower(c);
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

const char *read_hex_number_str()
{
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, is_hex_char(c));
    // Write our null terminator
    buffer_write(buffer, 0x00);
    // Wangss add - start.
    c = tolower(c);
    if (!is_hex_char(c) && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')))
    {
        compiler_warning(lex_process->compiler, "The character %c is not a valid hexadecimal character\n", c);
    }
    // Wangss add - end.

    return buffer_ptr(buffer);
}
struct token *token_make_special_number_hexadecimal()
{
    // Skip the "x"
    nextc();

    unsigned long number = 0;
    const char *number_str = read_hex_number_str();
    number = strtoul(number_str, 0, 16);
    return token_make_number_for_value(number);
}
void lexer_validate_binary_string(const char *str)
{
    size_t len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        if (str[i] != '0' && str[i] != '1')
        {
            compiler_error(lex_process->compiler, "The character %c is not a valid binary character\n", str[i]);
        }
    }
}
struct token *token_make_special_number_binary()
{
    // Skip the "b"
    nextc();

    unsigned long number = 0;
    const char *number_str = read_number_str();
    lexer_validate_binary_string(number_str);
    number = strtol(number_str, 0, 2);
    return token_make_number_for_value(number);
}
struct token *token_make_special_number()
{
    struct token *token = NULL;
    struct token *last_token = lexer_last_token();
    if (!last_token || !(last_token->type == TOKEN_TYPE_NUMBER && last_token->llnum == 0))
    {
        // This must be an identifier since we have no last token
        // or that last number is not zero
        return token_make_identifier_or_keyword();
    }
    lexer_pop_token();
    char c = peekc();
    if (c == 'x')
    {
        token = token_make_special_number_hexadecimal();
    }
    else if (c == 'b')
    {
        token = token_make_special_number_binary();
    }
    return token;
}
static struct token *token_make_quote()
{
    assert_next_char('\'');
    char c = nextc();
    if (c == '\\')
    {
        // We have an escape here.
        c = nextc();
        c = lex_get_escaped_char(c);
    }

    assert_next_char('\'');
    // Characters are basically just small numbers. Treat it as such.
    return token_create(&(struct token){TOKEN_TYPE_NUMBER, .cval = c});
}

struct token *read_next_token()
{
    struct token *token = NULL;
    char c = peekc();

    token = handle_comment();
    if (token)
    {
        // This was a comment as we now have a comment token
        return token;
    }

    switch (c)
    {
    NUMBER_CASE:
        token = tokern_make_number();
        break;
    OPERATOR_CASE_EXCLUDING_DIVISON:
        token = token_make_operator_or_string();
        break;
    SYMBOL_CASE:
        token = token_make_symbol();
        break;
    case 'b':
    case 'x':
        token = token_make_special_number();
        break;
    case '"':
        token = token_make_string('"', '"');
        break;
    case '\'':
        token = token_make_quote();
        break;
    case ' ':
    case '\t':
        token = handle_whitespace();
        break;
    case '\n':
        token = token_make_newline();
        break;
    case EOF:
        // we have finished lexical analysis on this file
        break;
    default:
        token = read_token_special();
        if (!token)
        {
            compiler_error(lex_process->compiler, "Unexcepted token\n");
        }
        break;
    }

    return token;
};
int lex(struct lex_process *process)
{
    process->current_expression_count = 0;
    process->parentheses_buffer = NULL;
    lex_process = process;
    process->pos.filename = process->compiler->cfile.abs_path;

    struct token *token = read_next_token();
    while (token)
    {
        vector_push(process->token_vec, token);
        token = read_next_token();
    };

    // Wangss add - start. For showing tokens.
    showTokens(process);
    // Wangss add - end.
    return LEXICAL_ANALYSIS_ALL_OK;
}

char lexer_string_buffer_next_char(struct lex_process *process)
{
    struct buffer *buf = lex_process_private(process);
    return buffer_read(buf);
}

char lexer_string_buffer_peek_char(struct lex_process *process)
{
    struct buffer *buf = lex_process_private(process);
    return buffer_peek(buf);
}

void lexer_string_buffer_push_char(struct lex_process *process, char c)
{
    struct buffer *buf = lex_process_private(process);
    buffer_write(buf, c);
}

struct lex_process_functions lexer_string_buffer_functions = {
    .next_char = lexer_string_buffer_next_char,
    .peek_char = lexer_string_buffer_peek_char,
    .push_char = lexer_string_buffer_push_char};

struct lex_process *tokens_build_for_string(struct compile_process *compiler, const char *str)
{
    struct buffer *buffer = buffer_create();
    buffer_printf(buffer, str);
    struct lex_process *lex_process = lex_process_create(compiler, &lexer_string_buffer_functions, buffer);
    if (!lex_process)
    {
        return NULL;
    }

    if (lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK)
    {
        return NULL;
    }

    return lex_process;
}