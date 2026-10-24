/*
 * Minimal diff(1) for Agon MOS (moslet).
 * Line-oriented compare: prints differing line pairs.
 * diff [-q] file1 file2
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LINE_MAX 512

static void usage(const char *prog)
{
	printf("Usage: %s [-hq] file1 file2\n", prog);
	printf("  -q  quiet: only report whether files differ\n");
	printf("  -h  show this help\n");
}

static void strip_nl(char *s)
{
	size_t n = strlen(s);
	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
		s[--n] = '\0';
}

int main(int argc, char *argv[])
{
	bool quiet = false;
	const char *f1 = NULL;
	const char *f2 = NULL;
	FILE *a, *b;
	char la[LINE_MAX], lb[LINE_MAX];
	int line = 0;
	int diffs = 0;
	int i;
	bool eof_a = false, eof_b = false;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "-q") == 0) {
			quiet = true;
		} else if (argv[i][0] == '-' && argv[i][1] != '\0') {
			usage(argv[0]);
			return 1;
		} else if (!f1) {
			f1 = argv[i];
		} else if (!f2) {
			f2 = argv[i];
		} else {
			usage(argv[0]);
			return 1;
		}
	}

	if (!f1 || !f2) {
		usage(argv[0]);
		return 1;
	}

	a = fopen(f1, "r");
	if (!a) {
		fprintf(stderr, "diff: cannot open %s\n", f1);
		return 2;
	}
	b = fopen(f2, "r");
	if (!b) {
		fprintf(stderr, "diff: cannot open %s\n", f2);
		fclose(a);
		return 2;
	}

	for (;;) {
		char *pa = eof_a ? NULL : fgets(la, sizeof(la), a);
		char *pb = eof_b ? NULL : fgets(lb, sizeof(lb), b);

		if (!pa)
			eof_a = true;
		if (!pb)
			eof_b = true;
		if (eof_a && eof_b)
			break;

		line++;
		if (pa)
			strip_nl(la);
		else
			la[0] = '\0';
		if (pb)
			strip_nl(lb);
		else
			lb[0] = '\0';

		if (!pa || !pb || strcmp(la, lb) != 0) {
			diffs++;
			if (!quiet) {
				printf("%dc%d\n", line, line);
				if (pa)
					printf("< %s\n", la);
				else
					printf("< (no line)\n");
				printf("---\n");
				if (pb)
					printf("> %s\n", lb);
				else
					printf("> (no line)\n");
			}
		}
	}

	fclose(a);
	fclose(b);

	if (quiet && diffs)
		printf("Files %s and %s differ\n", f1, f2);

	return diffs ? 1 : 0;
}
