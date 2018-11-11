#pragma once

// These are standard library functions, which are not present for ts7350 :-(

#include <stdlib.h>

void qsort_r2(void *base, int nmemb, int size,
                  int (*compar)(const void *, const void *, void *),
                  void *arg);
void mergesort_r2(void *base, int nmemb, int size,
                  int (*compar)(const void *, const void *, void *),
                  void *arg);

unsigned char *SHA1(const unsigned char *d, size_t n, unsigned char *md);
