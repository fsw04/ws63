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

#include "CertificateStoreManager.h"
#include <platform/internal/CHIPDeviceLayerInternal.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <crypto/CHIPCryptoPAL.h>

using namespace chip;
using namespace chip::DeviceLayer;
using namespace chip::Crypto;

namespace chip {
namespace DeviceLayer {

CertificateStoreManager & CertificateStoreManager::GetInstance()
{
    static CertificateStoreManager instance;
    return instance;
}

CHIP_ERROR CertificateStoreManager::FactoryProvisionCertificates(
    const uint8_t * cdData, size_t cdLen,
    const uint8_t * dacData, size_t dacLen, 
    const uint8_t * paiData, size_t paiLen,
    const uint8_t * privateKeyData, size_t privateKeyLen)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    
    ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put("chip-config;cert-declaration", cdData, cdLen));
    ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put("chip-factory;device-cert", dacData, dacLen));
    ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put("chip-factory;device-ca-certs", paiData, paiLen));
    ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put("chip-factory;device-key", privateKeyData, privateKeyLen));
        
    ChipLogProgress(DeviceLayer, "Factory certificates provisioned successfully");
    mCertificatesValidated = false;
    return err;
}

bool CertificateStoreManager::AreCertificatesProvisioned()
{
    uint8_t testBuffer[1024]; // 1024 is max length of DAC
    size_t outLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-factory;device-cert", testBuffer, sizeof(testBuffer), &outLen);
    // Clear sensitive data buffer
    memset_s(testBuffer, sizeof(testBuffer), 0, sizeof(testBuffer));
    return (err == CHIP_NO_ERROR || err == CHIP_ERROR_BUFFER_TOO_SMALL);
}

CHIP_ERROR CertificateStoreManager::ValidateCertificateChain()
{
    if (!AreCertificatesProvisioned()) {
        return CHIP_ERROR_CERT_LOAD_FAILED;
    }
    mCertificatesValidated = true;
    ChipLogProgress(DeviceLayer, "Certificate chain validation completed (simulated).");
    return CHIP_NO_ERROR;
}

CHIP_ERROR CertificateStoreManager::GetCertificationDeclaration(MutableByteSpan & outBuffer)
{
    size_t cdLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-config;cert-declaration", outBuffer.data(), outBuffer.size(), &cdLen);
    if (err == CHIP_NO_ERROR) {
        outBuffer.reduce_size(cdLen);
    }
    return err;
}

CHIP_ERROR CertificateStoreManager::GetDeviceAttestationCertificate(MutableByteSpan & outBuffer)
{
    size_t dacLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-factory;device-cert", outBuffer.data(), outBuffer.size(), &dacLen);
    if (err == CHIP_NO_ERROR) {
        outBuffer.reduce_size(dacLen);
    }
    return err;
}

CHIP_ERROR CertificateStoreManager::GetProductAttestationIntermediate(MutableByteSpan & outBuffer)
{
    size_t paiLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-factory;device-ca-certs", outBuffer.data(), outBuffer.size(), &paiLen);
    if (err == CHIP_NO_ERROR) {
        outBuffer.reduce_size(paiLen);
    }
    return err;
}

CHIP_ERROR CertificateStoreManager::GetDevicePrivateKey(MutableByteSpan & outBuffer)
{
    size_t keyLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-factory;device-key", outBuffer.data(), outBuffer.size(), &keyLen);
    if (err == CHIP_NO_ERROR) {
        outBuffer.reduce_size(keyLen);
    }
    return err;
}

CHIP_ERROR CertificateStoreManager::GetDevicePublicKey(MutableByteSpan & outBuffer)
{
    size_t keyLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-factory;device-pkey", outBuffer.data(), outBuffer.size(), &keyLen);
    if (err == CHIP_NO_ERROR) {
        outBuffer.reduce_size(keyLen);
    }
    return err;
}

