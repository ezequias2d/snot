#include <malloc.h>
#include <snot.h>
#include <stdio.h>

void *__grow(void *memory, size_t *size, size_t grow_size)
{
    *size = *size + (((grow_size + 7) >> 3) << 3);
    return realloc(memory, *size);
}

void __start_section_xml(SNOT_PARSER *parser, size_t id, void *userdata)
{
    const char *name;
    snot_value(parser, id, &name, NULL);
    printf("<%s>\n", name);
}

void __end_section_xml(SNOT_PARSER *parser, size_t id, void *userdata)
{
    const char *name;
    snot_value(parser, id, &name, NULL);
    printf("</%s>\n", name);
}

void __string_xml(SNOT_PARSER *parser, size_t id, void *userdata)
{
    const char *str;
    snot_value(parser, id, &str, NULL);
    printf("%s\n", str);
}

void __number_xml(SNOT_PARSER *parser, size_t id, void *userdata)
{
    const char *lexeme;
    snot_value(parser, id, &lexeme, NULL);
    printf("%s\n", lexeme);
}

int main(void)
{
    SNOT_PARSER *parser;
    SNOT_CALLBACKS cx;
    void *userdata = NULL;
    size_t file_size;
    const char *filename = "example5.snot";

    cx.alloc         = malloc;
    cx.free          = free;
    cx.grow          = __grow;
    cx.start_section = __start_section_xml;
    cx.end_section   = __end_section_xml;
    cx.string        = __string_xml;
    cx.number        = __number_xml;

    parser = snot_create(cx, userdata);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("cannot open %s\n", filename);
        return -1;
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    while (file_size--)
    {
        // ASCII
        char c;
        fread(&c, sizeof(char), 1, file);

        // fill the parser each with character at a time
        SNOT_RESULT result = snot_parse(parser, c);
        switch (result)
        {
        case SNOT_OK:
            break;
        case SNOT_ERROR_NO_MEMORY:
        case SNOT_ERROR_INVALID_CHARACTER:
        case SNOT_ERROR_PARTIAL:
        case SNOT_ERROR_TOKEN_TYPE_UNDEFINED:
            printf("Error at %c(code: %d)\n", c, result);
            file_size = 0;
            break;
        }
    }
    fclose(file);

    snot_end(parser);
    snot_free(parser);

    return 0;
}
