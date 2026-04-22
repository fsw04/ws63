/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025-2025. All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *      This file implements heap memory allocation APIs for CHIP. These functions are platform
 *      specific and might be C Standard Library heap functions re-direction in most of cases.
 *
 */

#include <lib/support/CHIPMem.h>
#include <platform/PlatformManager.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "osal_addr.h"

#if CHIP_CONFIG_MEMORY_MGMT_PLATFORM

namespace chip {
namespace Platform {

#define VERIFY_INITIALIZED() VerifyInitialized(__func__)

static std::atomic_int memoryInitialized{ 0 };

static void VerifyInitialized(const char * func)
{
    // Logging can use Memory::Alloc, so we can't use logging with our
    // VerifyOrDie bits here.
    VerifyOrDieWithoutLogging(memoryInitialized);
}

CHIP_ERROR MemoryAllocatorInit(void * buf, size_t bufSize)
{
#ifndef NDEBUG
    VerifyOrDieWithoutLogging(memoryInitialized++ == 0);
#endif
    return CHIP_NO_ERROR;
}

void MemoryAllocatorShutdown()
{
#ifndef NDEBUG
    VerifyOrDieWithoutLogging(--memoryInitialized == 0);
#endif
}

void * MemoryAlloc(size_t size)
{
    VERIFY_INITIALIZED();
    void *ptr = osal_kmalloc(size, OSAL_GFP_ATOMIC);
    return ptr;
}

void * MemoryAlloc(size_t size, bool isLongTermAlloc)
{
    VERIFY_INITIALIZED();
    void *ptr = osal_kmalloc(size, OSAL_GFP_ATOMIC);
    return ptr;
}

void * MemoryCalloc(size_t num, size_t size)
{
    VERIFY_INITIALIZED();
    void *ptr = osal_kmalloc(num * size, OSAL_GFP_ATOMIC);
    return ptr;
}

void * MemoryRealloc(void * p, size_t size)
{
    VERIFY_INITIALIZED();
    p = osal_krealloc(p, size, OSAL_GFP_ATOMIC);
    return p;
}

void MemoryFree(void * p)
{
    VERIFY_INITIALIZED();
    osal_kfree(p);
}

bool MemoryInternalCheckPointer(const void * p, size_t min_size)
{
    return (p != nullptr);
}

} // namespace Platform
} // namespace chip

#endif // CHIP_CONFIG_MEMORY_MGMT_PLATFORM
