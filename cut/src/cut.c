/*
 * Minimal cut(1) for Agon MOS (moslet).
 * Supports: cut -d DELIM -f LIST file
 * LIST is comma-separated 1-based field numbers (e.g. 1,3 or 2).
 * Delimiter defaults to TAB. Accepts -d, and -f1,3 attached forms.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 64
#define LINE_MAX 1024

static void usage(const char *prog)
{
	printf("Usage: %s [-h] [-d delim] -f list file\n", prog);
	printf("  -d delim  field delimiter (default TAB)\n");
	printf("  -f list   comma-separated field numbers (1-based)\n");
	printf("  -h        show this help\n");
}

static int parse_list(const char *list, int *fields, int max)
{
	int n = 0;
	const char *p = list;

	while (*p && n < max) {
		int v = 0;
		if (*p < '0' || *p > '9')
			return -1;
		while (*p >= '0' && *p <= '9') {
			v = v * 10 + (*p - '0');
			p++;
		}
		if (v < 1)
			return -1;
		fields[n++] = v;
		if (*p == ',') {
			p++;
			continue;
		}
		if (*p == '\0')
			break;
		return -1;
	}
	return n;
}

static void emit_fields(char *line, char delim, const int *fields, int nfields)
{
	char *parts[MAX_FIELDS];
	int count = 0;
	char *p = line;
	size_t len;
	int i;
	bool first = true;

	len = strlen(line);
	while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
		line[--len] = '\0';
	}
	if (len == 0)
		return;

	parts[count++] = p;
	while (*p && count < MAX_FIELDS) {
		if (*p == delim) {
			*p = '\0';
			parts[count++] = p + 1;
		}
		p++;
	}

	{
		char out[LINE_MAX];
		size_t o = 0;
		for (i = 0; i < nfields; i++) {
			int idx = fields[i] - 1;
			size_t plen;
			if (idx < 0 || idx >= count)
				continue;
			if (!first) {
				if (o + 1 >= sizeof(out))
					break;
				out[o++] = delim;
			}
			plen = strlen(parts[idx]);
			if (o + plen >= sizeof(out))
				break;
			memcpy(out + o, parts[idx], plen);
			o += plen;
			first = false;
		}
		out[o] = '\0';
		printf("%s\n", out);
	}
}

int main(int argc, char *argv[])
{
	char delim = '\t';
	const char *list = NULL;
	const char *filename = NULL;
	int fields[MAX_FIELDS];
	int nfields;
	FILE *fp;
	char line[LINE_MAX];
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else if (strncmp(argv[i], "-d", 2) == 0) {
			if (argv[i][2] != '\0') {
				delim = argv[i][2];
			} else if (i + 1 >= argc || argv[i + 1][0] == '\0') {
				usage(argv[0]);
				return 1;
			} else {
				delim = argv[++i][0];
			}
		} else if (strncmp(argv[i], "-f", 2) == 0) {
			if (argv[i][2] != '\0') {
				list = &argv[i][2];
			} else if (i + 1 >= argc) {
				usage(argv[0]);
				return 1;
			} else {
				list = argv[++i];
			}
		} else if (argv[i][0] == '-' && argv[i][1] != '\0') {
			usage(argv[0]);
			return 1;
		} else if (!filename) {
			filename = argv[i];
		} else {
			usage(argv[0]);
			return 1;
		}
	}

	if (!list || !filename) {
		usage(argv[0]);
		return 1;
	}

	nfields = parse_list(list, fields, MAX_FIELDS);
	if (nfields < 1) {
		fprintf(stderr, "cut: bad -f list\n");
		return 1;
	}

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "cut: cannot open %s\n", filename);
		return 1;
	}

	while (fgets(line, sizeof(line), fp)) {
		emit_fields(line, delim, fields, nfields);
	}

	fclose(fp);
	return 0;
}
