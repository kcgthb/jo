#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include "json.h"

/*
 * Copyright (C) 2016 Jan-Piet Mens <jpmens@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef TRUE
# define TRUE 	(1)
# define FALSE	(0)
#endif

/*
 * Attempt to "sniff" the type of data in `str' and return
 * a JsonNode of the correct JSON type.
 */

JsonNode *vnode(char *str)
{

	if (strlen(str) == 0) {
		return json_mknull();
	}

	/* If str begins with a double quote, keep it a string */

	if (*str == '"') {
		char *bp = str + strlen(str) - 1;

		if (bp > str && *bp == '"')
			*bp = 0;		/* Chop closing double quote */
		return json_mkstring(str + 1);
	}

	char *endptr;
	double num = strtod(str, &endptr);

	if (!*endptr && isfinite(num)) {
		return json_mknumber(num);
	}

	if (*str == '{' || *str == '[') {
		JsonNode *obj = json_decode(str);

		if (obj == NULL) {
			/* JSON cannot be decoded; return the string */
			// fprintf(stderr, "Cannot decode JSON from %s\n", str);

			obj = json_mkstring(str);
		}

		return (obj);
	}

	return json_mkstring(str);
}

/*
 * Attempt to sniff `str' into a boolean; return a
 * corresponding JsonNode for it.
 */

JsonNode *boolnode(char *str)
{
	if (strlen(str) == 0) {
		return json_mknull();
	}

	if (tolower(*str) == 't') {
		return json_mkbool(1);
	}

	return json_mkbool(atoi(str));
}

int usage(char *prog)
{
	fprintf(stderr, "Usage: %s [-a] [-p] [word...]\n", prog);
	fprintf(stderr, "\tword is key=value or key@value\n");
	fprintf(stderr, "\t-a creates an array of words, -p pretty-prints\n");

	return (-1);
}

int member_to_object(JsonNode *object, char *kv)
{
	/* we expect key=value or key:value (boolean on last) */
	char *p = strchr(kv, '=');
	char *q = strchr(kv, '@');

	if (!p && !q) {
		return (-1);
	}

	if (p) {
		*p = 0;

		json_append_member(object, kv, vnode(p+1));
	} else {
		*q = 0;
		json_append_member(object, kv, boolnode(q+1));
	}
	return (0);
}

/*
 * Append kv to the array or object.
 */

void append_kv(JsonNode *object_or_array, int isarray, char *kv)
{
	if (isarray) {
		json_append_element(object_or_array, vnode(kv));
	} else {
		if (member_to_object(object_or_array, kv) == -1) {
			fprintf(stderr, "Argument `%s' is neither k=v nor k@v\n", kv);
		}
	}
}

#ifdef _WIN32
#include <windows.h>
char* utf8_from_locale(const char *str, size_t len)
{
	wchar_t* wcsp;
	char* mbsp;
	size_t mbssize, wcssize;

	if (len == 0) {
		return strdup("");
	}
	if (len == -1) {
		len = strlen(str);
	}
	wcssize = MultiByteToWideChar(GetACP(), 0, str, len,  NULL, 0);
	wcsp = (wchar_t*) malloc((wcssize + 1) * sizeof(wchar_t));
	if (!wcsp) {
		return NULL;
	}
	wcssize = MultiByteToWideChar(GetACP(), 0, str, len, wcsp, wcssize + 1);
	wcsp[wcssize] = 0;

	mbssize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) wcsp, -1, NULL, 0, NULL, NULL);
	mbsp = (char*) malloc((mbssize + 1));
	if (!mbsp) {
		free(wcsp);
		return NULL;
	}
	mbssize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) wcsp, -1, mbsp, mbssize, NULL, NULL);
	mbsp[mbssize] = 0;
	free(wcsp);
	return mbsp;
}
# define utf8_free(p) free(p)

char* locale_from_utf8(const char *utf8, size_t len)
{
	wchar_t* wcsp;
	char* mbsp;
	size_t mbssize, wcssize;

	if (len == 0) {
		return strdup("");
	}
	if (len == -1) {
		len = strlen(utf8);
	}
	wcssize = MultiByteToWideChar(CP_UTF8, 0, utf8, len,  NULL, 0);
	wcsp = (wchar_t*) malloc((wcssize + 1) * sizeof(wchar_t));
	if (!wcsp) {
		return NULL;
	}
	wcssize = MultiByteToWideChar(CP_UTF8, 0, utf8, len, wcsp, wcssize + 1);
	wcsp[wcssize] = 0;
	mbssize = WideCharToMultiByte(GetACP(), 0, (LPCWSTR) wcsp, -1, NULL, 0, NULL, NULL);
	mbsp = (char*) malloc((mbssize + 1));
	if (!mbsp) {
		free(wcsp);
		return NULL;
	}
	mbssize = WideCharToMultiByte(GetACP(), 0, (LPCWSTR) wcsp, -1, mbsp, mbssize, NULL, NULL);
	mbsp[mbssize] = 0;
	free(wcsp);
	return mbsp;
}
# define locale_free(p) free(p)
#else
# define utf8_from_locale(p, l) (p)
# define utf8_free(p)
# define locale_from_utf8(p, l) (p)
# define locale_free(p)
#endif

int main(int argc, char **argv)
{
	int c, isarray = FALSE;
	char *kv, *js_string, *progname, *pretty = NULL, buf[BUFSIZ], *p;
	int ttyin = isatty(fileno(stdin)), ttyout = isatty(fileno(stdout));
	JsonNode *json;

	progname = (progname = strrchr(*argv, '/')) ? progname + 1 : *argv;

	while ((c = getopt(argc, argv, "ap")) != EOF) {
		switch (c) {
			case 'a':
				isarray = TRUE;
				break;
			case 'p':
				pretty = "   ";
				break;
			default:
				exit(usage(progname));
		}
	}

	argc -= optind;
	argv += optind;

	json = (isarray) ? json_mkarray() : json_mkobject();

	if (argc == 0) {
		while (fgets(buf, sizeof(buf), stdin) != NULL) {
			if (buf[strlen(buf) - 1] == '\n')
				buf[strlen(buf) - 1] = 0;
			p = ttyin ? utf8_from_locale(buf, -1) : buf;
			append_kv(json, isarray, p);
			if (ttyin) utf8_free(p);
		}
	} else {
		while ((kv = *argv++)) {
			p = utf8_from_locale(kv, -1);
			append_kv(json, isarray, p);
			utf8_free(p);
		}
	}

	if ((js_string = json_stringify(json, pretty)) == NULL) {
		fprintf(stderr, "Invalid JSON\n");
		exit(2);
	}

	p = ttyout ? locale_from_utf8(js_string, -1) : js_string;
	printf("%s\n", p);
	if (ttyout) locale_free(p);
	free(js_string);
	json_delete(json);
	return (0);
}
