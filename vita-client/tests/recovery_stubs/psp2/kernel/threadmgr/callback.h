#pragma once
typedef int (*SceKernelCallbackFunction)(int,int,int,void *);
int sceKernelCreateCallback(const char *,unsigned,SceKernelCallbackFunction,void *);
int sceKernelDeleteCallback(int);
int sceKernelCheckCallback(void);
