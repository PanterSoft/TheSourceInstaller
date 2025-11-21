#ifndef FETCHER_H
#define FETCHER_H

#include <stdbool.h>
#include "package.h"

#ifdef __cplusplus
extern "C" {
#endif

// Source fetcher
typedef struct {
    char *source_dir;
} SourceFetcher;

// Fetcher functions
SourceFetcher* fetcher_new(const char *source_dir);
void fetcher_free(SourceFetcher *fetcher);
char* fetcher_fetch(SourceFetcher *fetcher, Package *pkg, bool force);
bool fetcher_download_file(const char *url, const char *dest);
bool fetcher_extract_tarball(const char *archive, const char *dest);
bool fetcher_clone_git(const char *url, const char *dest, const char *branch, const char *tag, const char *commit);

#ifdef __cplusplus
}
#endif

#endif // FETCHER_H

