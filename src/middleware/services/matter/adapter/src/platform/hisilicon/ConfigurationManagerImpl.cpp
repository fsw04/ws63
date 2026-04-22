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
 *          Provides the implementation of the Device Layer ConfigurationManager object
 *          for the Hisilicon.
 */
/* this file behaves like a config.h, comes first */
#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <platform/hisilicon/HisiConfig.h>
#include <crypto/RandUtils.h>
#include <platform/hisilicon/DeviceAttestationCredsProviderImpl.h>
#include <platform/hisilicon/CertificateStoreManager.h>
#include <platform/ConfigurationManager.h>
#include <platform/DiagnosticDataProvider.h>
#include <platform/internal/GenericConfigurationManagerImpl.ipp>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>


namespace chip {
namespace DeviceLayer {

using namespace chip::DeviceLayer::Internal;
using namespace chip::Crypto;

ConfigurationManagerImpl & ConfigurationManagerImpl::GetDefaultInstance()
{
    static ConfigurationManagerImpl sInstance;
    return sInstance;
}

CHIP_ERROR ConfigurationManagerImpl::Init()
{
    ChipLogProgress(DeviceLayer, "ConfigurationManagerImpl::Init start");
    CHIP_ERROR err = CHIP_NO_ERROR;
    uint32_t rebootCount;

    // Save out software version on first boot
    if (!HisiConfig::ConfigValueExists(HisiConfig::kConfigKey_SoftwareVersion)) {
        err = StoreSoftwareVersion(CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION);
        SuccessOrExit(err);
    }

    if (HisiConfig::ConfigValueExists(HisiConfig::kCounterKey_RebootCount)) {
        err = GetRebootCount(rebootCount);
        SuccessOrExit(err);

        err = StoreRebootCount(rebootCount + 1);
        SuccessOrExit(err);
    } else {
        // The first boot after factory reset of the Node.
        err = StoreRebootCount(1);
        SuccessOrExit(err);
    }

    if (!HisiConfig::ConfigValueExists(HisiConfig::kCounterKey_TotalOperationalHours)) {
        err = StoreTotalOperationalHours(0);
        SuccessOrExit(err);
    }

    // Initialize the generic implementation base class.
    err = Internal::GenericConfigurationManagerImpl<HisiConfig>::Init();
    VerifyOrReturnError(CHIP_NO_ERROR == err, err);
    // 设置设备认证凭证提供者，Matter配网流程会调用它
    Credentials::SetDeviceAttestationCredentialsProvider(&gDeviceAttestationCredsProvider);

    // 验证设备证书链是否就绪
    err = CertificateStoreManager::GetInstance().ValidateCertificateChain();
    if (err != CHIP_NO_ERROR) {
        ChipLogError(DeviceLayer, "Certificate chain validation failed: %" CHIP_ERROR_FORMAT, err.Format());
    }
    ChipLogProgress(DeviceLayer, "ConfigurationManagerImpl::Init OK");
exit:
    return err;
}

CHIP_ERROR ConfigurationManagerImpl::GetRebootCount(uint32_t & rebootCount)
{
    return ReadConfigValue(HisiConfig::kCounterKey_RebootCount, rebootCount);
}

CHIP_ERROR ConfigurationManagerImpl::StoreRebootCount(uint32_t rebootCount)
{
    return WriteConfigValue(HisiConfig::kCounterKey_RebootCount, rebootCount);
}

CHIP_ERROR ConfigurationManagerImpl::GetSoftwareVersion(uint32_t & softwareVer)
{
    return ReadConfigValue(HisiConfig::kConfigKey_SoftwareVersion, softwareVer);
}

CHIP_ERROR ConfigurationManagerImpl::StoreSoftwareVersion(uint32_t softwareVer)
{
    return WriteConfigValue(HisiConfig::kConfigKey_SoftwareVersion, softwareVer);
}

CHIP_ERROR ConfigurationManagerImpl::GetTotalOperationalHours(uint32_t & totalOperationalHours)
{
    return ReadConfigValue(HisiConfig::kCounterKey_TotalOperationalHours, totalOperationalHours);
}

CHIP_ERROR ConfigurationManagerImpl::StoreTotalOperationalHours(uint32_t totalOperationalHours)
{
    return WriteConfigValue(HisiConfig::kCounterKey_TotalOperationalHours, totalOperationalHours);
}

CHIP_ERROR ConfigurationManagerImpl::GetBootReason(uint32_t & bootReason)
{
    return WriteConfigValue(HisiConfig::kCounterKey_BootReason, bootReason);
}

CHIP_ERROR ConfigurationManagerImpl::StoreBootReason(uint32_t bootReason)
{
    return ReadConfigValue(HisiConfig::kCounterKey_BootReason, bootReason);
}

CHIP_ERROR ConfigurationManagerImpl::GetPrimaryWiFiMACAddress(uint8_t * buf)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

bool ConfigurationManagerImpl::CanFactoryReset()
{
    return true;
}

void ConfigurationManagerImpl::InitiateFactoryReset()
{
    PlatformMgr().ScheduleWork(DoFactoryReset);
}

CHIP_ERROR ConfigurationManagerImpl::ReadPersistedStorageValue(
    ::chip::Platform::PersistedStorage::Key key, uint32_t & value)
{
    uint32_t in    = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get(key, &in, 4);
    value          = in;
    return err;
}

CHIP_ERROR ConfigurationManagerImpl::WritePersistedStorageValue(
    ::chip::Platform::PersistedStorage::Key key, uint32_t value)
{
    return PersistedStorage::KeyValueStoreMgr().Put(key, static_cast<void *>(&value), 4);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValue(Key key, bool & val)
{
    return HisiConfig::ReadConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValue(Key key, uint32_t & val)
{
    return HisiConfig::ReadConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValue(Key key, uint64_t & val)
{
    return HisiConfig::ReadConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValueStr(Key key, char * buf, size_t bufSize, size_t & outLen)
{
    return HisiConfig::ReadConfigValueStr(key, buf, bufSize, outLen);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen)
{
    return HisiConfig::ReadConfigValueBin(key, buf, bufSize, outLen);
}
CHIP_ERROR ConfigurationManagerImpl::WriteConfigValue(Key key, bool val)
{
    return HisiConfig::WriteConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValue(Key key, uint32_t val)
{
    return HisiConfig::WriteConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValue(Key key, uint64_t val)
{
    return HisiConfig::WriteConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValueStr(Key key, const char * str)
{
    return HisiConfig::WriteConfigValueStr(key, str);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValueStr(Key key, const char * str, size_t strLen)
{
    return HisiConfig::WriteConfigValueStr(key, str, strLen);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValueBin(Key key, const uint8_t * data, size_t dataLen)
{
    return HisiConfig::WriteConfigValueBin(key, data, dataLen);
}

void ConfigurationManagerImpl::RunConfigUnitTest(void)
{
    HisiConfig::RunConfigUnitTest();
}

void ConfigurationManagerImpl::DoFactoryReset(intptr_t arg)
{
    CHIP_ERROR err;

    ChipLogProgress(DeviceLayer, "Performing factory reset");

    err = HisiConfig::FactoryResetConfig();
    if (err != CHIP_NO_ERROR) {
        ChipLogError(DeviceLayer, "FactoryResetConfig() failed: %s", ErrorStr(err));
    }
}

ConfigurationManager & ConfigurationMgrImpl()
{
    return ConfigurationManagerImpl::GetDefaultInstance();
}

} // namespace DeviceLayer
} // namespace chip
