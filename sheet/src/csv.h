#ifndef SHEET_CSV_H
#define SHEET_CSV_H

#include "model.h"

bool sheet_load_csv(Sheet *s, const char *path, const char **errmsg);
bool sheet_save_csv(const Sheet *s, const char *path, const char **errmsg);

#endif
