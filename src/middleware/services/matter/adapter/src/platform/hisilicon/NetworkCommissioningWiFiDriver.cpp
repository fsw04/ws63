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
#include <platform/hisilicon/NetworkCommissioningDriver.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/SafeInt.h>
#include <platform/CHIPDeviceLayer.h>

#include <limits>
#include <string>
#include "wifi_device.h"
#include "WifiUtils.h"
#include "lwip/netifapi.h"
#include "osal_addr.h"


using namespace ::chip;
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
namespace chip {
namespace DeviceLayer {
namespace NetworkCommissioning {

namespace {
constexpr char kWiFiSSIDKeyName[]        = "chip-config;wifi-ssid";
constexpr char kWiFiCredentialsKeyName[] = "chip-config;wifi-password";
} // namespace

CHIP_ERROR HisiWiFiDriver::Init(NetworkStatusChangeCallback * networkStatusChangeCallback)
{
    CHIP_ERROR err;
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::Init\r\n");

    err = GetSavedNetWorkConfig(&mSavedNetwork);

    mStagingNetwork        = mSavedNetwork;
    mpScanCallback         = nullptr;
    mpConnectCallback      = nullptr;
    mpStatusChangeCallback = networkStatusChangeCallback;
    return err;
}

CHIP_ERROR HisiWiFiDriver::GetSavedNetWorkConfig(WiFiNetwork * WifiNetconf)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s\r\n", __func__);
    CHIP_ERROR err;
    size_t ssidLen        = 0;
    size_t credentialsLen = 0;

    memset_s(WifiNetconf, sizeof(WiFiNetwork), 0x0, sizeof(WiFiNetwork));
    err = PersistedStorage::KeyValueStoreMgr().Get(kWiFiCredentialsKeyName, WifiNetconf->credentials,
                                                   sizeof(WifiNetconf->credentials), &credentialsLen);
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        return CHIP_NO_ERROR;
    }

    err = PersistedStorage::KeyValueStoreMgr().Get(
        kWiFiSSIDKeyName, WifiNetconf->ssid, sizeof(WifiNetconf->ssid), &ssidLen);
    if (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND) {
        return CHIP_NO_ERROR;
    }

    WifiNetconf->credentialsLen = credentialsLen;
    WifiNetconf->ssidLen        = ssidLen;

    return err;
}

void HisiWiFiDriver::Shutdown()
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s\r\n", __func__);
    mpStatusChangeCallback = nullptr;
}

CHIP_ERROR HisiWiFiDriver::CommitConfiguration()
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::CommitConfiguration\r\n");
    ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put(kWiFiSSIDKeyName, mStagingNetwork.ssid,
                                                                  mStagingNetwork.ssidLen));
    ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put(kWiFiCredentialsKeyName, mStagingNetwork.credentials,
                                                                  mStagingNetwork.credentialsLen));
    mSavedNetwork = mStagingNetwork;
    return CHIP_NO_ERROR;
}

CHIP_ERROR HisiWiFiDriver::RevertConfiguration()
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::RevertConfiguration\r\n");
    mStagingNetwork = mSavedNetwork;
    return CHIP_NO_ERROR;
}

bool HisiWiFiDriver::NetworkMatch(const WiFiNetwork & network, ByteSpan networkId)
{    // 确定是否是同一个网络
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::NetworkMatch\r\n");
    ChipLogProgress(NetworkProvisioning, "networkId ssid:%.*s", networkId.size(), networkId.data());
    ChipLogProgress(NetworkProvisioning, "network ssid:%.*s", network.ssidLen, network.ssid);
    bool isMactch = networkId.size() == network.ssidLen && memcmp(networkId.data(), network.ssid, network.ssidLen) == 0;
    return isMactch;
}

