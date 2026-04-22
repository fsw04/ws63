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

#include <platform/ConnectivityManager.h>

#include <platform/internal/GenericConnectivityManagerImpl_UDP.ipp>

#if INET_CONFIG_ENABLE_TCP_ENDPOINT
#include <platform/internal/GenericConnectivityManagerImpl_TCP.ipp>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
#include <platform/internal/GenericConnectivityManagerImpl_BLE.ipp>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/internal/GenericConnectivityManagerImpl_Thread.ipp>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
#include <platform/internal/GenericConnectivityManagerImpl_WiFi.ipp>
#endif

#include <platform/DiagnosticDataProvider.h>
#include <platform/internal/BLEManager.h>
#include <lib/support/CHIPMemString.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

#include <lwip/dns.h>
#include <lwip/ip_addr.h>
#include <lwip/nd6.h>
#include <lwip/netif.h>

#include "NetworkCommissioningDriver.h"
#include "HisiConfig.h"
#include "wifi_device.h"
#include "WifiUtils.h"
#include "securec.h"

using namespace ::chip;
using namespace ::chip::Inet;
using namespace ::chip::DeviceLayer::Internal;

namespace chip {
namespace DeviceLayer {

ConnectivityManagerImpl ConnectivityManagerImpl::sInstance;
NetworkCommissioning::HisiWiFiDriver::WiFiNetwork g_wifiNetconf;

// ==================== ConnectivityManager Platform Internal Methods ====================
CHIP_ERROR ConnectivityManagerImpl::_Init()
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::_Init");
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
    mLastStationConnectFailTime   = System::Clock::kZero;
    mLastAPDemandTime             = System::Clock::kZero;
    mWiFiStationMode              = kWiFiStationMode_Disabled;
    mWiFiStationState             = kWiFiStationState_NotConnected;
    mWiFiAPMode                   = kWiFiAPMode_Disabled;
    mWiFiAPState                  = kWiFiAPState_NotActive;
    mWiFiStationReconnectInterval = System::Clock::Milliseconds32(CHIP_DEVICE_CONFIG_WIFI_STATION_RECONNECT_INTERVAL);
    mWiFiAPIdleTimeout            = System::Clock::Milliseconds32(CHIP_DEVICE_CONFIG_WIFI_AP_IDLE_TIMEOUT);
    mFlags.SetRaw(0);

    WifiUtilsInit();
    wifi_sta_enable();

    // If there is no persistent station provision...
    if (!IsWiFiStationProvisioned()) {
        // If the code has been compiled with a default WiFi station provision, configure that now.
        ReturnErrorOnFailure(SetWiFiStationMode(kWiFiStationMode_Enabled));
    } else {
        // disable WiFi station mode.
        ReturnErrorOnFailure(SetWiFiStationMode(kWiFiStationMode_Enabled));
    }
    // Force AP mode off for now.

    // Queue work items to bootstrap the AP and station state machines once the Chip event loop is running.
    ReturnErrorOnFailure(DeviceLayer::SystemLayer().ScheduleWork(DriveStationState, NULL));

#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::_Init OK");
    return CHIP_NO_ERROR;
}

void ConnectivityManagerImpl::_OnPlatformEvent(const ChipDeviceEvent * event)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s eventType:0x%x\r\n", __func__, event->Type);
    // Forward the event to the generic base classes as needed.
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
    if (event->Type == DeviceEventType::kHisiWiFiStationConnectedEvent) {
        ChipLogProgress(DeviceLayer, "_OnPlatformEvent WIFI_EVENT_STA_CONNECTED");
        if (mWiFiStationState == kWiFiStationState_Connecting) {
            NetworkCommissioning::HisiWiFiDriver::GetInstance().OnConnectWiFiNetwork();
            ChangeWiFiStationState(kWiFiStationState_Connecting_Succeeded);
        }
        DriveStationState();
    }
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI
}

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
ConnectivityManager::WiFiStationMode ConnectivityManagerImpl::_GetWiFiStationMode(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    // Enabled when platform instance initilazed
    return kWiFiStationMode_Enabled;
}

