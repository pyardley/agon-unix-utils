/*
 * Minimal tr(1) for Agon MOS (moslet).
 * tr [-d] SET1 [SET2] file
 * SET may include ranges like a-z or A-Z.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define SET_MAX 256

static void usage(const char *prog)
{
	printf("Usage: %s [-hd] SET1 [SET2] file\n", prog);
	printf("  -d  delete characters in SET1\n");
	printf("  -h  show this help\n");
}

/* Expand set with ranges (x-y) into out[]; returns length. */
static int expand_set(const char *spec, unsigned char *out, int max)
{
	int n = 0;
	int i = 0;

	while (spec[i] && n < max) {
		unsigned char a = (unsigned char)spec[i];
		if (spec[i + 1] == '-' && spec[i + 2] != '\0') {
			unsigned char b = (unsigned char)spec[i + 2];
			unsigned char c;
			if (a <= b) {
				for (c = a; c <= b && n < max; c++)
					out[n++] = c;
			} else {
				for (c = a; c >= b && n < max; c--)
					out[n++] = c;
			}
			i += 3;
		} else {
			out[n++] = a;
			i++;
		}
	}
	return n;
}

int main(int argc, char *argv[])
{
	bool delete_mode = false;
	const char *set1 = NULL;
	const char *set2 = NULL;
	const char *filename = NULL;
	FILE *fp;
	int c;
	int i;
	char map[256];
	bool del[256];
	unsigned char e1[SET_MAX], e2[SET_MAX];
	int n1, n2;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "-d") == 0) {
			delete_mode = true;
		} else if (argv[i][0] == '-' && argv[i][1] != '\0') {
			usage(argv[0]);
			return 1;
		} else if (!set1) {
			set1 = argv[i];
		} else if (!delete_mode && !set2) {
			set2 = argv[i];
		} else if (!filename) {
			filename = argv[i];
		} else {
			usage(argv[0]);
			return 1;
		}
	}

	if (!set1 || !filename || (!delete_mode && !set2)) {
		usage(argv[0]);
		return 1;
	}

	for (i = 0; i < 256; i++) {
		map[i] = (char)i;
		del[i] = false;
	}

	n1 = expand_set(set1, e1, SET_MAX);
	if (delete_mode) {
		for (i = 0; i < n1; i++)
			del[e1[i]] = true;
	} else {
		n2 = expand_set(set2, e2, SET_MAX);
		if (n2 == 0) {
			fprintf(stderr, "tr: SET2 empty\n");
			return 1;
		}
		for (i = 0; i < n1; i++) {
			unsigned char to = (i < n2) ? e2[i] : e2[n2 - 1];
			map[e1[i]] = (char)to;
		}
	}

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "tr: cannot open %s\n", filename);
		return 1;
	}

	while ((c = fgetc(fp)) != EOF) {
		unsigned char uc = (unsigned char)c;
		if (delete_mode) {
			if (!del[uc])
				putchar(c);
		} else {
			putchar(map[uc]);
		}
	}

	fclose(fp);
	return 0;
}