Status HisiWiFiDriver::AddOrUpdateNetwork(ByteSpan ssid, ByteSpan credentials, MutableCharSpan & outDebugText,
                                           uint8_t & outNetworkIndex)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::AddOrUpdateNetwork\r\n");
    outDebugText.reduce_size(0);
    outNetworkIndex = 0;

    VerifyOrReturnError(mStagingNetwork.ssidLen == 0 || NetworkMatch(mStagingNetwork, ssid), Status::kBoundsExceeded);

    VerifyOrReturnError(credentials.size() <= sizeof(mStagingNetwork.credentials), Status::kOutOfRange);

    VerifyOrReturnError(ssid.size() <= sizeof(mStagingNetwork.ssid), Status::kOutOfRange);

    memcpy_s(mStagingNetwork.credentials, DeviceLayer::Internal::kMaxWiFiKeyLength,
             credentials.data(), credentials.size());
    mStagingNetwork.credentialsLen = static_cast<decltype(mStagingNetwork.credentialsLen)>(credentials.size());

    memcpy_s(mStagingNetwork.ssid, DeviceLayer::Internal::kMaxWiFiSSIDLength, ssid.data(), ssid.size());
    mStagingNetwork.ssidLen = static_cast<decltype(mStagingNetwork.ssidLen)>(ssid.size());
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s  OK", __func__);

    return Status::kSuccess;
}

Status HisiWiFiDriver::RemoveNetwork(ByteSpan networkId, MutableCharSpan & outDebugText, uint8_t & outNetworkIndex)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::RemoveNetwork\r\n");

    outDebugText.reduce_size(0);
    outNetworkIndex = 0;
    VerifyOrReturnError(NetworkMatch(mStagingNetwork, networkId), Status::kNetworkIDNotFound);

    // Use empty ssid for representing invalid network
    mStagingNetwork.ssidLen = 0;
    return Status::kSuccess;
}

Status HisiWiFiDriver::ReorderNetwork(ByteSpan networkId, uint8_t index, MutableCharSpan & outDebugText)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::ReorderNetwork\r\n");
    // Only one network is supported now
    outDebugText.reduce_size(0);

    VerifyOrReturnError(index == 0, Status::kOutOfRange);

    VerifyOrReturnError(NetworkMatch(mStagingNetwork, networkId), Status::kNetworkIDNotFound);

    return Status::kSuccess;
}

CHIP_ERROR HisiWiFiDriver::ConnectWiFiNetwork(const char * ssid, uint8_t ssidLen, const char * key, uint8_t keyLen)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s\r\n", __func__);
    CHIP_ERROR err = CHIP_NO_ERROR;

    wifi_sta_config_stru sta_config = {0};
    memcpy_s(sta_config.ssid, WIFI_MAX_SSID_LEN, ssid, ssidLen);
    memcpy_s(sta_config.pre_shared_key, WIFI_MAX_KEY_LEN, key, keyLen);

    CHIP_ERROR ret = WifiUtilsConnectNetwork(&sta_config);
    if (ret != CHIP_NO_ERROR) {
        ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s WifiUtilsConnectNetwork failed.\r\n", __func__);
    }

    err = ConnectivityMgr().SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Enabled);

    return err;
}

void HisiWiFiDriver::OnConnectWiFiNetwork()
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::OnConnectWiFiNetwork Enter\r\n");
    if (mpConnectCallback) {
        CommitConfiguration();
        mpConnectCallback->OnResult(Status::kSuccess, CharSpan(), 0);
        mpConnectCallback = nullptr;
    }
}

void HisiWiFiDriver::ConnectNetwork(ByteSpan networkId, ConnectCallback * callback)
{
    CHIP_ERROR err          = CHIP_NO_ERROR;
    Status networkingStatus = Status::kSuccess;

    VerifyOrExit(NetworkMatch(mStagingNetwork, networkId), networkingStatus = Status::kNetworkIDNotFound);
    VerifyOrExit(mpConnectCallback == nullptr, networkingStatus = Status::kUnknownError);
    ChipLogProgress(NetworkProvisioning,
                    "NetworkCommissioningDelegate: SSID: %s", StringOrNullMarker(mStagingNetwork.ssid));
    err = ConnectWiFiNetwork(reinterpret_cast<const char *>(mStagingNetwork.ssid), mStagingNetwork.ssidLen,
            reinterpret_cast<const char *>(mStagingNetwork.credentials), mStagingNetwork.credentialsLen);
    mpConnectCallback = callback;
exit:
    if (err != CHIP_NO_ERROR) {
        networkingStatus = Status::kUnknownError;
    }
    if (networkingStatus != Status::kSuccess) {
        ChipLogError(NetworkProvisioning, "Failed to connect to WiFi network:%s", chip::ErrorStr(err));
        mpConnectCallback = nullptr;
        callback->OnResult(networkingStatus, CharSpan(), 0);
    }
}