bool ConnectivityManagerImpl::_IsWiFiStationEnabled(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    return GetWiFiStationMode() == kWiFiStationMode_Enabled;
}

CHIP_ERROR ConnectivityManagerImpl::_SetWiFiStationMode(WiFiStationMode val)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s  Mode:%s\r\n", __func__, WiFiStationModeToStr(val));
    CHIP_ERROR err = CHIP_NO_ERROR;
    VerifyOrExit(val != kWiFiStationMode_NotSupported, err = CHIP_ERROR_INVALID_ARGUMENT);

    if (val != kWiFiStationMode_ApplicationControlled) {
        if (val == kWiFiStationMode_Enabled) {
            ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
            wifi_sta_enable();
        }
        ChipLogProgress(DeviceLayer, "WiFi station mode change: %s -> %s", WiFiStationModeToStr(mWiFiStationMode),
                        WiFiStationModeToStr(val));
        mWiFiStationMode = val;
        /* Schedule work for disabled case causes station mode not getting enabled */
        if (mWiFiStationMode != kWiFiStationMode_Disabled) {
            DeviceLayer::SystemLayer().ScheduleWork(DriveStationState, NULL);
        } else {
            /* Call Drive Station directly to disable directly instead of scheduling */
            DriveStationState();
        }
    }

exit:
    return err;
}

bool ConnectivityManagerImpl::_IsWiFiStationProvisioned(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    if (g_wifiNetconf.ssidLen == 0) {
        NetworkCommissioning::HisiWiFiDriver::GetInstance().GetSavedNetWorkConfig(&g_wifiNetconf);
    }

    ChipLogProgress(DeviceLayer, "wifi ssid:%s len:%d\r\n", g_wifiNetconf.ssid, g_wifiNetconf.ssidLen);
    return (g_wifiNetconf.ssidLen != 0) ? true : false;
}

void ConnectivityManagerImpl::_ClearWiFiStationProvision(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    CHIP_ERROR err = CHIP_NO_ERROR;

    err = HisiConfig::ClearConfigValue(HisiConfig::kConfigKey_WiFiSSID);
    SuccessOrExit(err);

    err = HisiConfig::ClearConfigValue(HisiConfig::kConfigKey_WiFiPassword);
    SuccessOrExit(err);
exit:
    return;
}

CHIP_ERROR ConnectivityManagerImpl::_SetWiFiAPMode(WiFiAPMode val)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    CHIP_ERROR err = CHIP_NO_ERROR;

    VerifyOrExit(val != kWiFiAPMode_NotSupported, err = CHIP_ERROR_INVALID_ARGUMENT);
    if (mWiFiAPMode != val) {
        ChipLogProgress(DeviceLayer, "WiFi AP mode change: %s -> %s",
                        WiFiAPModeToStr(mWiFiAPMode), WiFiAPModeToStr(val));
    }
    mWiFiAPMode = val;
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    DeviceLayer::SystemLayer().ScheduleWork(DriveAPState, NULL);

exit:
    return err;
}

void ConnectivityManagerImpl::_DemandStartWiFiAP(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    if (mWiFiAPMode == kWiFiAPMode_OnDemand || mWiFiAPMode == kWiFiAPMode_OnDemand_NoStationProvision) {
        mLastAPDemandTime = System::SystemClock().GetMonotonicTimestamp();
        DeviceLayer::SystemLayer().ScheduleWork(DriveAPState, NULL);
    }
}

void ConnectivityManagerImpl::_StopOnDemandWiFiAP(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    if (mWiFiAPMode == kWiFiAPMode_OnDemand || mWiFiAPMode == kWiFiAPMode_OnDemand_NoStationProvision) {
        mLastAPDemandTime = System::Clock::kZero;
        DeviceLayer::SystemLayer().ScheduleWork(DriveAPState, NULL);
    }
}

