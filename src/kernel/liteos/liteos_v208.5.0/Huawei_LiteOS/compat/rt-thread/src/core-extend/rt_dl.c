/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025. All rights reserved.
 * Description : LiteOS adapt rtthread dl.
 * Author : Huawei LiteOS Team
 * Create : 2025-11-24
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef LOSCFG_KERNEL_DYNLOAD

#include <rthw.h>

#include "rtthread.h"
#include "dlfcn.h"
#include "dlmodule.h"
#include "dlelf.h"
#include "los_printf.h"
#include "los_ld_elflib.h"

void *dlopen(const char *filename, int flag)
{
    void *ret = NULL;
    RT_UNUSED(flag);

#if defined(LOSCFG_KERNEL_DYNLOAD) && defined(LOSCFG_DYNLOAD_DYN_FROM_FS)
    ret = LOS_SoLoad((CHAR *)filename);
#else
    (void)filename;
    PRINTK("Dynamic loading is not supported.\n");
#endif
    return ret;
}

int dlclose(void *handle)
{
#ifdef LOSCFG_KERNEL_DYNLOAD
    return LOS_ModuleUnload(handle);
#else
    RT_UNUSED(handle);
    PRINTK("Dynamic loading is not supported.\n");
    return -RT_ERROR;
#endif
}

void *dlsym(void *handle, const char *symbol)
{
    void *ret = NULL;
#ifdef LOSCFG_KERNEL_DYNLOAD
    ret = LOS_FindSymByName(handle, (CHAR *)symbol);
#else
    RT_UNUSED(handle);
    RT_UNUSED(symbol);
    PRINTK("Dynamic loading is not supported.\n");
#endif
    return ret;
}

struct rt_dlmodule *dlmodule_load(const char* pgname)
{
    RT_UNUSED(pgname);
    return RT_NULL;
}

struct rt_dlmodule* dlmodule_exec(const char* pgname, const char* cmd, int cmd_size)
{
    RT_UNUSED(pgname);
    RT_UNUSED(cmd);
    RT_UNUSED(cmd_size);
    return RT_NULL;
}

void dlmodule_exit(int ret_code)
{
    RT_UNUSED(ret_code);
    return;
}

struct rt_dlmodule *dlmodule_find(const char *name)
{
    RT_UNUSED(name);
    return RT_NULL;
}

rt_ubase_t dlmodule_symbol_find(const char *sym_str)
{
    RT_UNUSED(sym_str);
    return RT_EOK;
}
#endif