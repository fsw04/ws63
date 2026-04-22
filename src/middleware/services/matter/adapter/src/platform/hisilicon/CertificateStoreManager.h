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

#include <lib/core/CHIPError.h>
#include <platform/KeyValueStoreManager.h>
#include <crypto/CHIPCryptoPAL.h>

namespace chip {
namespace DeviceLayer {

// 前向声明
using Crypto::P256ECDSASignature;

class CertificateStoreManager
{
public:
    static CertificateStoreManager & GetInstance();
    
    // ==== 证书生命周期管理 ====
    CHIP_ERROR FactoryProvisionCertificates(const uint8_t * cdData, size_t cdLen,
                                           const uint8_t * dacData, size_t dacLen,
                                           const uint8_t * paiData, size_t paiLen,
                                           const uint8_t * privateKeyData, size_t privateKeyLen);
    bool AreCertificatesProvisioned();
    CHIP_ERROR ValidateCertificateChain();

    // ==== 证书读取接口 ====
    CHIP_ERROR GetCertificationDeclaration(MutableByteSpan & outBuffer);
    CHIP_ERROR GetDeviceAttestationCertificate(MutableByteSpan & outBuffer);
    CHIP_ERROR GetProductAttestationIntermediate(MutableByteSpan & outBuffer);
    CHIP_ERROR GetDevicePrivateKey(MutableByteSpan & outBuffer);
    CHIP_ERROR GetDevicePublicKey(MutableByteSpan & outBuffer);
    // ==== 密码学操作 ====
    CHIP_ERROR SignWithDeviceKey(const ByteSpan & messageToSign, Crypto::P256ECDSASignature & outSignature);

    // ==== NOC操作证书管理 ====
    CHIP_ERROR StoreOperationalCertificates(const uint8_t * nocData, size_t nocLen,
                                           const uint8_t * icaData, size_t icaLen,
                                           const uint8_t * opPrivateKeyData, size_t opPrivateKeyLen);
    
    CHIP_ERROR GetNodeOperationalCertificate(MutableByteSpan & outBuffer);
    CHIP_ERROR GetIntermediateCACertificate(MutableByteSpan & outBuffer);
    CHIP_ERROR GetOperationalPrivateKey(MutableByteSpan & outBuffer);
    
    bool AreOperationalCertificatesProvisioned();
    CHIP_ERROR SignWithOperationalKey(const ByteSpan & messageToSign, Crypto::P256ECDSASignature & outSignature);

private:
    CertificateStoreManager() = default;
    bool mCertificatesValidated = false;
    bool mOperationalCertsValidated = false;
};

} // namespace DeviceLayer
} // namespace chip