void ConnectivityManagerImpl::_MaintainOnDemandWiFiAP(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    if (mWiFiAPMode == kWiFiAPMode_OnDemand || mWiFiAPMode == kWiFiAPMode_OnDemand_NoStationProvision) {
        if (mWiFiAPState == kWiFiAPState_Activating || mWiFiAPState == kWiFiAPState_Active) {
            mLastAPDemandTime = System::SystemClock().GetMonotonicTimestamp();
        }
    }
}

void ConnectivityManagerImpl::_SetWiFiAPIdleTimeout(System::Clock::Timeout val)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    mWiFiAPIdleTimeout = val;
    DeviceLayer::SystemLayer().ScheduleWork(DriveAPState, NULL);
}

CHIP_ERROR ConnectivityManagerImpl::_GetAndLogWifiStatsCounters(void)
{
    return CHIP_NO_ERROR;
}

void ConnectivityManagerImpl::_OnWiFiScanDone()
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    // Schedule a call to DriveStationState method in case a station connect attempt was
    // deferred because the scan was in progress.
    DeviceLayer::SystemLayer().ScheduleWork(DriveStationState, NULL);
}

void ConnectivityManagerImpl::_OnWiFiStationProvisionChange()
{
    // Schedule a call to the DriveStationState method to adjust the station state as needed.
    DeviceLayer::SystemLayer().ScheduleWork(DriveStationState, NULL);
}

// ==================== ConnectivityManager Private Methods ====================
void ConnectivityManagerImpl::WiFiStationConnectedHandler()
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    ChipDeviceEvent event;
    memset_s(&event, sizeof(event), 0, sizeof(event));
    event.Type = DeviceEventType::kHisiWiFiStationConnectedEvent;
    PlatformMgr().PostEventOrDie(&event);
}

void ConnectivityManagerImpl::IsWifiStationConnected(bool *bConnected)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);

    wifi_linked_info_stru wifi_status;
    wifi_sta_get_ap_info(&wifi_status);
    if (wifi_status.conn_state == WIFI_DISCONNECTED) {
        *bConnected = false;
    }
    if (wifi_status.conn_state == WIFI_CONNECTED) {
        if (mWiFiStationState == kWiFiStationState_Connecting) {
            ChangeWiFiStationState(kWiFiStationState_Connecting_Succeeded);
        }
        *bConnected = true;
    }
}

