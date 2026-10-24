/*
 * Minimal touch(1) for Agon MOS (moslet).
 * Creates an empty file if missing; opens existing files for append
 * then closes them (timestamp update depends on filesystem/MOS).
 */
#include <stdio.h>
#include <string.h>

static void usage(const char *prog)
{
	printf("Usage: %s [-h] file...\n", prog);
}

int main(int argc, char *argv[])
{
	int i, errors = 0;
	int saw_file = 0;

	for (i = 1; i < argc; i++) {
		FILE *fp;

		if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		}
		if (argv[i][0] == '-' && argv[i][1] != '\0') {
			usage(argv[0]);
			return 1;
		}

		saw_file = 1;
		fp = fopen(argv[i], "a");
		if (!fp) {
			fp = fopen(argv[i], "w");
		}
		if (!fp) {
			fprintf(stderr, "touch: cannot create %s\n", argv[i]);
			errors = 1;
			continue;
		}
		fclose(fp);
	}

	if (!saw_file) {
		usage(argv[0]);
		return 1;
	}
	return errors;
}
