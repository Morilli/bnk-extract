#ifndef WPK_H
#define WPK_H

#include <stdbool.h>
#include "bin.h"

void extract_wpk_file(char* wpk_path, StringHashes* string_hashes, char* output_path, bool wems_only, bool oggs_only);

#endif