void ConnectivityManagerImpl::DriveStationState()
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    CHIP_ERROR err = CHIP_NO_ERROR;
    bool stationConnected = false;

    // If the station interface is NOT under application control...
    if (mWiFiStationMode != kWiFiStationMode_ApplicationControlled) {
        // Ensure that station mode is enabled in the Hisi WiFi layer.
        wifi_sta_enable();
    }

    IsWifiStationConnected(&stationConnected);
    mWiFiStationMode = GetWiFiStationMode();
    ChipLogProgress(DeviceLayer, "wifi stationConnected:%d, mWiFiStationMode:%d", stationConnected, mWiFiStationMode);
    // If the station interface is currently connected ...
    if (stationConnected) {
        // Advance the station state to Connected if it was previously NotConnected or
        // a previously initiated connect attempt succeeded.
        if (mWiFiStationState == kWiFiStationState_NotConnected ||
            mWiFiStationState == kWiFiStationState_Connecting_Succeeded) {
            ChangeWiFiStationState(kWiFiStationState_Connected);
            ChipLogProgress(DeviceLayer, "WiFi station interface connected");
            mLastStationConnectFailTime = System::Clock::kZero;
            OnStationConnected();
            IpConnectedEventNotify();
        }

        // If the WiFi station interface is no longer enabled, or no longer provisioned,
        // disconnect the station from the AP, unless the WiFi station mode is currently
        // under application control.
        if (mWiFiStationMode != kWiFiStationMode_ApplicationControlled &&
            (mWiFiStationMode != kWiFiStationMode_Enabled)) {
            ChipLogProgress(DeviceLayer, "Disconnecting WiFi station interface");
            errcode_t ret = wifi_sta_disconnect();
            if (ret != 0) {
                ChipLogError(DeviceLayer, "wifi_sta_disconnect() failed: %d", ret);
            }
            SuccessOrExit(ret);

            ChangeWiFiStationState(kWiFiStationState_Disconnecting);
        }
    } else {
        System::Clock::Timestamp now = System::SystemClock().GetMonotonicTimestamp();

        // Advance the station state to NotConnected if it was previously Connected or Disconnecting,
        // or if a previous initiated connect attempt failed.
        if (mWiFiStationState == kWiFiStationState_Connected ||
            mWiFiStationState == kWiFiStationState_Disconnecting ||
            mWiFiStationState == kWiFiStationState_Connecting_Failed) {
            WiFiStationState prevState = mWiFiStationState;
            ChangeWiFiStationState(kWiFiStationState_NotConnected);
            if (prevState != kWiFiStationState_Connecting_Failed) {
                ChipLogProgress(DeviceLayer, "WiFi station interface disconnected");
                mLastStationConnectFailTime = System::Clock::kZero;
                OnStationDisconnected();
            } else {
                ChipLogProgress(NetworkProvisioning, "ConnectivityManagerImpl::%s line:%d\r\n", __func__, __LINE__);
                mLastStationConnectFailTime = now;
            }
        }
        // If the WiFi station interface is now enabled and provisioned (and by implication,
        // not presently under application control), AND the system is not in the process of
        // scanning, then...
        ChipLogProgress(DeviceLayer, "mWiFiStationMode:%d, IsWiFiStationProvisioned:%d",
                        mWiFiStationMode, IsWiFiStationProvisioned());
        if (mWiFiStationMode == kWiFiStationMode_Enabled && IsWiFiStationProvisioned() &&
            mWiFiStationState != kWiFiStationState_Connecting) {
            // Initiate a connection to the AP if we haven't done so before, or if enough
            // time has passed since the last attempt.
            if (mLastStationConnectFailTime == System::Clock::kZero ||
                now >= mLastStationConnectFailTime + mWiFiStationReconnectInterval) {
                ChipLogProgress(DeviceLayer, "try to connect wifi");
                wifi_sta_config_stru sta_config;
                memset_s(&sta_config, sizeof(sta_config), 0x0, sizeof(sta_config));
                memcpy_s(sta_config.ssid, WIFI_MAX_SSID_LEN, g_wifiNetconf.ssid, g_wifiNetconf.ssidLen);
                memcpy_s(sta_config.pre_shared_key, WIFI_MAX_KEY_LEN,
                         g_wifiNetconf.credentials, g_wifiNetconf.credentialsLen);
                err = WifiUtilsConnectNetwork(&sta_config);
                SuccessOrExit(err);
                ChangeWiFiStationState(kWiFiStationState_Connecting);
            } else {
                // Otherwise arrange another connection attempt at a suitable point in the future.
                System::Clock::Timeout timeToNextConnect =
                    (mLastStationConnectFailTime + mWiFiStationReconnectInterval) - now;
                ChipLogProgress(DeviceLayer, "Next WiFi station reconnect in %" PRIu32 " ms",
                                System::Clock::Milliseconds32(timeToNextConnect).count());
                ReturnOnFailure(DeviceLayer::SystemLayer().StartTimer(timeToNextConnect, DriveStationState, nullptr));
            }
        }
    }
exit:
    ChipLogProgress(DeviceLayer, "Done driving station state, nothing else to do...");
    // Kick-off any pending network scan that might have been deferred due to the activity
    // of the WiFi station.
}

