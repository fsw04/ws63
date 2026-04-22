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

#include "DeviceAttestationCredsProviderImpl.h"
#include <lib/support/CodeUtils.h>

using namespace chip;
using namespace chip::Crypto;

namespace chip {
namespace DeviceLayer {
// 全局实例
DeviceAttestationCredsProviderImpl gDeviceAttestationCredsProvider;
CHIP_ERROR DeviceAttestationCredsProviderImpl::GetCertificationDeclaration(MutableByteSpan & out_cd_buffer)
{
    return CertificateStoreManager::GetInstance().GetCertificationDeclaration(out_cd_buffer);
}

CHIP_ERROR DeviceAttestationCredsProviderImpl::GetFirmwareInformation(MutableByteSpan & out_firmware_info_buffer)
{
    // 返回空，表示没有额外的固件信息。
    out_firmware_info_buffer.reduce_size(0);
    return CHIP_NO_ERROR;
}

CHIP_ERROR DeviceAttestationCredsProviderImpl::GetDeviceAttestationCert(MutableByteSpan & out_dac_buffer)
{
    return CertificateStoreManager::GetInstance().GetDeviceAttestationCertificate(out_dac_buffer);
}

CHIP_ERROR DeviceAttestationCredsProviderImpl::GetProductAttestationIntermediateCert(MutableByteSpan & out_pai_buffer)
{
    return CertificateStoreManager::GetInstance().GetProductAttestationIntermediate(out_pai_buffer);
}

CHIP_ERROR DeviceAttestationCredsProviderImpl::SignWithDeviceAttestationKey(const ByteSpan & message_to_sign,
                                                                            MutableByteSpan & out_signature)
{
    // 使用正确的 P256ECDSASignature 类型
    P256ECDSASignature signature;
    CHIP_ERROR err = CertificateStoreManager::GetInstance().SignWithDeviceKey(message_to_sign, signature);
    if (err != CHIP_NO_ERROR)
    {
        return err;
    }

    // 将签名复制到输出缓冲区
    return CopySpanToMutableSpan(ByteSpan(signature.ConstBytes(), signature.Length()), out_signature);
}
} // namespace DeviceLayer
} // namespace chip