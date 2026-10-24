/*
 * Minimal df(1) for Agon MOS (moslet).
 * FatFS free-space API is not exposed via AgDev mos_api, so this
 * reports bytes *used* by walking the volume from /.
 */
#include <mos_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AM_DIR 0x10
#define MAX_QUEUE 200

static char *queue[MAX_QUEUE];
static int q_w, q_r;

static void usage(const char *prog)
{
	printf("Usage: %s [-h]\n", prog);
	printf("  Reports total bytes used on the volume (free space N/A).\n");
}

static unsigned long walk_used(void)
{
	DIR dir;
	FILINFO info;
	char child[256];
	unsigned long used = 0;
	uint8_t res;

	q_w = q_r = 0;
	queue[q_w++] = strdup("/");

	while (q_r < q_w) {
		char *path = queue[q_r++];
		res = ffs_dopen(&dir, path);
		if (res != FR_OK) {
			free(path);
			continue;
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
				if (q_w < MAX_QUEUE)
					queue[q_w++] = strdup(child);
			} else {
				used += info.fsize;
			}
		}
		ffs_dclose(&dir);
		free(path);
	}
	return used;
}

int main(int argc, char *argv[])
{
	unsigned long used;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else {
			usage(argv[0]);
			return 1;
		}
	}

	used = walk_used();
	printf("Filesystem     Used(bytes)\n");
	printf("%-14s %lu\n", "/", used);
	printf("(free space not available via MOS API)\n");
	return 0;
}