void ConnectivityManagerImpl::OnStationConnected()
{
    ChipLogProgress(NetworkProvisioning, "ConnectivityManagerImpl::%s line:%d\r\n", __func__, __LINE__);
    NetworkCommissioning::HisiWiFiDriver::GetInstance().OnConnectWiFiNetwork();

    // Alert other components of the new state.
    ChipDeviceEvent event;
    event.Type                          = DeviceEventType::kWiFiConnectivityChange;
    event.WiFiConnectivityChange.Result = kConnectivity_Established;
    PlatformMgr().PostEventOrDie(&event);
    WiFiDiagnosticsDelegate * delegate = GetDiagnosticDataProvider().GetWiFiDiagnosticsDelegate();

    if (delegate) {
        ChipLogProgress(NetworkProvisioning, "ConnectivityManagerImpl::%s line:%d\r\n", __func__, __LINE__);
        delegate->OnConnectionStatusChanged(
            chip::to_underlying(chip::app::Clusters::WiFiNetworkDiagnostics::ConnectionStatusEnum::kConnected));
    }

    UpdateInternetConnectivityState();
}

void ConnectivityManagerImpl::OnStationDisconnected()
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    // Alert other components of the new state.
    ChipDeviceEvent event;
    event.Type                          = DeviceEventType::kWiFiConnectivityChange;
    event.WiFiConnectivityChange.Result = kConnectivity_Lost;
    PlatformMgr().PostEventOrDie(&event);

    UpdateInternetConnectivityState();
}

void ConnectivityManagerImpl::ChangeWiFiStationState(WiFiStationState newState)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    if (mWiFiStationState != newState) {
        ChipLogProgress(DeviceLayer, "WiFi station state change: %s -> %s", WiFiStationStateToStr(mWiFiStationState),
                        WiFiStationStateToStr(newState));
        mWiFiStationState = newState;
        SystemLayer().ScheduleLambda([]() {
            NetworkCommissioning::HisiWiFiDriver::GetInstance().OnNetworkStatusChange();
        });
    }
}

void ConnectivityManagerImpl::DriveStationState(::chip::System::Layer * aLayer, void * aAppState)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    sInstance.DriveStationState();
}

void ConnectivityManagerImpl::DriveAPState()
{
    ChipLogProgress(DeviceLayer, "WiFi ConnectivityManagerImpl::DriveAPState, do nothing!");
    CHIP_ERROR err = CHIP_NO_ERROR;
}

CHIP_ERROR ConnectivityManagerImpl::ConfigureWiFiAP()
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::ConfigureWiFiAP");
    return CHIP_NO_ERROR;
}

void ConnectivityManagerImpl::ChangeWiFiAPState(WiFiAPState newState)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    if (mWiFiAPState != newState) {
        ChipLogProgress(DeviceLayer, "WiFi AP state change: %s -> %s",
                        WiFiAPStateToStr(mWiFiAPState), WiFiAPStateToStr(newState));
        mWiFiAPState = newState;
    }
}

void ConnectivityManagerImpl::DriveAPState(::chip::System::Layer * aLayer, void * aAppState)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    sInstance.DriveAPState();
}

