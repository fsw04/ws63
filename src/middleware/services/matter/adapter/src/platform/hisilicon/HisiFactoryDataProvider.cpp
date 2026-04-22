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

#include "HisiFactoryDataProvider.h"

#include <lib/core/CHIPError.h>
#include <lib/support/Base64.h>
#include <lib/support/BytesToHex.h>
#include <lib/support/Span.h>
#include <platform/hisilicon/CHIPDevicePlatformConfig.h>
#include <platform/CHIPDeviceConfig.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/ConnectivityManager.h>
#include <platform/internal/CHIPDeviceLayerInternal.h>
#include <platform/internal/GenericConfigurationManagerImpl.ipp>
#include <platform/hisilicon/HisiConfig.h>

using namespace ::chip::DeviceLayer::Internal;
using namespace chip::app::Clusters::BasicInformation;

namespace chip {
namespace DeviceLayer {

CHIP_ERROR HisiFactoryDataProvider::Init()
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    return err;
}

CHIP_ERROR HisiFactoryDataProvider::GetSetupDiscriminator(uint16_t & setupDiscriminator)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    uint32_t setupDiscriminator32;
    err = HisiConfig::ReadConfigValue(HisiConfig::kConfigKey_SetupDiscriminator, setupDiscriminator32);
#if defined(CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR) && CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        setupDiscriminator32 = CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR;
        err = CHIP_NO_ERROR;
    }
#endif // defined(CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR) && CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR
    SuccessOrExit(err);
    setupDiscriminator = static_cast<uint16_t>(setupDiscriminator32);
exit:
    return err;
}

CHIP_ERROR HisiFactoryDataProvider::SetSetupDiscriminator(uint16_t setupDiscriminator)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetSpake2pIterationCount(uint32_t & iterationCount)
{
    iterationCount = 1000; // 1000 is the minimum number of iterations
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetSpake2pSalt(MutableByteSpan & saltBuf)
{
    static constexpr size_t kSpake2pSalt_MaxBase64Len =
        BASE64_ENCODED_LEN(chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length) + 1;

    CHIP_ERROR err                          = CHIP_NO_ERROR;
    char saltB64[kSpake2pSalt_MaxBase64Len] = { 0 };
    size_t saltB64Len                       = 0;

    saltB64Len = strlen(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_SALT);
    VerifyOrReturnError(saltB64Len <= sizeof(saltB64), CHIP_ERROR_BUFFER_TOO_SMALL);
    memcpy_s(saltB64, saltB64Len, CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_SALT, saltB64Len);

    size_t saltLen = chip::Base64Decode32(saltB64, saltB64Len, reinterpret_cast<uint8_t *>(saltB64));
    VerifyOrReturnError(saltLen <= saltBuf.size(), CHIP_ERROR_BUFFER_TOO_SMALL);

    memcpy_s(saltBuf.data(), saltBuf.size(), saltB64, saltLen);
    saltBuf.reduce_size(saltLen);

    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetSpake2pVerifier(MutableByteSpan & verifierBuf, size_t & verifierLen)
{
    static constexpr size_t kSpake2pSerializedVerifier_MaxBase64Len =
        BASE64_ENCODED_LEN(chip::Crypto::kSpake2p_VerifierSerialized_Length) + 1;

    CHIP_ERROR err                                            = CHIP_NO_ERROR;
    char verifierB64[kSpake2pSerializedVerifier_MaxBase64Len] = { 0 };
    size_t verifierB64Len                                     = 0;

    verifierB64Len = strlen(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_VERIFIER);
    VerifyOrReturnError(verifierB64Len <= sizeof(verifierB64), CHIP_ERROR_BUFFER_TOO_SMALL);
    memcpy_s(verifierB64, verifierB64Len,  CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_VERIFIER, verifierB64Len);

    verifierLen = chip::Base64Decode32(verifierB64, verifierB64Len, reinterpret_cast<uint8_t *>(verifierB64));
    VerifyOrReturnError(verifierLen <= verifierBuf.size(), CHIP_ERROR_BUFFER_TOO_SMALL);

    memcpy_s(verifierBuf.data(), verifierBuf.size(), verifierB64, verifierLen);
    verifierBuf.reduce_size(verifierLen);

    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetSetupPasscode(uint32_t & setupPasscode)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    uint32_t passCode;
    err = HisiConfig::ReadConfigValue(HisiConfig::kConfigKey_SetupPinCode, passCode);
#if defined(CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE) && CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        passCode = CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE;
        err = CHIP_NO_ERROR;
    }
#endif // defined(CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE) && CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE
    SuccessOrExit(err);
    setupPasscode = static_cast<uint32_t>(passCode);
exit:
    return err;
}

CHIP_ERROR HisiFactoryDataProvider::SetSetupPasscode(uint32_t setupPasscode)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetVendorName(char * buf, size_t bufSize)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetVendorId(uint16_t & vendorId)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetProductName(char * buf, size_t bufSize)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetProductId(uint16_t & productId)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetPartNumber(char * buf, size_t bufSize)
{
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR HisiFactoryDataProvider::GetProductURL(char * buf, size_t bufSize)
{
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR HisiFactoryDataProvider::GetProductLabel(char * buf, size_t bufSize)
{
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR HisiFactoryDataProvider::GetSerialNumber(char * buf, size_t bufSize)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day)
{
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR HisiFactoryDataProvider::GetHardwareVersion(uint16_t & hardwareVersion)
{
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR HisiFactoryDataProvider::GetHardwareVersionString(char * buf, size_t bufSize)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetRotatingDeviceIdUniqueId(MutableByteSpan & uniqueIdSpan)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetProductFinish(ProductFinishEnum * finish)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiFactoryDataProvider::GetProductPrimaryColor(ColorEnum * primaryColor)
{
    return CHIP_NO_ERROR;
}

} // namespace DeviceLayer
} // namespace chip