CHIP_ERROR GetConnectedNetwork(Network & network)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s\r\n", __func__);
    errcode_t ret = 0;
    wifi_linked_info_stru result;
    ret = wifi_sta_get_ap_info(&result);
    if (ret != 0 || result.ssid[0] == 0) {
        ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s line:%d\r\n", __func__, __LINE__);
        return CHIP_ERROR_INTERNAL;
    }
    uint8_t length = strnlen(reinterpret_cast<const char *>(result.ssid), DeviceLayer::Internal::kMaxWiFiSSIDLength);
    if (length > sizeof(network.networkID)) {
        ChipLogError(DeviceLayer, "HisiWiFiDriver::%s, SSID too long", __func__);
        return CHIP_ERROR_INTERNAL;
    }
    memcpy_s(network.networkID, kMaxNetworkIDLen, result.ssid, length);
    network.networkIDLen = length;
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s\r\n", __func__);
    return CHIP_NO_ERROR;
}

void HisiWiFiDriver::OnNetworkStatusChange()
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::OnNetworkStatusChange\r\n");
    Network configuredNetwork = { 0 };

    VerifyOrReturn(mpStatusChangeCallback != nullptr);
    GetConnectedNetwork(configuredNetwork);

    if (configuredNetwork.networkIDLen) {
        ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s line:%d\r\n", __func__, __LINE__);
        mpStatusChangeCallback->OnNetworkingStatusChange(Status::kSuccess,
                MakeOptional(ByteSpan(configuredNetwork.networkID, configuredNetwork.networkIDLen)), NullOptional);
        return;
    }

    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s line:%d\r\n", __func__, __LINE__);
    mpStatusChangeCallback->OnNetworkingStatusChange(
        Status::kUnknownError, MakeOptional(ByteSpan(configuredNetwork.networkID, configuredNetwork.networkIDLen)),
        MakeOptional(GetLastDisconnectReason()));
}

CHIP_ERROR HisiWiFiDriver::SetLastDisconnectReason(const ChipDeviceEvent * event)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::SetLastDisconnectReason\r\n");
    mLastDisconnectedReason = event->Platform.HisiSystemEvent.Data.WiFiStaDisconnected;
    return CHIP_NO_ERROR;
}

int32_t HisiWiFiDriver::GetLastDisconnectReason()
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::GetLastDisconnectReason\r\n");
    return mLastDisconnectedReason;
}

CHIP_ERROR HisiWiFiDriver::StartScanWiFiNetworks(ByteSpan ssid)
{
    errcode_t ret = 0;
    wifi_scan_params_stru scan_params = {0};

    if (ssid.data() == NULL) {
        ChipLogProgress(NetworkProvisioning, "non-directed scanning...\r\n");
        ret = wifi_sta_scan();
        if (ret != 0) {
            ChipLogError(NetworkProvisioning, "wifi_sta_scan failed.");
        }
    } else {
        ChipLogProgress(NetworkProvisioning, "directed scanning...\r\n");
        ret = strncpy_s((char*)scan_params.ssid, WIFI_MAX_SSID_LEN, (char*)ssid.data(), ssid.size());
        if (ret != 0) {
            ChipLogError(NetworkProvisioning, "HisiWiFiDriver::%s line:%d", __func__, __LINE__);
            return CHIP_ERROR_INTERNAL;
        }
        scan_params.scan_type = WIFI_SSID_SCAN;
        scan_params.ssid_len = ssid.size();
        scan_params.ssid[ssid.size()] = '\0';
        WifiUtilsStaScan(&scan_params);
    }
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::StartScanWiFiNetworks OK\r\n");
    return CHIP_NO_ERROR;
}

