#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#define MIN(a,b) ((a)<(b) ? (a): (b))
#define MAX(a,b) ((a)>(b) ? (a): (b))

typedef int cell;
#define FORMAT "%d"

char*
scan(char *source, char open, char close)
{
	int levels = 1;
	while (levels > 0 && *source)
	{
		char c = *source++;
		if (open  && c == open)  levels++;
		if (close && c == close) levels--;
	}
	return source;
}

cell
interpret(char *source, cell *outer)
{
	cell *inner, *current, *previous, rc;
	assert((inner = calloc(26, sizeof(cell))));
	current = inner, previous = inner;

	FILE *input = stdin; int depth = 0, done = 0, mem = 26;
	char *base = source, *buffer, *from, *branches[4];

	while (!done && source && *source)
	{
		char c = *source++;
		switch (c)
		{
			case '(':
				source = scan(source, '(', ')');
				break;

			case '{':
				*current = source - base;
				source = scan(source, '{', '}');
				break;

			case '}':
				done = 1;
				break;

			case '[':
				assert(depth < 4);
				branches[depth++] = source;
				break;

			case '?':
				assert(depth > 0);
				if (!*current)
				{
					source = scan(source, '[', ']');
					depth--;
				}
				break;

			case ']':
				assert(depth > 0);
				source = branches[depth-1];
				break;

			case '"':
				buffer = (char*)current;
				while (source && (c = *source++) && c != '"')
				{
					if (c == '$')
					{
						c = *source++;
						assert(isalpha(c));
						from = islower(c) ? (char*)(&inner[c-'a']): (char*)(&outer[c-'A']);
						while (*from) *buffer++ = *from++;
						continue;
					}
					if (c == '#')
					{
						c = *source++;
						assert(isalpha(c));
						buffer += sprintf(buffer, FORMAT, islower(c) ? inner[c-'a']: outer[c-'A']);
						continue;
					}
					*buffer++ = c;
				}
				*buffer = 0;
				break;

			case '$':
				if (input && input != stdin)
					pclose(input);
				assert((input = popen((char*)current, "r")));
				break;

			case ',':
				*current = fgetc(input);
				if (*current == EOF)
				{
					if (input != stdin)
					{
						pclose(input);
						input = stdin;
					}
					*current = 0;
				}
				break;

			case '.':
				buffer = (char*)current;
				while (buffer && *buffer)
					fputc(*buffer++, stdout);
				fflush(stdout);
				break;

			case '@':
				assert(*current > 0);
				if (*current > mem)
				{
					assert((inner = realloc(inner, *current * sizeof(cell))));
					memset(inner + mem, 0, (*current - mem) * sizeof(cell));
					mem = *current;
				}
				current = &inner[*current];
				break;

			case '#': printf(FORMAT, *current);                  break;
			case ';': interpret(base + *current, inner);         break;
			case ':': *current = *previous;                      break;
			case '+': *current += *previous;                     break;
			case '-': *current = *previous - *current;           break;
			case '*': *current *= *previous;                     break;
			case '/': *current = *previous / *current;           break;
			case '%': *current = *previous % *current;           break;
			case '&': *current &= *previous;                     break;
			case '|': *current |= *previous;                     break;
			case '^': *current ^= *previous;                     break;
			case '=': *current = *previous == *current ? -1:0;   break;
			case '!': *current = *current == 0 ? -1:0;           break;
			case '<': *current = MIN(*current, *previous);       break;
			case '>': *current = MAX(*current, *previous);       break;

			default:

				if (isdigit(c))
				{
					*current = strtol(source-1, &source, 10);
					break;
				}
				if (islower(c))
				{
					previous = current;
					current = &inner[c - 'a'];
					break;
				}
				if (isupper(c))
				{
					previous = current;
					current = &outer[c - 'A'];
					break;
				}
		}
	}
	rc = *current;
	if (input && input != stdin)
		pclose(input);
	free(inner);
	return rc;
}

int
main (int argc, char *argv[])
{
	int len = 0, rc = 0;
	char *source = malloc(1024);

	cell globals[26]; FILE *script;
	memset(globals, 0, sizeof(globals));

	if (argc > 1)
	{
		strcpy(source, argv[1]);
		if ((script = fopen(argv[1], "r")))
		{
			while ((source[len++] = fgetc(script)) != EOF)
			{
				if (len % 1024 == 0)
					assert((source = realloc(source, len+1024)));
			}
			source[len] = 0;
			fclose(script);
		}
		rc = interpret(source, globals);
	}
	else
	{
		for(;;)
		{
			printf("> ");
			if (!fgets(source, 1024, stdin))
				break;
			printf(FORMAT "\n", interpret(source, globals));
		}
	}
	free(source);
	return rc;
}