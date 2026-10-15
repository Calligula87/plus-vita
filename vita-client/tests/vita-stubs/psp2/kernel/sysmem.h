#pragma once
#include <psp2/types.h>
#include <stddef.h>
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW 0
int sceKernelFreeMemBlock(SceUID);
SceUID sceKernelAllocMemBlock(const char *, int, size_t, void *);
int sceKernelGetMemBlockBase(SceUID, void **);
