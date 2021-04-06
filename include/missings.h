#pragma once

// These are standard library functions, which are not present for ts7350 :-(

#include <stdlib.h>

void qsort_r2(void *base, int nmemb, int size,
                  int (*compar)(const void *, const void *, void *),
                  void *arg);
void mergesort_r2(void *base, int nmemb, int size,
                  int (*compar)(const void *, const void *, void *),
                  void *arg);

typedef struct {
    unsigned long long size;
    unsigned int H[5];
    unsigned int W[16];
} blk_SHA_CTX;
void blk_SHA1_Init(blk_SHA_CTX *ctx);
void blk_SHA1_Update(blk_SHA_CTX *ctx, const void *dataIn, unsigned long len);
void blk_SHA1_Final(unsigned char hashout[20], blk_SHA_CTX *ctx);

unsigned char *SHA1(const unsigned char *d, size_t n, unsigned char *md);

bool file_getsha1(const char *filename,unsigned char *hash);