void HisiWiFiDriver::OnScanWiFiNetworkDone()
{
    errcode_t ret = 0;
    uint32_t scan_rst_ap_num = 64;
    uint32_t count = 0;
    constexpr int WIFI_SCAN_AP_LIMIT = 64;
    constexpr int DELAY_TIME = 5;
    constexpr int MAX_RETRY_TIMES = 100;
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::OnScanWiFiNetworkDone\r\n");
    if (!GetInstance().mpScanCallback) {
        ChipLogProgress(NetworkProvisioning, "can't find the ScanCallback function\r\n");
        return;
    }
    /* 获取扫描结果 */
    uint32_t scan_len = sizeof(wifi_scan_info_stru) * WIFI_SCAN_AP_LIMIT;
    wifi_scan_info_stru *scan_result = (wifi_scan_info_stru*)osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (scan_result == NULL) {
        return;
    }

    while (1) {
        osDelay(DELAY_TIME);
        ret = wifi_sta_get_scan_info(scan_result, &scan_rst_ap_num);
        if (ret == 0) {
            break;
        }
        count++;
        if (count > MAX_RETRY_TIMES) {
            ChipLogError(NetworkProvisioning, "wifi_sta_get_scan_info timeout, failed.");
            osal_kfree(scan_result);
            return;
        }
    }

    ChipLogProgress(NetworkProvisioning, "AP num = %d\r\n", scan_rst_ap_num);
    HisiScanResponseIterator iter(scan_rst_ap_num, scan_result);
    GetInstance().mpScanCallback->OnFinished(Status::kSuccess, CharSpan(), &iter);
    GetInstance().mpScanCallback = nullptr;
    osal_kfree(scan_result);
}

void HisiWiFiDriver::ScanNetworks(ByteSpan ssid, WiFiDriver::ScanCallback * callback)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s\r\n", __func__);
    if (callback != nullptr) {
        mpScanCallback = callback;
        if (StartScanWiFiNetworks(ssid) != CHIP_NO_ERROR) {
            mpScanCallback = nullptr;
            callback->OnFinished(Status::kUnknownError, CharSpan(), nullptr);
        }
    }
    OnScanWiFiNetworkDone();
}

size_t HisiWiFiDriver::WiFiNetworkIterator::Count()
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::WiFiNetworkIterator::Count\r\n");
    return mDriver->mStagingNetwork.ssidLen == 0 ? 0 : 1;
}

bool HisiWiFiDriver::WiFiNetworkIterator::Next(Network & item)
{
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::WiFiNetworkIterator::Next\r\n");
    if (mExhausted || mDriver->mStagingNetwork.ssidLen == 0) {
        ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s line:%d\r\n", __func__, __LINE__);
        return false;
    }
    ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s line:%d\r\n", __func__, __LINE__);
    memcpy_s(item.networkID, kMaxNetworkIDLen, mDriver->mStagingNetwork.ssid, mDriver->mStagingNetwork.ssidLen);
    item.networkIDLen = mDriver->mStagingNetwork.ssidLen;
    item.connected           = false;
    mExhausted               = true;
    Network connectedNetwork = { 0 };
    CHIP_ERROR err           = GetConnectedNetwork(connectedNetwork);
    if (err == CHIP_NO_ERROR) {
        ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s line:%d\r\n", __func__, __LINE__);
        if (connectedNetwork.networkIDLen == item.networkIDLen &&
            memcmp(connectedNetwork.networkID, item.networkID, item.networkIDLen) == 0) {
            ChipLogProgress(NetworkProvisioning, "HisiWiFiDriver::%s line:%d\r\n", __func__, __LINE__);
            item.connected = true;
        }
    }
    return true;
}

} // namespace NetworkCommissioning
} // namespace DeviceLayer
} // namespace chip
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI
