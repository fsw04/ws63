/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025-2026. All rights reserved.
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
 *          Platform-specific key value storage implementation for hisilicon
 */
/* this file behaves like a config.h, comes first */

#include <platform/KeyValueStoreManager.h>
#include <lib/support/logging/CHIPLogging.h>
#include <map>
#include <string>
#include <vector>
#include "nv.h"
#include "KeyValueStoreManagerImpl.h"

namespace chip {
namespace DeviceLayer {
namespace PersistedStorage {

KeyValueStoreManagerImpl KeyValueStoreManagerImpl::sInstance;

// Extended key mapping table, including all certificates and keys
struct HiKeyMap {
    uint16_t    keyID;
    const char *keyStr;
    bool        isSensitive;  // Mark as sensitive data
};

struct HiKeyMap g_keyMap[] = {
    // Factory Data - Sensitive data needs to be stored encrypted
    { NV_ID_MATTER_AREA_START,      "chip-factory;serial-num", false },
    { NV_ID_MATTER_AREA_START + 1,  "chip-factory;device-id", false },
    { NV_ID_MATTER_AREA_START + 2,  "chip-config;cert-declaration", true }, // DC Certificate Statement - Sensitive
    { NV_ID_MATTER_AREA_START + 3,  "chip-factory;device-cert", true },      // DAC Certificate - Sensitive
    { NV_ID_MATTER_AREA_START + 4,  "chip-factory;device-ca-certs", true },  // PAI Certificate - Sensitive
    { NV_ID_MATTER_AREA_START + 5,  "chip-factory;device-key", true },       // Private Key - Highly Sensitive
    { NV_ID_MATTER_AREA_START + 6,  "chip-factory;hardware-ver", false },
    { NV_ID_MATTER_AREA_START + 7,  "chip-factory;mfg-date", false },
    { NV_ID_MATTER_AREA_START + 8,  "chip-factory;pin-code", true },         // PIN code - Sensitive
    { NV_ID_MATTER_AREA_START + 9,  "chip-factory;discriminator", false },
    { NV_ID_MATTER_AREA_START + 10,  "chip-factory;iteration-count", true },  // SPAKE2P Parameters - Sensitive
    { NV_ID_MATTER_AREA_START + 11, "chip-factory;salt", true },             // SPAKE2P Salt - Sensitive
    { NV_ID_MATTER_AREA_START + 12, "chip-factory;verifier", true },         // SPAKE2P Verifier - Sensitive
    { NV_ID_MATTER_AREA_START + 13, "chip-factory;uniqueId", false },
    { NV_ID_MATTER_AREA_START + 14, "chip-factory;software-ver", false },

    // Config Data
    { NV_ID_MATTER_AREA_START + 15, "chip-config;fabric-id", false },
    { NV_ID_MATTER_AREA_START + 16, "chip-config;service-config", false },
    { NV_ID_MATTER_AREA_START + 17, "chip-config;account-id", false },
    { NV_ID_MATTER_AREA_START + 18, "chip-config;service-id", false },
    { NV_ID_MATTER_AREA_START + 19, "chip-config;group-key-index", true },   // Group Key Index - Sensitive
    { NV_ID_MATTER_AREA_START + 20, "chip-config;last-ek-id", true },        // Last used Epoch key - Sensitive
    { NV_ID_MATTER_AREA_START + 21, "chip-config;fail-safe-armed", false },
    { NV_ID_MATTER_AREA_START + 22, "chip-config;sta-sec-type", false },
    { NV_ID_MATTER_AREA_START + 23, "chip-config;op-device-id", false },
    { NV_ID_MATTER_AREA_START + 24, "chip-config;op-device-cert", true },    // NOC Certificate - Sensitive
    { NV_ID_MATTER_AREA_START + 25, "chip-config;op-device-ca-certs", true }, // ICA Certificate - Sensitive
    { NV_ID_MATTER_AREA_START + 26, "chip-config;op-device-key", true },     // Handling Private Keys - Highly Sensitive
    { NV_ID_MATTER_AREA_START + 27, "chip-config;regulatory-location", false },
    { NV_ID_MATTER_AREA_START + 28, "chip-config;country-code", false },
    { NV_ID_MATTER_AREA_START + 29, "chip-config;breadcrumb", false },
    { NV_ID_MATTER_AREA_START + 30, "chip-config;wifi-ssid", true },         // WiFi SSID - Sensitive
    { NV_ID_MATTER_AREA_START + 31, "chip-config;wifi-password", true },     // WiFi Password - Highly Sensitive
    { NV_ID_MATTER_AREA_START + 32, "chip-config;wifi-security", false },
    { NV_ID_MATTER_AREA_START + 33, "chip-config;wifimode", false },

