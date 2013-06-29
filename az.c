/*

MIT/X11 License
Copyright (c) 2013 Sean Pringle <sean.pringle@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <err.h>

#define FORMAT "%d"
#define AZ 26
#define MARKS 1024
#define CHAINS AZ
#define NAME AZ
#define BLOCK 1024
#define CELL sizeof(cell)

#define assert(c,e) if (!(c)) \
	errx((e), "abort (%s %d): %s", __FILE__, __LINE__, errmsg[(e)])

enum {
	ERR_MEMORY=1,
	ERR_MARKED,
	ERR_NOMARK,
	ERR_POPEN,
	ERR_BOUNDS,
	ERR_CHARACTER,
	ERR_NAME
};

char *errmsg[] = {
	[ERR_MEMORY]    = "memory allocation failed",
	[ERR_MARKED]    = "mark stack overflow",
	[ERR_NOMARK]    = "mark stack underflow",
	[ERR_POPEN]     = "start subprocess failed",
	[ERR_BOUNDS]    = "memory out of bounds",
	[ERR_CHARACTER] = "unexpected character",
	[ERR_NAME]      = "variable name too long"
};

typedef int32_t cell;

typedef struct _Name {
	char name[NAME];
	cell value;
	struct _Name *next;
} Name;

char *sourcecode;
Name *names[CHAINS];
cell globals[AZ];
cell *current, *previous;
FILE *input;
char *buffer;
char *marks[MARKS];
int marked = 0;

// find or create a global named variable

Name*
find(const char *name, uint32_t len)
{
	uint32_t hash = 0, i = 0, chain;
	while (i < len) hash = 33 * hash + name[i++];

	chain = hash % CHAINS;
	Name *n = names[chain];

	while (n && (strncmp(name, n->name, len) || n->name[len]))
		n = n->next;

	if (!n)
	{
		assert(len < NAME, ERR_NAME);
		assert((n = calloc(1, sizeof(Name))), ERR_MEMORY);
		strncpy(n->name, name, len);
		n->name[len] = 0;
		n->next = names[chain];
		names[chain] = n;
		n->value = 0;
	}
	return n;
}

// scan to the end of a source block

char*
scan(char *source, char open, char close)
{
	int levels = 1; char c;
	while (levels > 0 && (c = *source))
	{
		source++;
		if (open && c == open) levels++;
		else if (close && c == close) levels--;
	}
	return source;
}

// revert input to stdin

void
input_stdin()
{
	if (input && input != stdin)
		pclose(input);
	input = stdin;
}

// parse and process a function

cell
interpret(char *source, cell *outer)
{
	cell *inner, working, done = 0, mem = AZ;
	char c; int markers = marked;
	assert((inner = calloc(mem, CELL)), ERR_MEMORY);

	while (!done && source && (c = *source++)) switch (c)
	{
		case '(': // comment
			source = scan(source, '(', ')');
			break;

		case '{': // start of function
			*current = source - sourcecode;
			source = scan(source, '{', '}');
			break;

		case '}': // end of function
			done = 1;
			break;

		case '[': // start of loop
			assert(marked < MARKS, ERR_MARKED);
			marks[marked++] = source;
			break;

		case '?': // conditional break function/loop
			if (!*current && !(done = (marked == markers)))
			{
				source = scan(source, '[', ']');
				marked--;
			}
			break;

		case ']': // end of loop
			assert(marked > 0, ERR_NOMARK);
			source = marks[marked-1];
			break;

		case '"': // string literal
			buffer = (char*)current;
			while (source && (c = *source++) && c != '"')
			{
				if (c == '$')
				{
					c = *source++;
					assert(isalpha(c), ERR_CHARACTER);
					buffer += sprintf(buffer, "%s", (char*)(islower(c) ? &inner[c-'a']: &outer[c-'A']));
					continue;
				}
				if (c == '#')
				{
					c = *source++;
					assert(isalpha(c), ERR_CHARACTER);
					buffer += sprintf(buffer, FORMAT, islower(c) ? inner[c-'a']: outer[c-'A']);
					continue;
				}
				*buffer++ = c;
			}
			*buffer = 0;
			break;

		case '$': // execute system command
			input_stdin();
			assert((input = popen((char*)current, "r")), ERR_POPEN);
			break;

		case ',': // read one character from input
			*current = fgetc(input);
			if (*current == EOF)
			{
				input_stdin();
				*current = 0;
			}
			break;

		case '.': // output character (or string)
			fwrite((char*)current, 1, strlen((char*)current), stdout);
			fflush(stdout);
			break;

		case '@': // set current by index
			assert(*current >= 0, ERR_BOUNDS);
			working = *current;
			if (working > mem)
			{
				assert((inner = realloc(inner, (working+1) * CELL)), ERR_MEMORY);
				memset(inner + mem, 0, ((working+1) - mem) * CELL);
				mem = working;
			}
			current = &inner[working];
			break;

		case ';': // call a function
			interpret(sourcecode + *current, inner);
			break;

		case '#': printf(FORMAT, *current);         break;
		case '+': *current += *previous;            break;
		case '-': *current *= -1;                   break;
		case '<': *current = *previous << *current; break;
		case '>': *current = *previous >> *current; break;
		case '&': *current &= *previous;            break;
		case '|': *current |= *previous;            break;
		case '^': *current ^= *previous;            break;
		case '!': *current = *current == 0 ? -1:0;  break;
		case '\\': *current = *current < 0 ? -1:0;   break;

		default:

			// integer literal
			if (isdigit(c))
			{
				*current = strtol(source-1, &source, 10);
				break;
			}

			// global named variable
			if (isalpha(c) && isalpha(*source) && isalpha(source[1]))
			{
				buffer = source-1;
				while (isalpha(*source)) source++;
				previous = current;
				current  = &(find(buffer, source-buffer)->value);
				break;
			}

			// local variable
			if (islower(c))
			{
				previous = current;
				current  = &inner[c-'a'];
				break;
			}

			// calling scope variable
			if (isupper(c))
			{
				previous = current;
				current  = &outer[c-'A'];
				break;
			}

			assert(isspace(c), ERR_CHARACTER);
	}
	working = *current;
	free(inner);
	return working;
}

int
main (int argc, char *argv[])
{
	FILE *script; int len = 0, rc = 0;
	previous = current = &globals[0];

	sourcecode = malloc(BLOCK);
	input_stdin();

	if (argc > 1)
	{
		strcpy(sourcecode, argv[1]);
		if ((script = fopen(argv[1], "r")))
		{
			while ((sourcecode[len] = fgetc(script)) != EOF)
			{
				if (++len % BLOCK == 0)
					assert((sourcecode = realloc(sourcecode, len+BLOCK)), ERR_MEMORY);
			}
			sourcecode[len] = 0;
			fclose(script);
		}
		rc = interpret(sourcecode, globals);
	}
	else
	for(;;)
	{
		printf("az> ");
		input_stdin();
		if (!fgets(sourcecode, BLOCK, stdin)) break;
		printf(FORMAT "\n", interpret(sourcecode, globals));
	}
	return rc;
}