CHIP_ERROR LoadKeypairFromRaw(ByteSpan privateKey, ByteSpan publicKey, Crypto::P256Keypair & keypair)
{
    // Check input parameters
    if (privateKey.empty() || publicKey.empty()) {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    Crypto::P256SerializedKeypair serializedKeypair;
    
    // Calculate the total length and set the serialization buffer length
    size_t totalSize = privateKey.size() + publicKey.size();
    ReturnErrorOnFailure(serializedKeypair.SetLength(totalSize));
    
    // Note: Here the public key comes first, followed by the private key. This is a common serialization format.
    if (memcpy_s(serializedKeypair.Bytes(), serializedKeypair.Length(), 
                 publicKey.data(), publicKey.size()) != 0) {
        return CHIP_ERROR_INTERNAL;
    }
    
    if (memcpy_s(serializedKeypair.Bytes() + publicKey.size(), 
                 serializedKeypair.Length() - publicKey.size(),
                 privateKey.data(), privateKey.size()) != 0) {
        // Clear the buffer if copying fails
        memset_s(serializedKeypair.Bytes(), serializedKeypair.Length(), 0, serializedKeypair.Length());
        return CHIP_ERROR_INTERNAL;
    }
    
    CHIP_ERROR err = keypair.Deserialize(serializedKeypair);
    
    // Regardless of success or failure, clear sensitive data from the serialization buffer
    memset_s(serializedKeypair.Bytes(), serializedKeypair.Length(), 0, serializedKeypair.Length());
    
    return err;
}

CHIP_ERROR CertificateStoreManager::SignWithDeviceKey(const ByteSpan & messageToSign, Crypto::P256ECDSASignature & outSignature)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    uint8_t privateKeyBuf[kP256_PrivateKey_Length] = {0};
    uint8_t publicKeyBuf[256] = {0};  // 256 is max length of public key

    // Check input parameters
    VerifyOrReturnError(!messageToSign.empty(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(outSignature.Capacity() >= Crypto::kP256_ECDSA_Signature_Length_Raw, 
                        CHIP_ERROR_BUFFER_TOO_SMALL);

    ChipLogProgress(DeviceLayer, "Starting device key signing");

    // Read the private key from NV
    MutableByteSpan privateKeySpan(privateKeyBuf, sizeof(privateKeyBuf));
    err = GetDevicePrivateKey(privateKeySpan);
    if (err != CHIP_NO_ERROR) {
        // Reset the buffer to zero if reading fails
        memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
        memset_s(publicKeyBuf, sizeof(publicKeyBuf), 0, sizeof(publicKeyBuf));
        return err;
    }

    // Read the public key from NV
    MutableByteSpan publicKeySpan(publicKeyBuf, sizeof(publicKeyBuf));
    err = GetDevicePublicKey(publicKeySpan);
    if (err != CHIP_NO_ERROR) {
        // Clear the buffer when reading fails
        memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
        memset_s(publicKeyBuf, sizeof(publicKeyBuf), 0, sizeof(publicKeyBuf));
        return err;
    }

    // Check private key length
    if (privateKeySpan.size() != kP256_PrivateKey_Length) {
        ChipLogError(DeviceLayer, "Private key size mismatch: expected %d, got %d", 
                     kP256_PrivateKey_Length, privateKeySpan.size());
        memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
        memset_s(publicKeyBuf, sizeof(publicKeyBuf), 0, sizeof(publicKeyBuf));
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // Create key pair
    Crypto::P256Keypair keypair;

    // Load key pair using LoadKeypairFromRaw
    err = LoadKeypairFromRaw(privateKeySpan, publicKeySpan, keypair);
    if (err != CHIP_NO_ERROR) {
        ChipLogError(DeviceLayer, "Failed to load keypair from raw data: %" CHIP_ERROR_FORMAT, err.Format());
        memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
        memset_s(publicKeyBuf, sizeof(publicKeyBuf), 0, sizeof(publicKeyBuf));
        return err;
    }

    err = keypair.ECDSA_sign_msg(messageToSign.data(), messageToSign.size(), outSignature);
    if (err != CHIP_NO_ERROR) {
        ChipLogError(DeviceLayer, "Failed to sign message: %" CHIP_ERROR_FORMAT, err.Format());
        // Note: The private key is not cleared here because the serialization buffer has already been cleared by LoadKeypairFromRaw.
    }

    // Clear the key data in the local buffer
    memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
    memset_s(publicKeyBuf, sizeof(publicKeyBuf), 0, sizeof(publicKeyBuf));

    if (err == CHIP_NO_ERROR) {
        ChipLogProgress(DeviceLayer, "Signature generated successfully");
    }

    return err;
}

CHIP_ERROR CertificateStoreManager::StoreOperationalCertificates(
    const uint8_t * nocData, size_t nocLen,
    const uint8_t * icaData, size_t icaLen, 
    const uint8_t * opPrivateKeyData, size_t opPrivateKeyLen)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    
    if (nocData && nocLen > 0) {
        ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put("chip-config;op-device-cert", nocData, nocLen));
    }
    
    if (icaData && icaLen > 0) {
        ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put("chip-config;op-device-ca-certs", icaData, icaLen));
    }
    
    if (opPrivateKeyData && opPrivateKeyLen > 0) {
        ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put("chip-config;op-device-key", opPrivateKeyData, opPrivateKeyLen));
    }
    
    ChipLogProgress(DeviceLayer, "Operational certificates provisioned successfully");
    mOperationalCertsValidated = false;
    return err;
}