    // Counters Data
    { NV_ID_MATTER_AREA_START + 34, "chip-counters;reboot-count", false },
    { NV_ID_MATTER_AREA_START + 35, "chip-counters;up-time", false },
    { NV_ID_MATTER_AREA_START + 36, "chip-counters;total-hours", false },
    { NV_ID_MATTER_AREA_START + 37, "chip-counters;boot-reason", false },
    { NV_ID_MATTER_AREA_START + 38, "chip-counters;life-count", false },
    { NV_ID_MATTER_AREA_START + 39, "chip-factory;paa-cert", true },
    { NV_ID_MATTER_AREA_START + 40, "chip-factory;device-pkey", true },       // DAC Public Key - Sensitive
};

#define DYNAMIC_NV_ID_START (NV_ID_MATTER_AREA_START + 0x100) // Reserve some space after the static area
#define DYNAMIC_NV_ID_END   (DYNAMIC_NV_ID_START + 0x0400)    // Allocate 1024 dynamic IDs
#define DYNAMIC_NV_ID_COUNT (DYNAMIC_NV_ID_END - DYNAMIC_NV_ID_START + 1)

uint16_t KeyValueStoreManagerImpl::FindKeyIDByKeyName(const char * key)
{
    const uint16_t keyMapCount = 100;
    // 1. First, look up in the static mapping table (g_keyMap)
    uint16_t keyMapLen = sizeof(g_keyMap) / sizeof(g_keyMap[0]);
    for (int i = 0; i < keyMapLen; i++) {
        if (strcmp(key, g_keyMap[i].keyStr) == 0) {
            ChipLogProgress(DeviceLayer, "Key [%s] hit the static mapping table, ID: 0x%04X", key, g_keyMap[i].keyID);
            // Optional: Verify if the static ID is within its expected range
            if (g_keyMap[i].keyID >= NV_ID_MATTER_AREA_START &&
                g_keyMap[i].keyID < (NV_ID_MATTER_AREA_START + keyMapCount)) {
                return g_keyMap[i].keyID;
            } else {
                ChipLogError(DeviceLayer, "Static key [%s] ID 0x%04X is not within the expected static area range!",
                    key, g_keyMap[i].keyID);
                return 0; // Static table configuration error, return invalid ID
            }
        }
    }

    // 2. Key name not in static table, allocate a fixed ID in dynamic area via hash function
    ChipLogProgress(DeviceLayer, "Key [%s] not found in static table, performing dynamic hash allocation...", key);
    
    // Use a simple and stable hash function
    uint32_t hash = 5381; // DJB2 Initial Hash Value
    const char *p = key;
    while (*p) {
        hash = ((hash << 5) + hash) + *p++; // 5: hash * 33 + c
    }

    // Map the hash value to the dynamic ID range
    uint16_t dynamicId = DYNAMIC_NV_ID_START + (hash % DYNAMIC_NV_ID_COUNT);

    ChipLogProgress(DeviceLayer, "Key [%s] dynamically allocated ID: 0x%04X (hash: 0x%08lX)",
        key, dynamicId, (unsigned long)hash);
    return dynamicId;
}

bool KeyValueStoreManagerImpl::IsSensitiveKey(const char * key)
{
    uint16_t keyMapLen = sizeof(g_keyMap) / sizeof(g_keyMap[0]);
    for (int i = 0; i < keyMapLen; i++) {
        if (strcmp(key, g_keyMap[i].keyStr) == 0) {
            return g_keyMap[i].isSensitive;
        }
    }
    
    // The keys related to Fabric are all sensitive.
    if (strncmp(key, "f/", 2) == 0) { // Strings starting with 'f/' and 'g/' need to be stored encrypted
        return true;
    }

    if (strncmp(key, "g/", 2) == 0) { // // Strings starting with 'f/' and 'g/' need to be stored encrypted
        return true;
    }
    
    return false;
}

nv_key_attr_t KeyValueStoreManagerImpl::GetKeyAttributes(uint16_t keyID)
{
    nv_key_attr_t attr = {
        .permanent = false,   // Non-permanent, updates allowed
        .encrypted = true,    // Encrypted Storage
        .non_upgrade = false, // Upgrade allowed
        .reserve = 0
    };

    // For highly sensitive data, set to permanent storage
    const char* permanentKeys[] = {
        "chip-factory;device-cert",
        "chip-factory;device-ca-certs",
        "chip-factory;device-key",
        "chip-factory;pin-code"
    };

    uint16_t keyMapLen = sizeof(g_keyMap) / sizeof(g_keyMap[0]);
    for (int i = 0; i < keyMapLen; i++) {
        if (g_keyMap[i].keyID == keyID) {
            for (size_t j = 0; j < sizeof(permanentKeys) / sizeof(permanentKeys[0]); j++) {
                if (strcmp(g_keyMap[i].keyStr, permanentKeys[j]) == 0) {
                    attr.permanent = true;
                    break;
                }
            }
            break;
        }
    }

    return attr;
}

CHIP_ERROR KeyValueStoreManagerImpl::_Get(const char * key, void * value, size_t value_size, size_t * read_bytes_size,
                                          size_t offset_bytes)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    uint16_t keyID = FindKeyIDByKeyName(key);
    if (keyID < NV_ID_MATTER_AREA_START) {
        ChipLogError(DeviceLayer, "FindKeyIDByKeyName err, key_str:%s keyID:%x", key, keyID);
        return CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }

    uint16_t actualLen = 0;
    errcode_t ret;

    if (IsSensitiveKey(key)) {
        // For sensitive data, use attribute-based reads (automatic decryption)
        nv_key_attr_t attr = {0, 0, 0, 0};
        ret = uapi_nv_read_with_attr(keyID, static_cast<uint16_t>(value_size), &actualLen,
            static_cast<uint8_t*>(value), &attr);
    } else {
        // For non-sensitive data, use regular reads
        ret = uapi_nv_read(keyID, static_cast<uint16_t>(value_size), &actualLen,
            static_cast<uint8_t*>(value));
    }

    if (ret != 0) {
        ChipLogError(DeviceLayer, "NV read failed for key %s, error: %x", key, ret);
        return CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }

    if (read_bytes_size != nullptr) {
        *read_bytes_size = actualLen;
    }

    ChipLogProgress(DeviceLayer, "Successfully read key: %s, length: %d", key, actualLen);
    return err;
}

CHIP_ERROR KeyValueStoreManagerImpl::_Put(const char * key, const void * value, size_t value_size)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    uint16_t keyID = FindKeyIDByKeyName(key);
    if (keyID < NV_ID_MATTER_AREA_START) {
        ChipLogError(DeviceLayer, "Invalid key: %s", key);
        return CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }

    errcode_t ret;

    if (IsSensitiveKey(key)) {
        // For sensitive data, use encrypted storage
        nv_key_attr_t attr = GetKeyAttributes(keyID);
        if (value != nullptr && value_size == 0) {
            return CHIP_NO_ERROR;
        }

        ret = uapi_nv_write_with_attr(keyID, static_cast<const uint8_t*>(value),
            static_cast<uint16_t>(value_size), &attr, nullptr);
        ChipLogProgress(DeviceLayer, "Storing sensitive key: %s with encryption, permanent: %d",
            key, attr.permanent);
    } else {
        if (value != nullptr && value_size == 0) {
            ChipLogError(DeviceLayer, "NV write failed for key %s, key value size invalid! error: %x", key, ret);
            return CHIP_NO_ERROR;
        }
        // For non-sensitive data, use regular storage
        ret = uapi_nv_write(keyID, static_cast<const uint8_t*>(value),
            static_cast<uint16_t>(value_size));
    }

    if (ret != 0) {
        ChipLogError(DeviceLayer, "NV write failed for key %s, error: %x", key, ret);
        return CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    }

    ChipLogProgress(DeviceLayer, "Successfully stored key: %s, length: %d", key, value_size);
    return err;
}

CHIP_ERROR KeyValueStoreManagerImpl::_PutEncrypted(const char * key, const void * value, size_t value_size)
{
    // Enforce the use of encrypted storage, even if marked as non-sensitive in the original mapping
    uint16_t keyID = FindKeyIDByKeyName(key);
    if (keyID < NV_ID_MATTER_AREA_START) {
        return CHIP_ERROR_INVALID_KEY_ID;
    }

    nv_key_attr_t attr = GetKeyAttributes(keyID);
    attr.encrypted = true; // Ensure encryption

    errcode_t ret = uapi_nv_write_with_attr(keyID, static_cast<const uint8_t*>(value),
        static_cast<uint16_t>(value_size), &attr, nullptr);
    if (ret != 0) {
        ChipLogError(DeviceLayer, "Encrypted NV write failed for key %s, error: %d", key, ret);
        return CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    }

    ChipLogProgress(DeviceLayer, "Successfully stored encrypted key: %s", key);
    return CHIP_NO_ERROR;
}

CHIP_ERROR KeyValueStoreManagerImpl::_Delete(const char * key)
{
    // For encrypted permanent NV items, they cannot actually be deleted; here it returns as not supported.
    uint16_t keyID = FindKeyIDByKeyName(key);
    if (keyID < NV_ID_MATTER_AREA_START) {
        return CHIP_ERROR_INVALID_KEY_ID;
    }

    // Check if it is a permanent key-value
    nv_key_attr_t attr = GetKeyAttributes(keyID);
    if (attr.permanent) {
        ChipLogError(DeviceLayer, "Cannot delete permanent key: %s", key);
        return CHIP_ERROR_ACCESS_DENIED;
    }

    return CHIP_NO_ERROR;
}
} // namespace PersistedStorage
} // namespace DeviceLayer
} // namespace chip
