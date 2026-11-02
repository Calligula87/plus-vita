#pragma once
#include <psp2/types.h>
#include <stddef.h>
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW 0
enum { SCE_KERNEL_MODEL_VITA=0x10000,SCE_KERNEL_MODEL_VITATV=0x20000 };
int sceKernelGetModel(void);
int sceKernelFreeMemBlock(SceUID);
SceUID sceKernelAllocMemBlock(const char *, int, size_t, void *);
int sceKernelGetMemBlockBase(SceUID, void **);
