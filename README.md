# SNOT

Stack, Simple, Stupid, Straightforward or Silly  Notation is a stack-based human-readable data-serialization language. It is inspired by XML, but aims to be simple(maybe it didn't work).

**WIP: Missing implementation of string escape sequence greater than one character**

## Syntax

I was just thinking about trying to use as few symbols (or anything else as keywords) to represent things, so the language has no reserved keyword.

Therefore, any sequence of valid unicode characters that are not whitespaces, symbols used by language or starting with an arabic digit is an 'identifier', 'string' are enclosed in double quotes and support C-like escape sequence and has semantically the same meaning as identifiers, 'number' starts with a arabic digit and need to be in a valid format.

Ooh, whitespaces separate only the tokens, so indentation is not necessary if you do not want to use.

### Identifier

```
each word in this phase is an identifier and this phrase must be recognized as a valid SNOT
```

### String
```
"yeah, it's just ordinary string syntax\nHello World!" \
"but I thought of this syntax to make a multiline string"
```
| Escape sequence |     Description    |    Representation     |
|:---------------:|--------------------|:---------------------:|
|`\a`             | audible beil       | 0x07 in ASCII         |
|`\b`             | backspace          | 0x08 in ASCII         |
|`\e`             | escape character   | 0x1B in ASCII         |
|`\f`             | page break         | 0x0C in ASCII         |
|`\n`             | newline            | 0x0A in ASCII         |
|`\r`             | carriage return    | 0x0D in ASCII         |
|`\t`             | horizontal tab     | 0x09 in ASCII         |
|`\v`             | vertical tab       | 0x0B in ASCII         |
|`\\`             | backslash          | 0x5C in ASCII         |
|`\"`             | double quote       | 0x22 in ASCII         |
|`\?`             | question mark      | 0x3F in ASCII         |
|`\nnn`           | octal byte         | any                   |
|`\xhh`           | hexadecimal byte   | any                   |
|`\uhhhh`         | unicode code point | code point U+nnnn     |
|`\uhhhhhhhh`     | unicode code point | code point U+nnnnnnnn |

### Number
```
decimal     116104971163911532973211011710998101114
octal       0157143164141154
hexadecimal 0x706172652064652070656E73617220717565206EE36F
real        10210811197116.63
```
### Symbols

The symbols used is:
```
, stack pop
; two stack pop
. three stack pop
( stack mark
) stack pop until stack mark
\ continue last stack token
```

### Example

So with all of this you can write something like this:

```
glossary
  title "example glossary";
  GlossDiv
    title S;
    GlossEntry
      ID SGML;
      SortAs SGML;
      GlossTerm "Standard Generalized Markup Language";
      Acronym SGML;
      Abbrev "ISO 8879:1986";
      GlossDef 
        para "A meta-markup language, used to create markup languages such as DocBook.";
        GlossSeeAlso GML, XML.
      GlossSee markup
```

Stack starts emtpy, when it finds any value token(identifier, string or number) its pushed into stack, but if stack peek token are a identifier or string(number is invalid), the previous token is turned into a "section" token, any tokens inserted after the section token in stack are inside that section and sections maybe inside other sections.

When finds a comma, semicolon or a period symbol the last 1, 2 or 3, respectively, tokens are popped from stack, soo you can insert more values into a section without create new sections.

But if 1, 2 or 3 pop is not enough for you, you can drop everything and use parentheses. The '(' symbol will add a token into the stack that is ignored by the rule that generates sections and ')' will pops until reaching the '(' token.

```
web-app
  (servlet
    servlet-name cofaxCDS;
    servlet-class "org.cofax.cds.CDSServlet";
    init-param
      configGlossary:installationAt "Philadelphia, PA";
      configGlossary:adminEmail "ksm@pobox.com";
  )
  (serverlet
    serverlet-name cofaxEmail;
    servlet-class "org.cofax.cds.EmailServlet";
    init-param
      mailHost mail1;
      mailHostOverride mail2
  )
  (serverlet
    serverlet-name cofaxAdmin;
    servlet-class "org.cofax.cds.AdminServlet"
  )
```

## Usage

This library is self contained in a single header file and can be used either in header only mode or in implementation mode. The header only mode is the default when included and allows including this header in other headers and does not contain the implementation.

The implementation mode requires the macro `SNOT_IMPLEMENTATION` into .c file before `#include`:
```C
#define SNOT_IMPLEMENTATION
#include "snot.h" /* or "snot.h" */
```

To make the implementation private to the file that generates the implementation:
```C
#define SNOT_STATIC
#define SNOT_IMPLEMENTATION
#include "snot.h" /* or "snot.h" */
```

Sample code

```C
#define SNOT_IMPLEMENTATION
#include "snot.h"

#include <malloc.h>
#include <stdio.h>

void *__grow(void *memory, size_t *size, size_t grow_size);
void __start_section(SNOT_PARSER *parser, size_t id, void *userdata);
void __end_section(SNOT_PARSER *parser, size_t id, void *userdata);
void __string(SNOT_PARSER *parser, size_t id, void *userdata);
void __number(SNOT_PARSER *parser, size_t id, void *userdata);

int main(void) 
{
    SNOT_PARSER *parser;
    SNOT_CALLBACKS cx;
    void *userdata = NULL;
    size_t file_size;
    const char *filename = "document.snot";

    cx.alloc         = malloc;
    cx.free          = free;
    cx.grow          = __grow;
    cx.start_section = __start_section;
    cx.end_section   = __end_section;
    cx.string        = __string;
    cx.number        = __number;

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
```

## License
[MIT](https://choosealicense.com/licenses/mit/)
