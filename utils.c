#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <stdarg.h>
#include <alloca.h>

#include "./utils.h"

char *
strdup_printf(const char *fmt, ...)
{
	static size_t initial_size = 128;
	va_list args;
	char *buf;
	int s;

	s = initial_size;
	buf = malloc(s+1);
retry:
	va_start (args, fmt);
	s = vsnprintf (buf, s+1, fmt, args);
	va_end (args);
	if ((size_t)s > initial_size) {
		// TODO Replace with an atomic opetarion
		initial_size = s;
		buf = realloc (buf, s+1);
		goto retry;
	}
	return buf;
}