void ConnectivityManagerImpl::UpdateInternetConnectivityState(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    bool haveIPv4Conn      = false;
    bool haveIPv6Conn      = false;
    const bool hadIPv4Conn = mFlags.Has(ConnectivityFlags::kHaveIPv4InternetConnectivity);
    const bool hadIPv6Conn = mFlags.Has(ConnectivityFlags::kHaveIPv6InternetConnectivity);
    IPAddress addr;

    // If the WiFi station is currently in the connected state...
    if (mWiFiStationState == kWiFiStationState_Connected) {
        // Get the LwIP netif for the WiFi station interface.
        struct netif * netif = netif_list;
        // If the WiFi station interface is up...
        if (netif != NULL && netif_is_up(netif) && netif_is_link_up(netif)) {
            // Check if a DNS server is currently configured.  If so...
            ip_addr_t dnsServerAddr = *dns_getserver(0);
            if (!ip_addr_isany_val(dnsServerAddr)) {
                // If the station interface has been assigned an IPv4 address, and has
                // an IPv4 gateway, then presume that the device has IPv4 Internet connectivity.
                if (!ip4_addr_isany_val(*netif_ip4_addr(netif)) && !ip4_addr_isany_val(*netif_ip4_gw(netif))) {
                    haveIPv4Conn = true;
                    char addrStr[INET_ADDRSTRLEN];
                    ip4addr_ntoa_r(netif_ip4_addr(netif), addrStr, sizeof(addrStr));

                    ChipLogProgress(DeviceLayer, "IPv4 Address Assigned : %s", ip4addr_ntoa(netif_ip4_addr(netif)));

                    IPAddress::FromString(addrStr, addr);
                }

                // Search among the IPv6 addresses assigned to the interface for a Global Unicast
                // address (2000::/3) that is in the valid state.  If such an address is found...
                for (uint8_t i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
                    if (ip6_addr_isglobal(netif_ip6_addr(netif, i)) &&
                        ip6_addr_isvalid(netif_ip6_addr_state(netif, i))) {
                        // Determine if there is a default IPv6 router that is currently reachable
                        // via the station interface.  If so, presume for now that the device has
                        // IPv6 connectivity.
                        struct netif * found_if = nd6_find_route(IP6_ADDR_ANY6);
                        if (found_if && netif->num == found_if->num) {
                            haveIPv6Conn = true;
                            ChipLogProgress(DeviceLayer, "IPv6 Address Assigned : %s", ip6addr_ntoa(netif_ip6_addr(netif,
                            i)));
                        }
                    }
                }
            }
        }
    }

    // If the internet connectivity state has changed...
    if (haveIPv4Conn != hadIPv4Conn || haveIPv6Conn != hadIPv6Conn) {
        ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
        // Update the current state.
        mFlags.Set(ConnectivityFlags::kHaveIPv4InternetConnectivity, haveIPv4Conn)
            .Set(ConnectivityFlags::kHaveIPv6InternetConnectivity, haveIPv6Conn);

        // Alert other components of the state change.
        ChipDeviceEvent event;
        event.Type                            = DeviceEventType::kInternetConnectivityChange;
        event.InternetConnectivityChange.IPv4 = GetConnectivityChange(hadIPv4Conn, haveIPv4Conn);
        event.InternetConnectivityChange.IPv6 = GetConnectivityChange(hadIPv6Conn, haveIPv6Conn);
        PlatformMgr().PostEventOrDie(&event);

        if (haveIPv4Conn != hadIPv4Conn) {
            ChipLogProgress(DeviceLayer, "%s Internet connectivity %s", "IPv4",
                            (haveIPv4Conn) ? "ESTABLISHED" : "LOST");
        }

        if (haveIPv6Conn != hadIPv6Conn) {
            ChipLogProgress(DeviceLayer, "%s Internet connectivity %s", "IPv6",
                            (haveIPv6Conn) ? "ESTABLISHED" : "LOST");
        }
    }
}

#define MATTER_DEBUG 0