CHIP_ERROR CertificateStoreManager::GetNodeOperationalCertificate(MutableByteSpan & outBuffer)
{
    size_t nocLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-config;op-device-cert", outBuffer.data(), outBuffer.size(), &nocLen);
    if (err == CHIP_NO_ERROR) {
        outBuffer.reduce_size(nocLen);
    }
    return err;
}

CHIP_ERROR CertificateStoreManager::GetIntermediateCACertificate(MutableByteSpan & outBuffer)
{
    size_t icaLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-config;op-device-ca-certs", outBuffer.data(), outBuffer.size(), &icaLen);
    if (err == CHIP_NO_ERROR) {
        outBuffer.reduce_size(icaLen);
    }
    return err;
}

CHIP_ERROR CertificateStoreManager::GetOperationalPrivateKey(MutableByteSpan & outBuffer)
{
    size_t keyLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-config;op-device-key", outBuffer.data(), outBuffer.size(), &keyLen);
    if (err == CHIP_NO_ERROR) {
        outBuffer.reduce_size(keyLen);
    }
    return err;
}

bool CertificateStoreManager::AreOperationalCertificatesProvisioned()
{
    uint8_t testBuffer[1024]; // 1024 is max length of NOC
    size_t outLen = 0;
    CHIP_ERROR err = PersistedStorage::KeyValueStoreMgr().Get("chip-config;op-device-cert", testBuffer, sizeof(testBuffer), &outLen);
    memset_s(testBuffer, sizeof(testBuffer), 0, sizeof(testBuffer));
    return (err == CHIP_NO_ERROR || err == CHIP_ERROR_BUFFER_TOO_SMALL);
}

CHIP_ERROR CertificateStoreManager::SignWithOperationalKey(const ByteSpan & messageToSign, Crypto::P256ECDSASignature & outSignature)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    uint8_t privateKeyBuf[kP256_PrivateKey_Length] = {0};  // 初始化清零

    // Read operational private key from NV
    MutableByteSpan privateKeySpan(privateKeyBuf, sizeof(privateKeyBuf));
    err = GetOperationalPrivateKey(privateKeySpan);
    if (err != CHIP_NO_ERROR) {
        memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
        return err;
    }

    if (privateKeySpan.size() != kP256_PrivateKey_Length) {
        memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // Create a P256SerializedKeypair object
    P256SerializedKeypair serializedKeypair;
    if (privateKeySpan.size() > serializedKeypair.Capacity()) {
        memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    
    if (memcpy_s(serializedKeypair.Bytes(), serializedKeypair.Capacity(), 
                 privateKeySpan.data(), privateKeySpan.size()) != 0) {
        memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
        memset_s(serializedKeypair.Bytes(), serializedKeypair.Capacity(), 0, serializedKeypair.Capacity());
        return CHIP_ERROR_INTERNAL;
    }
    
    serializedKeypair.SetLength(privateKeySpan.size());

    // Sign using the operational private key
    P256Keypair keypair;
    err = keypair.Deserialize(serializedKeypair);
    
    // Regardless of success or failure, clear sensitive data from the serialization buffer.
    memset_s(serializedKeypair.Bytes(), serializedKeypair.Capacity(), 0, serializedKeypair.Capacity());
    
    if (err != CHIP_NO_ERROR) {
        memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));
        return err;
    }

    // Signature
    err = keypair.ECDSA_sign_msg(messageToSign.data(), messageToSign.size(), outSignature);
    
    // Clear the private key data in the local buffer
    memset_s(privateKeyBuf, sizeof(privateKeyBuf), 0, sizeof(privateKeyBuf));

    return err;
}
} // namespace DeviceLayer
} // namespace chip