#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"
struct compile_process *compile_process_create(const char *filename, const char *filename_out, int flags)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("open input file error\n");
        return NULL;
    }
    FILE *out_file = NULL;
    if (filename_out != NULL)
    {
        out_file = fopen(filename_out, "w");
        if (out_file == NULL)
        {
            fclose(file); // wangss added.
            printf("open output file error\n");
            return NULL;
        }
    }
    struct compile_process *process = calloc(1, sizeof(struct compile_process));
    process->token_vec = vector_create(sizeof(struct token));
    process->node_vec = vector_create(sizeof(struct node *));
    process->node_tree_vec = vector_create(sizeof(struct node *));

    process->flags = flags;
    process->ofile = out_file;
    process->cfile.fp = file;

    process->cfile.abs_path = malloc(strlen(filename) + 1);        // wangss added.
    memcpy(&process->cfile.abs_path, &filename, strlen(filename)); // wangss added.

    node_set_vector(process->node_vec, process->node_tree_vec);

    return process;
};

char compile_process_next_char(struct lex_process *lex_process)
{
    struct compile_process *compiler = lex_process->compiler;
    compiler->pos.col += 1;
    char c = getc(compiler->cfile.fp);
    if (c == '\n')
    {
        compiler->pos.line += 1;
        compiler->pos.col = 1;
    }

    return c;
}

char compile_process_peek_char(struct lex_process *lex_process)
{
    struct compile_process *compiler = lex_process->compiler;
    char c = getc(compiler->cfile.fp);
    ungetc(c, compiler->cfile.fp);
    return c;
}

void compile_process_push_char(struct lex_process *lex_process, char c)
{
    struct compile_process *compiler = lex_process->compiler;
    ungetc(c, compiler->cfile.fp);
}