void ConnectivityManagerImpl::OnStationIPv4AddressAvailable(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
#if MATTER_DEBUG
    uint8_t * ip  = LwIP_GetIP(&xnetif[0]);
    uint8_t * gw  = LwIP_GetGW(&xnetif[0]);
    uint8_t * msk = LwIP_GetMASK(&xnetif[0]);
#if CHIP_PROGRESS_LOGGING
    {
        ChipLogProgress(DeviceLayer, "\n\r\tIP              => %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        ChipLogProgress(DeviceLayer, "\n\r\tGW              => %d.%d.%d.%d\n\r", gw[0], gw[1], gw[2], gw[3]);
        ChipLogProgress(DeviceLayer, "\n\r\tmsk             => %d.%d.%d.%d\n\r", msk[0], msk[1], msk[2], msk[3]);
    }
#endif // CHIP_PROGRESS_LOGGING
#endif

    UpdateInternetConnectivityState();
    ChipLogProgress(DeviceLayer, "IPv4 address available on WiFi station interface");
    ChipDeviceEvent event;
    event.Type                           = DeviceEventType::kInterfaceIpAddressChanged;
    event.InterfaceIpAddressChanged.Type = InterfaceIpChangeType::kIpV4_Assigned;
    PlatformMgr().PostEventOrDie(&event);
}

void ConnectivityManagerImpl::OnStationIPv4AddressLost(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    ChipLogProgress(DeviceLayer, "IPv4 address lost on WiFi station interface");

    UpdateInternetConnectivityState();

    ChipDeviceEvent event;
    event.Type                           = DeviceEventType::kInterfaceIpAddressChanged;
    event.InterfaceIpAddressChanged.Type = InterfaceIpChangeType::kIpV4_Lost;
    PlatformMgr().PostEventOrDie(&event);
}

void ConnectivityManagerImpl::OnIPv6AddressAvailable(void)
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
#if MATTER_DEBUG
#if LWIP_VERSION_MAJOR >= 2 && LWIP_VERSION_MINOR >= 1
#if LWIP_IPV6
    uint8_t * ipv6_0 = LwIP_GetIPv6_linklocal(&xnetif[0]);
    uint8_t * ipv6_1 = LwIP_GetIPv6_global(&xnetif[0]);
#endif
#endif
#if CHIP_PROGRESS_LOGGING
    {
#if LWIP_VERSION_MAJOR >= 2 && LWIP_VERSION_MINOR >= 1
#if LWIP_IPV6
        ChipLogProgress(DeviceLayer,
                        "\n\r\tlocal IPV6=>%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                        ipv6_0[0], ipv6_0[1], ipv6_0[2], ipv6_0[3], ipv6_0[4], ipv6_0[5], ipv6_0[6], ipv6_0[7],
                        ipv6_0[8], ipv6_0[9], ipv6_0[10], ipv6_0[11], ipv6_0[12], ipv6_0[13], ipv6_0[14], ipv6_0[15]);
        ChipLogProgress(DeviceLayer,
                        "\n\r\tIPV6   => %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                        ipv6_1[0], ipv6_1[1], ipv6_1[2], ipv6_1[3], ipv6_1[4], ipv6_1[5], ipv6_1[6], ipv6_1[7],
                        ipv6_1[8], ipv6_1[9], ipv6_1[10], ipv6_1[11], ipv6_1[12], ipv6_1[13], ipv6_1[14], ipv6_1[15]);
#endif
#endif
    }
#endif // CHIP_PROGRESS_LOGGING
#endif

    UpdateInternetConnectivityState();
    ChipLogProgress(DeviceLayer, "IPv6 address available on WiFi station interface");
    ChipDeviceEvent event;
    event.Type                           = DeviceEventType::kInterfaceIpAddressChanged;
    event.InterfaceIpAddressChanged.Type = InterfaceIpChangeType::kIpV6_Assigned;
    PlatformMgr().PostEventOrDie(&event);
}

void ConnectivityManagerImpl::IpConnectedEventNotify()
{
    ChipLogProgress(DeviceLayer, "ConnectivityManagerImpl::%s\r\n", __func__);
    const bool hadIPv4Conn = mFlags.Has(ConnectivityFlags::kHaveIPv4InternetConnectivity);
    const bool hadIPv6Conn = mFlags.Has(ConnectivityFlags::kHaveIPv6InternetConnectivity);

    if (hadIPv4Conn) {
        ChipLogProgress(NetworkProvisioning, "ConnectivityManagerImpl::%s line:%d\r\n", __func__, __LINE__);
        sInstance.OnStationIPv4AddressAvailable();
    }
    if (hadIPv6Conn) {
        ChipLogProgress(NetworkProvisioning, "ConnectivityManagerImpl::%s line:%d\r\n", __func__, __LINE__);
        sInstance.OnIPv6AddressAvailable();
    }
}

#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

} // namespace DeviceLayer
} // namespace chip
