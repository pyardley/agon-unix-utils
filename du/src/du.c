/*
 * Minimal du(1) for Agon MOS (moslet).
 * du [-s] [path...]
 * Reports sizes in bytes. -s: summarise only (one total per path).
 */
#include <mos_api.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AM_DIR 0x10

static bool summarise = false;

static void usage(const char *prog)
{
	printf("Usage: %s [-hs] [path...]\n", prog);
	printf("  -s  display only a total for each argument\n");
	printf("  -h  show this help\n");
}

static unsigned long du_path(const char *path, bool print_each)
{
	DIR dir;
	FILINFO info;
	char child[256];
	unsigned long total = 0;
	uint8_t res;

	res = ffs_dopen(&dir, path);
	if (res != FR_OK) {
		/* Maybe a plain file — try fopen size */
		FILE *fp = fopen(path, "r");
		long sz;
		if (!fp) {
			fprintf(stderr, "du: cannot access %s\n", path);
			return 0;
		}
		if (fseek(fp, 0, SEEK_END) == 0) {
			sz = ftell(fp);
			if (sz < 0)
				sz = 0;
		} else {
			sz = 0;
		}
		fclose(fp);
		if (print_each && !summarise)
			printf("%lu\t%s\n", (unsigned long)sz, path);
		return (unsigned long)sz;
	}

	for (;;) {
		res = ffs_dread(&dir, &info);
		if (res != FR_OK || info.fname[0] == 0)
			break;
		if (strcmp(info.fname, ".") == 0 || strcmp(info.fname, "..") == 0)
			continue;

		if (strcmp(path, "/") == 0)
			snprintf(child, sizeof(child), "/%s", info.fname);
		else
			snprintf(child, sizeof(child), "%s/%s", path, info.fname);

		if (info.fattrib & AM_DIR) {
			/* With -s, only the top-level path should print. */
			total += du_path(child, summarise ? false : print_each);
		} else {
			total += info.fsize;
			if (print_each && !summarise)
				printf("%lu\t%s\n", (unsigned long)info.fsize, child);
		}
	}
	ffs_dclose(&dir);

	if (print_each)
		printf("%lu\t%s\n", total, path);
	return total;
}

int main(int argc, char *argv[])
{
	int i;
	int saw = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "-s") == 0) {
			summarise = true;
		} else if (argv[i][0] == '-' && argv[i][1] != '\0') {
			usage(argv[0]);
			return 1;
		} else {
			saw = 1;
			du_path(argv[i], true);
		}
	}

	if (!saw)
		du_path(".", true);

	return 0;
}
