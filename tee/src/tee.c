/*
 * Minimal tee(1) for Agon MOS (moslet).
 * Without pipes: tee [-a] DEST SRC
 * Copies SRC to stdout and to DEST.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 512

static void usage(const char *prog)
{
	printf("Usage: %s [-ha] dest source\n", prog);
	printf("  -a  append to dest instead of overwriting\n");
	printf("  -h  show this help\n");
}

int main(int argc, char *argv[])
{
	bool append = false;
	const char *dest = NULL;
	const char *src = NULL;
	FILE *in, *out;
	char buf[BUF_SIZE];
	size_t n;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "-a") == 0) {
			append = true;
		} else if (argv[i][0] == '-' && argv[i][1] != '\0') {
			usage(argv[0]);
			return 1;
		} else if (!dest) {
			dest = argv[i];
		} else if (!src) {
			src = argv[i];
		} else {
			usage(argv[0]);
			return 1;
		}
	}

	if (!dest || !src) {
		usage(argv[0]);
		return 1;
	}

	in = fopen(src, "r");
	if (!in) {
		fprintf(stderr, "tee: cannot open %s\n", src);
		return 1;
	}
	out = fopen(dest, append ? "a" : "w");
	if (!out) {
		fprintf(stderr, "tee: cannot open %s\n", dest);
		fclose(in);
		return 1;
	}

	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		fwrite(buf, 1, n, out);
		fwrite(buf, 1, n, stdout);
	}

	fclose(in);
	fclose(out);
	return 0;
}
