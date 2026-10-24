/*
 * Minimal uniq(1) for Agon MOS (moslet).
 * Supports: uniq [-c] [-d] [-u] file
 * Input need not be sorted; adjacent duplicates are collapsed
 * (same as classic uniq). Use sort first for global uniqueness.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX 1024

static void usage(const char *prog)
{
	printf("Usage: %s [-hcdu] file\n", prog);
	printf("  -c  prefix count of occurrences\n");
	printf("  -d  only print duplicate lines\n");
	printf("  -u  only print unique lines\n");
	printf("  -h  show this help\n");
}

static void strip_nl(char *s)
{
	size_t len = strlen(s);
	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
		s[--len] = '\0';
	}
}

static void emit(const char *line, int count, bool show_count, bool only_dup, bool only_uniq)
{
	if (only_dup && count < 2)
		return;
	if (only_uniq && count != 1)
		return;
	if (show_count)
		printf("%7d %s\n", count, line);
	else
		printf("%s\n", line);
}

int main(int argc, char *argv[])
{
	bool show_count = false;
	bool only_dup = false;
	bool only_uniq = false;
	const char *filename = NULL;
	FILE *fp;
	char line[LINE_MAX];
	char prev[LINE_MAX];
	bool have_prev = false;
	int count = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "-c") == 0) {
			show_count = true;
		} else if (strcmp(argv[i], "-d") == 0) {
			only_dup = true;
		} else if (strcmp(argv[i], "-u") == 0) {
			only_uniq = true;
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

	if (!filename) {
		usage(argv[0]);
		return 1;
	}

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "uniq: cannot open %s\n", filename);
		return 1;
	}

	while (fgets(line, sizeof(line), fp)) {
		strip_nl(line);
		if (!have_prev) {
			strcpy(prev, line);
			have_prev = true;
			count = 1;
		} else if (strcmp(prev, line) == 0) {
			count++;
		} else {
			emit(prev, count, show_count, only_dup, only_uniq);
			strcpy(prev, line);
			count = 1;
		}
	}

	if (have_prev)
		emit(prev, count, show_count, only_dup, only_uniq);

	fclose(fp);
	return 0;
}
