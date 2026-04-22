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

/* this file behaves like a config.h, comes first */
#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <lib/core/CHIPEncoding.h>
#include <platform/hisilicon/HisiConfig.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CHIPMemString.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/hisilicon/KeyValueStoreManagerImpl.h>

using namespace chip::DeviceLayer::PersistedStorage;

namespace chip {
namespace DeviceLayer {
namespace Internal {

// *** CAUTION ***: Changing the names or namespaces of these values will *break* existing devices.

// NVS namespaces used to store device configuration information.
const char HisiConfig::kConfigNamespace_ChipFactory[]  = "chip-factory";
const char HisiConfig::kConfigNamespace_ChipConfig[]   = "chip-config";
const char HisiConfig::kConfigNamespace_ChipCounters[] = "chip-counters";

// Keys stored in the chip-factory namespace
const HisiConfig::Key HisiConfig::kConfigKey_SerialNum             = { kConfigNamespace_ChipFactory, "serial-num" };
const HisiConfig::Key HisiConfig::kConfigKey_MfrDeviceId           = { kConfigNamespace_ChipFactory, "device-id" };
const HisiConfig::Key HisiConfig::kConfigKey_CertDeclaration       = { kConfigNamespace_ChipConfig, "cert-declaration" };
const HisiConfig::Key HisiConfig::kConfigKey_MfrDeviceCert         = { kConfigNamespace_ChipFactory, "device-cert" };
const HisiConfig::Key HisiConfig::kConfigKey_MfrDeviceICACerts    = { kConfigNamespace_ChipFactory, "device-ca-certs"};
const HisiConfig::Key HisiConfig::kConfigKey_MfrDevicePrivateKey   = { kConfigNamespace_ChipFactory, "device-key" };
const HisiConfig::Key HisiConfig::kConfigKey_HardwareVersion       = { kConfigNamespace_ChipFactory, "hardware-ver" };
const HisiConfig::Key HisiConfig::kConfigKey_ManufacturingDate     = { kConfigNamespace_ChipFactory, "mfg-date" };
const HisiConfig::Key HisiConfig::kConfigKey_SetupPinCode          = { kConfigNamespace_ChipFactory, "pin-code" };
const HisiConfig::Key HisiConfig::kConfigKey_SetupDiscriminator    = { kConfigNamespace_ChipFactory, "discriminator" };
const HisiConfig::Key HisiConfig::kConfigKey_Spake2pIterationCount = { kConfigNamespace_ChipFactory, "iteration-count"};
const HisiConfig::Key HisiConfig::kConfigKey_Spake2pSalt           = { kConfigNamespace_ChipFactory, "salt" };
const HisiConfig::Key HisiConfig::kConfigKey_Spake2pVerifier       = { kConfigNamespace_ChipFactory, "verifier" };
const HisiConfig::Key HisiConfig::kConfigKey_UniqueId              = { kConfigNamespace_ChipFactory, "uniqueId" };
const HisiConfig::Key HisiConfig::kConfigKey_SoftwareVersion       = { kConfigNamespace_ChipFactory, "software-ver" };

// Keys stored in the chip-config namespace
const HisiConfig::Key HisiConfig::kConfigKey_FabricId               = { kConfigNamespace_ChipConfig, "fabric-id" };
const HisiConfig::Key HisiConfig::kConfigKey_ServiceConfig          = { kConfigNamespace_ChipConfig, "service-config" };
const HisiConfig::Key HisiConfig::kConfigKey_PairedAccountId        = { kConfigNamespace_ChipConfig, "account-id" };
const HisiConfig::Key HisiConfig::kConfigKey_ServiceId              = { kConfigNamespace_ChipConfig, "service-id" };
const HisiConfig::Key HisiConfig::kConfigKey_GroupKeyIndex          = { kConfigNamespace_ChipConfig, "group-key-index"};
const HisiConfig::Key HisiConfig::kConfigKey_LastUsedEpochKeyId     = { kConfigNamespace_ChipConfig, "last-ek-id" };
const HisiConfig::Key HisiConfig::kConfigKey_FailSafeArmed          = { kConfigNamespace_ChipConfig, "fail-safe-armed"};
const HisiConfig::Key HisiConfig::kConfigKey_WiFiStationSecType     = { kConfigNamespace_ChipConfig, "sta-sec-type" };
const HisiConfig::Key HisiConfig::kConfigKey_OperationalDeviceId    = { kConfigNamespace_ChipConfig, "op-device-id" };
const HisiConfig::Key HisiConfig::kConfigKey_OperationalDeviceCert  = { kConfigNamespace_ChipConfig, "op-device-cert"};
const HisiConfig::Key HisiConfig::kConfigKey_OperationalDeviceICACerts
                                    = { kConfigNamespace_ChipConfig, "op-device-ca-certs" };
const HisiConfig::Key HisiConfig::kConfigKey_OperationalDevicePrivateKey
                                    = { kConfigNamespace_ChipConfig, "op-device-key" };
const HisiConfig::Key HisiConfig::kConfigKey_RegulatoryLocation = { kConfigNamespace_ChipConfig, "regulatory-location"};
const HisiConfig::Key HisiConfig::kConfigKey_CountryCode        = { kConfigNamespace_ChipConfig, "country-code" };
const HisiConfig::Key HisiConfig::kConfigKey_Breadcrumb         = { kConfigNamespace_ChipConfig, "breadcrumb" };
const HisiConfig::Key HisiConfig::kConfigKey_WiFiSSID           = { kConfigNamespace_ChipConfig, "wifi-ssid" };
const HisiConfig::Key HisiConfig::kConfigKey_WiFiPassword       = { kConfigNamespace_ChipConfig, "wifi-password" };
const HisiConfig::Key HisiConfig::kConfigKey_WiFiSecurity       = { kConfigNamespace_ChipConfig, "wifi-security" };
const HisiConfig::Key HisiConfig::kConfigKey_WiFiMode           = { kConfigNamespace_ChipConfig, "wifimode" };


// Keys stored in the Chip-counters namespace
const HisiConfig::Key HisiConfig::kCounterKey_RebootCount           = { kConfigNamespace_ChipCounters, "reboot-count" };
const HisiConfig::Key HisiConfig::kCounterKey_UpTime                = { kConfigNamespace_ChipCounters, "up-time" };
const HisiConfig::Key HisiConfig::kCounterKey_TotalOperationalHours = { kConfigNamespace_ChipCounters, "total-hours" };
const HisiConfig::Key HisiConfig::kCounterKey_BootReason            = { kConfigNamespace_ChipCounters, "boot-reason" };

constexpr int KVSTORE_MAX_KEY_SIZE = 64;

CHIP_ERROR HisiConfig::ReadConfigValue(Key key, bool & val)
{
    bool in;
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get(key_str, static_cast<void *>(&in), sizeof(bool));
    val            = in;
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        ChipLogError(DeviceLayer, "KeyValueStoreMgr().Get bool failed");
        return err;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiConfig::ReadConfigValue(Key key, uint32_t & val)
{
    uint32_t in;
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get(key_str, static_cast<void *>(&in), 4);
    val            = in;
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        ChipLogError(DeviceLayer, "KeyValueStoreMgr().Get uint32_t failed");
        return err;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiConfig::ReadConfigValue(Key key, uint64_t & val)
{
    uint64_t in;
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get(key_str, static_cast<void *>(&in), 8);
    val            = in;
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        ChipLogError(DeviceLayer, "KeyValueStoreMgr().Get uint64_t failed");
        return err;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiConfig::ReadConfigValueStr(Key key, char * buf, size_t bufSize, size_t & outLen)
{
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get(key_str, buf, bufSize, &outLen);
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        return err;
    }

    // 特殊处理：记录证书和密钥的读取日志
    if (strstr(key_str, "cert") != nullptr || strstr(key_str, "key") != nullptr) {
        ChipLogProgress(DeviceLayer, "Read security data: %s, length: %d", key_str, outLen);
    }
    
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiConfig::ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen)
{
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get(key_str, buf, bufSize, &outLen);
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        return err;
    }

    // 特殊处理：记录二进制安全数据的读取日志
    if (strstr(key_str, "cert") != nullptr || strstr(key_str, "key") != nullptr) {
        ChipLogProgress(DeviceLayer, "Read binary security data: %s, length: %d", key_str, outLen);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiConfig::WriteConfigValue(Key key, bool val)
{
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    return PersistedStorage::KeyValueStoreMgr().Put(key_str, static_cast<void *>(&val), sizeof(bool));
}

CHIP_ERROR HisiConfig::WriteConfigValue(Key key, uint32_t val)
{
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    return PersistedStorage::KeyValueStoreMgr().Put(key_str, static_cast<void *>(&val), sizeof(uint32_t));
}

CHIP_ERROR HisiConfig::WriteConfigValue(Key key, uint64_t val)
{
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    return PersistedStorage::KeyValueStoreMgr().Put(key_str, static_cast<void *>(&val), sizeof(uint64_t));
}

CHIP_ERROR HisiConfig::WriteConfigValueStr(Key key, const char * str)
{
    size_t size                         = strlen(str) + 1;
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);

    // 特殊处理：记录证书和密钥的存储日志
    if (strstr(key_str, "cert") != nullptr || strstr(key_str, "key") != nullptr) {
        ChipLogProgress(DeviceLayer, "Storing security data: %s, length: %d", key_str, size);
    }
    return PersistedStorage::KeyValueStoreMgr().Put(key_str, str, size);
}

CHIP_ERROR HisiConfig::WriteConfigValueStr(Key key, const char * str, size_t strLen)
{
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);

    // 特殊处理：记录安全数据的存储日志
    if (strstr(key_str, "cert") != nullptr || strstr(key_str, "key") != nullptr) {
        ChipLogProgress(DeviceLayer, "Storing security data with length: %s, length: %d", key_str, strLen);
    }
    return PersistedStorage::KeyValueStoreMgr().Put(key_str, str, strLen);
}

CHIP_ERROR HisiConfig::WriteConfigValueBin(Key key, const uint8_t * data, size_t dataLen)
{
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);

    // 特殊处理：记录二进制安全数据的存储日志
    if (strstr(key_str, "cert") != nullptr || strstr(key_str, "key") != nullptr) {
        ChipLogProgress(DeviceLayer, "Storing binary security data: %s, length: %d", key_str, dataLen);

        // 对于证书和密钥，使用加密存储接口
        return KeyValueStoreMgrImpl()._PutEncrypted(key_str, static_cast<const void*>(data), dataLen);
    }

    return PersistedStorage::KeyValueStoreMgr().Put(key_str, static_cast<const void*>(data), dataLen);
}

CHIP_ERROR HisiConfig::ClearConfigValue(Key key)
{
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    return PersistedStorage::KeyValueStoreMgr().Delete(key_str);
}

bool HisiConfig::ConfigValueExists(Key key)
{
    char key_str[KVSTORE_MAX_KEY_SIZE] = { 0 };
    char buf[4];
    size_t outLen;
    key.to_str(key_str, KVSTORE_MAX_KEY_SIZE);
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get(key_str, buf, 4, &outLen);
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        return false;
    }

    return true;
}

CHIP_ERROR HisiConfig::EnsureNamespace(const char * ns)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR HisiConfig::ClearNamespace(const char * ns)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR HisiConfig::FactoryResetConfig(void)
{
    ChipLogProgress(DeviceLayer, "Do Factory Reset.");
    CHIP_ERROR err            = CHIP_NO_ERROR;
    constexpr int MATTER_NVREGION_IDX = 6;

    nv_restore_mode_t restore_mode = {0};
    restore_mode.region_mode[MATTER_NVREGION_IDX] = 1;
    uapi_nv_set_restore_mode_partitial(&restore_mode);

    return CHIP_NO_ERROR;
}

void HisiConfig::RunConfigUnitTest() {}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
