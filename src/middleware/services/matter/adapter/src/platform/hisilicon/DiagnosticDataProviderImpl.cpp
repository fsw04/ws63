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
 *          Provides an implementation of the DiagnosticDataProvider object
 *          for Hisi platform.
 */
#include <platform/hisilicon/DiagnosticDataProviderImpl.h>
#include <lib/support/CHIPMemString.h>
#include <platform/internal/CHIPDeviceLayerInternal.h>
#include <crypto/CHIPCryptoPAL.h>
#include <platform/DiagnosticDataProvider.h>
#include <lwip/netif.h>
#include "wifi_device.h"
#include "los_memory.h"
#include "securec.h"


namespace chip {
namespace DeviceLayer {

constexpr int SECONDS_PER_HOURS = 3600;

DiagnosticDataProviderImpl & DiagnosticDataProviderImpl::GetDefaultInstance()
{
    static DiagnosticDataProviderImpl sInstance;
    return sInstance;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetCurrentHeapFree(uint64_t & currentHeapFree)
{
    LOS_MEM_POOL_STATUS poolStatus;
    UINT32 ret = LOS_MemInfoGet(m_aucSysMem0, &poolStatus);
    currentHeapFree = static_cast<uint64_t>(poolStatus.uwTotalFreeSize);
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetCurrentHeapUsed(uint64_t & currentHeapUsed)
{
    LOS_MEM_POOL_STATUS poolStatus;
    UINT32 ret = LOS_MemInfoGet(m_aucSysMem0, &poolStatus);
    currentHeapUsed = static_cast<uint64_t>(poolStatus.uwTotalUsedSize);
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetCurrentHeapHighWatermark(uint64_t & currentHeapHighWatermark)
{
    LOS_MEM_POOL_STATUS poolStatus;
    UINT32 ret = LOS_MemInfoGet(m_aucSysMem0, &poolStatus);
    currentHeapHighWatermark = static_cast<uint64_t>(poolStatus.uwUsageWaterLine);
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetRebootCount(uint16_t & rebootCount)
{
    uint32_t count = 0;

    CHIP_ERROR err = ConfigurationMgr().GetRebootCount(count);
    if (err == CHIP_NO_ERROR) {
        VerifyOrReturnError(count <= UINT16_MAX, CHIP_ERROR_INVALID_INTEGER_VALUE);
        rebootCount = static_cast<uint16_t>(count);
    }

    return err;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetUpTime(uint64_t & upTime)
{
    System::Clock::Timestamp currentTime = System::SystemClock().GetMonotonicTimestamp();
    System::Clock::Timestamp startTime   = PlatformMgrImpl().GetStartTime();
    if (currentTime >= startTime) {
        upTime = std::chrono::duration_cast<System::Clock::Seconds64>(currentTime - startTime).count();
        return CHIP_NO_ERROR;
    }

    return CHIP_ERROR_INVALID_TIME;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetTotalOperationalHours(uint32_t & totalOperationalHours)
{
    uint64_t upTime = 0;

    if (GetUpTime(upTime) == CHIP_NO_ERROR) {
        uint32_t totalHours = 0;
        if (ConfigurationMgr().GetTotalOperationalHours(totalHours) == CHIP_NO_ERROR) {
            /* uptime is terms of seconds and dividing it by 3600 to calculate
             * totalOperationalHours in hours.
             */
            VerifyOrReturnError(upTime / SECONDS_PER_HOURS <= UINT32_MAX, CHIP_ERROR_INVALID_INTEGER_VALUE);
            totalOperationalHours = totalHours + static_cast<uint32_t>(upTime / SECONDS_PER_HOURS);
            return CHIP_NO_ERROR;
        }
    }

    return CHIP_ERROR_INVALID_TIME;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetBootReason(BootReasonType & bootReason)
{
    uint32_t reason = 0;

    CHIP_ERROR err = ConfigurationMgr().GetBootReason(reason);
    if (err == CHIP_NO_ERROR) {
        VerifyOrReturnError(reason <= UINT8_MAX, CHIP_ERROR_INVALID_INTEGER_VALUE);
        bootReason = static_cast<BootReasonType>(reason);
    }

    return err;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetNetworkInterfaces(NetworkInterface ** netifpp)
{
    NetworkInterface *ifp = new NetworkInterface();
    struct netif *netif = netif_list;

    if (netif == NULL || ifp == NULL) {
        ChipLogError(DeviceLayer, "Can't get the netif instance");
        *netifpp = NULL;
        return CHIP_ERROR_INTERNAL;
    }

    Platform::CopyString(ifp->Name, netif->name);
    ifp->name = CharSpan::fromCharString(ifp->Name);
    ifp->type = app::Clusters::GeneralDiagnostics::InterfaceTypeEnum::kWiFi;
    ifp->offPremiseServicesReachableIPv4.SetNonNull(false);
    ifp->offPremiseServicesReachableIPv6.SetNonNull(false);
    memcpy_s(ifp->MacAddress, kMaxHardwareAddrSize, netif->hwaddr, sizeof(netif->hwaddr));
    *netifpp = ifp;
    return CHIP_NO_ERROR;
}

void DiagnosticDataProviderImpl::ReleaseNetworkInterfaces(NetworkInterface * netifp)
{
    while (netifp) {
        NetworkInterface *del = netifp;
        netifp                 = netifp->Next;
        delete del;
    }
}

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiBssId(MutableByteSpan & BssId)
{
    errcode_t ret = 0;
    wifi_linked_info_stru result;
    ret = wifi_sta_get_ap_info(&result);
    if (ret != 0 || result.ssid[0] == 0) {
        return CHIP_ERROR_INTERNAL;
    }
    uint8_t length = strnlen(reinterpret_cast<const char *>(result.ssid), DeviceLayer::Internal::kMaxWiFiSSIDLength);
    if (length > BssId.size()) {
        ChipLogError(DeviceLayer, "SSID too long");
        return CHIP_ERROR_INTERNAL;
    }
    memcpy_s(BssId.data(), BssId.size(), result.ssid, length);
    BssId.reduce_size(length);

    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiVersion(
    app::Clusters::WiFiNetworkDiagnostics::WiFiVersionEnum & wifiVersion)
{
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiSecurityType(
    app::Clusters::WiFiNetworkDiagnostics::SecurityTypeEnum & securityType)
{
    using app::Clusters::WiFiNetworkDiagnostics::SecurityTypeEnum;
    errcode_t ret = 0;
    wifi_scan_info_stru result = { 0 };
    uint32_t size = 0;
    ret = wifi_ap_get_scan_info(&result, &size);
    switch (result.security_type) {
        case WIFI_SEC_TYPE_OPEN:
            securityType = SecurityTypeEnum::kNone;
            break;
        case WIFI_SEC_TYPE_WEP:
            securityType = SecurityTypeEnum::kWep;
            break;
        case WIFI_SEC_TYPE_WPA2:
            securityType = SecurityTypeEnum::kWpa2;
            break;
        case WIFI_SEC_TYPE_WPA3:
            securityType = SecurityTypeEnum::kWpa3;
            break;
        default:
            securityType = SecurityTypeEnum::kUnspecified;
            break;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiChannelNumber(uint16_t & channelNumber)
{
    errcode_t ret = 0;
    wifi_linked_info_stru result;
    ret = wifi_sta_get_ap_info(&result);
    if (ret != 0 || result.ssid[0] == 0) {
        return CHIP_ERROR_INTERNAL;
    }
    channelNumber = result.channel_num;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiRssi(int8_t & rssi)
{
    errcode_t ret = 0;
    wifi_linked_info_stru result;
    ret = wifi_sta_get_ap_info(&result);
    if (ret != 0 || result.ssid[0] == 0) {
        return CHIP_ERROR_INTERNAL;
    }
    rssi = result.rssi;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiBeaconLostCount(uint32_t & beaconLostCount)
{
    beaconLostCount = 0;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiCurrentMaxRate(uint64_t & currentMaxRate)
{
    currentMaxRate = 0;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiPacketMulticastRxCount(uint32_t & packetMulticastRxCount)
{
    packetMulticastRxCount = 0;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiPacketMulticastTxCount(uint32_t & packetMulticastTxCount)
{
    packetMulticastTxCount = 0;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiPacketUnicastRxCount(uint32_t & packetUnicastRxCount)
{
    packetUnicastRxCount = 0;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiPacketUnicastTxCount(uint32_t & packetUnicastTxCount)
{
    packetUnicastTxCount = 0;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::GetWiFiOverrunCount(uint64_t & overrunCount)
{
    overrunCount = 0;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DiagnosticDataProviderImpl::ResetWiFiNetworkDiagnosticsCounts()
{
    return CHIP_NO_ERROR;
}

#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

DiagnosticDataProvider & GetDiagnosticDataProviderImpl()
{
    return DiagnosticDataProviderImpl::GetDefaultInstance();
}

} // namespace DeviceLayer
} // namespace chip
