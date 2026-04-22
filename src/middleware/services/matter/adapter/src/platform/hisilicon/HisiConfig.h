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

#pragma once

#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <cstring>
#include "securec.h"
namespace chip {
namespace DeviceLayer {
namespace Internal {

class HisiConfig {
public:
    struct Key;

    // Maximum length of an NVS key name.
    static constexpr size_t kMaxConfigKeyNameLength = 128;

    // NVS namespaces used to store device configuration information.
    static const char kConfigNamespace_ChipFactory[];
    static const char kConfigNamespace_ChipConfig[];
    static const char kConfigNamespace_ChipCounters[];

    // Key definitions for well-known keys.
    static const Key kConfigKey_SerialNum;
    static const Key kConfigKey_UniqueId;
    static const Key kConfigKey_MfrDeviceId;
    static const Key kConfigKey_CertDeclaration;
    static const Key kConfigKey_MfrDeviceCert;
    static const Key kConfigKey_MfrDeviceICACerts;
    static const Key kConfigKey_MfrDevicePrivateKey;
    static const Key kConfigKey_SoftwareVersion;
    static const Key kConfigKey_HardwareVersion;
    static const Key kConfigKey_ManufacturingDate;

    static const Key kConfigKey_SetupPinCode;
    static const Key kConfigKey_FabricId;

    static const Key kConfigKey_ServiceConfig;
    static const Key kConfigKey_PairedAccountId;
    static const Key kConfigKey_ServiceId;

    static const Key kConfigKey_FabricSecret;
    static const Key kConfigKey_GroupKeyIndex;

    static const Key kConfigKey_LastUsedEpochKeyId;
    static const Key kConfigKey_FailSafeArmed;

    static const Key kConfigKey_OperationalDeviceId;
    static const Key kConfigKey_OperationalDeviceCert;
    static const Key kConfigKey_OperationalDeviceICACerts;
    static const Key kConfigKey_OperationalDevicePrivateKey;
    static const Key kConfigKey_SetupDiscriminator;
    static const Key kConfigKey_RegulatoryLocation;
    static const Key kConfigKey_CountryCode;
    static const Key kConfigKey_Breadcrumb;
    static const Key kConfigKey_Spake2pIterationCount;
    static const Key kConfigKey_Spake2pSalt;
    static const Key kConfigKey_Spake2pVerifier;

    static const Key kConfigKey_WiFiStationSecType;
    static const Key kConfigKey_WiFiSSID;
    static const Key kConfigKey_WiFiPassword;
    static const Key kConfigKey_WiFiSecurity;
    static const Key kConfigKey_WiFiMode;

    // Counter keys
    static const Key kCounterKey_RebootCount;
    static const Key kCounterKey_UpTime;
    static const Key kCounterKey_TotalOperationalHours;
    static const Key kCounterKey_BootReason;

    static const char kGroupKeyNamePrefix[];

    // Config value accessors.
    static CHIP_ERROR ReadConfigValue(Key key, bool & val);
    static CHIP_ERROR ReadConfigValue(Key key, uint32_t & val);
    static CHIP_ERROR ReadConfigValue(Key key, uint64_t & val);
    static CHIP_ERROR ReadConfigValueStr(Key key, char * buf, size_t bufSize, size_t & outLen);
    static CHIP_ERROR ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen);
    static CHIP_ERROR WriteConfigValue(Key key, bool val);
    static CHIP_ERROR WriteConfigValue(Key key, uint32_t val);
    static CHIP_ERROR WriteConfigValue(Key key, uint64_t val);
    static CHIP_ERROR WriteConfigValueStr(Key key, const char * str);
    static CHIP_ERROR WriteConfigValueStr(Key key, const char * str, size_t strLen);
    static CHIP_ERROR WriteConfigValueBin(Key key, const uint8_t * data, size_t dataLen);
    static CHIP_ERROR ClearConfigValue(Key key);
    static bool ConfigValueExists(Key key);

    static CHIP_ERROR EnsureNamespace(const char * ns);
    static CHIP_ERROR ClearNamespace(const char * ns);

    static CHIP_ERROR FactoryResetConfig(void);
    static void RunConfigUnitTest(void);
};

struct HisiConfig::Key {
    const char * Namespace;
    const char * Name;

    CHIP_ERROR to_str(char * buf, size_t buf_size) const;
    size_t len() const;
    bool operator==(const Key & other) const;
};

inline CHIP_ERROR HisiConfig::Key::to_str(char * buf, size_t buf_size) const
{
    if (buf == nullptr) {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    if (buf_size < len() + 1) {
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }

    errno_t result = strcpy_s(buf, buf_size, Namespace);
    if (result != 0) {
        return CHIP_ERROR_INTERNAL;
    }

    result = strcat_s(buf, buf_size, ";");
    if (result != 0) {
        return CHIP_ERROR_INTERNAL;
    }

    result = strcat_s(buf, buf_size, Name);
    if (result != 0) {
        return CHIP_ERROR_INTERNAL;
    }

    return CHIP_NO_ERROR;
}

// Length of key str (not including terminating null char)
inline size_t HisiConfig::Key::len() const
{
    // + 1 for separating ';'
    size_t out_size = strlen(Namespace) + strlen(Name) + 1;
    return out_size;
}

inline bool HisiConfig::Key::operator==(const Key & other) const
{
    return strcmp(Namespace, other.Namespace) == 0 && strcmp(Name, other.Name) == 0;
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
