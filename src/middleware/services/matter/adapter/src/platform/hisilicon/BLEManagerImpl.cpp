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
 *          Provides an implementation of the BLEManager singleton object
 */

/**
 *    @file
 *          Provides an implementation of the BLEManager singleton object
 */

/* this file behaves like a config.h, comes first */
#include <crypto/CHIPCryptoPAL.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <platform/internal/CHIPDeviceLayerInternal.h>
#include <setup_payload/AdditionalDataPayloadGenerator.h>

#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
#include <ble/Ble.h>
#if CHIP_ENABLE_ADDITIONAL_DATA_ADVERTISING
#include <setup_payload/AdditionalDataPayloadGenerator.h>
#endif
#include "stdio.h"
#include <string.h>
#include "securec.h"
#include "BLEGattSvc.h"
/*******************************************************************************
 * Local data types
 *******************************************************************************/

#define CHIP_MAX_ADV_DATA_LEN 31

using namespace ::chip;
using namespace ::chip::Ble;


namespace chip {
namespace DeviceLayer {
namespace Internal {

namespace {

const uint8_t ShortUUID_CHIPoBLEService[] = { 0xF6, 0xFF };

static constexpr System::Clock::Timeout kFastAdvertiseTimeout =
    System::Clock::Milliseconds32(CHIP_DEVICE_CONFIG_BLE_ADVERTISING_INTERVAL_CHANGE_TIME);
System::Clock::Timestamp mAdvertiseStartTime;
} // unnamed namespace


#define log_i(format, ...)     // ChipLogProgress(DeviceLayer, format, ##__VA_ARGS__)

BLEManagerImpl BLEManagerImpl::sInstance;

CHIP_ERROR BLEManagerImpl::_Init()
{
    CHIP_ERROR err;

    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    // Initialize the CHIP BleLayer.
    err = BleLayer::Init(this, this, &DeviceLayer::SystemLayer());
    SuccessOrExit(err);
    mServiceMode = ConnectivityManager::kCHIPoBLEServiceMode_Enabled;
    log_i("%s:%s:%d err=%s \r\n", "BLEManagerImpl", __func__, __LINE__, ErrorStr(err));

    MatterInitCallback();

    {
        MatterBleStackOpen();
        MatterBleAddService();
        BLEMgrImpl().SetStackInit();
        mFlags.Set(Flags::kFlag_StackInitialized, true);
        log_i("ble is alread open!\n");
    }
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);

    if (CHIP_DEVICE_CONFIG_CHIPOBLE_ENABLE_ADVERTISING_AUTOSTART)
    {
        mFlags.Set(Flags::kFlag_AdvertisingEnabled, true);
        log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    }
    else
    {
        mFlags.Set(Flags::kFlag_AdvertisingEnabled, false);
        log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    }
    mFlags.Set(Flags::kFlag_FastAdvertisingEnabled);
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    mNumCons = 0;
    (void)memset_s(mCons, sizeof(mCons), 0, sizeof(mCons));
    (void)memset_s(mDeviceName, sizeof(mDeviceName), 0, sizeof(mDeviceName));

    ChipLogProgress(DeviceLayer, "BLEManagerImpl::Init() complete");
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    if (mFlags.Has(Flags::kFlag_StackInitialized))
    {
        PlatformMgr().ScheduleWork(DriveBLEState, 0);
    }
exit:
    log_i("%s:%s:%d err=%s\r\n", "BLEManagerImpl", __func__, __LINE__, ErrorStr(err));
    return err;
}

bool BLEManagerImpl::_IsAdvertisingEnabled(void)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    return mFlags.Has(Flags::kFlag_AdvertisingEnabled);
}

uint16_t BLEManagerImpl::_NumConnections(void)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    return mNumCons;
}

CHIP_ERROR BLEManagerImpl::_SetAdvertisingEnabled(bool val)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    log_i("%s:%s:%d  val=%d\r\n", "BLEManagerImpl", __func__, __LINE__, val);
    VerifyOrExit(mServiceMode != ConnectivityManager::kCHIPoBLEServiceMode_NotSupported, err = CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    if (val)
    {
        mAdvertiseStartTime = System::SystemClock().GetMonotonicTimestamp();
        ReturnErrorOnFailure(DeviceLayer::SystemLayer().StartTimer(kFastAdvertiseTimeout, HandleFastAdvertisementTimer, this));
    }

    if (mFlags.Has(Flags::kFlag_AdvertisingEnabled) != val)
    {
        mFlags.Set(Flags::kFlag_AdvertisingEnabled, val);
        mFlags.Set(Flags::kFlag_FastAdvertisingEnabled, val);
        PlatformMgr().ScheduleWork(DriveBLEState, 0);
    }
exit:
    return err;
}

CHIP_ERROR BLEManagerImpl::_SetAdvertisingMode(BLEAdvertisingMode mode)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    bool modeChange = false;
    switch (mode)
    {
    case BLEAdvertisingMode::kFastAdvertising:
        if (!mFlags.Has(Flags::kFlag_FastAdvertisingEnabled))
        {
            mFlags.Set(Flags::kFlag_FastAdvertisingEnabled, true);
            modeChange = true;
        }
        break;
    case BLEAdvertisingMode::kSlowAdvertising:
        if (!mFlags.Has(Flags::kFlag_FastAdvertisingEnabled))
        {
            mFlags.Set(Flags::kFlag_FastAdvertisingEnabled, false);
            modeChange = false;
        }
        break;
    default:
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    if (modeChange && mFlags.Has(Flags::kFlag_Advertising))
    {
        mFlags.Set(Flags::kFlag_AdvertisingRestarted);
    }
    PlatformMgr().ScheduleWork(DriveBLEState, 0);
    return CHIP_NO_ERROR;
}

CHIP_ERROR BLEManagerImpl::_GetDeviceName(char * buf, size_t bufSize)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    if (mServiceMode == ConnectivityManager::kCHIPoBLEServiceMode_NotSupported)
    {
        return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
    }
    if (strlen(mDeviceName) >= bufSize)
    {
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    (void)strncpy_s(buf, bufSize, mDeviceName, strlen(mDeviceName));
    ChipLogProgress(DeviceLayer, "Getting device name to : \"%s\"", StringOrNullMarker(mDeviceName));
    return CHIP_NO_ERROR;
}

CHIP_ERROR BLEManagerImpl::_SetDeviceName(const char * deviceName)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    return CHIP_NO_ERROR;
}

void BLEManagerImpl::_OnPlatformEvent(const ChipDeviceEvent * event)
{
    switch (event->Type)
    {
    case DeviceEventType::kCHIPoBLESubscribe: {
        ChipLogProgress(DeviceLayer, "_OnPlatformEvent kCHIPoBLESubscribe");
        HandleSubscribeReceived(event->CHIPoBLESubscribe.ConId, &CHIP_BLE_SVC_ID, &Ble::CHIP_BLE_CHAR_2_UUID);
        {
            ChipDeviceEvent connectionEvent;
            connectionEvent.Type = DeviceEventType::kCHIPoBLEConnectionEstablished;
            PlatformMgr().PostEventOrDie(&connectionEvent);
        }
        break;
    }

    case DeviceEventType::kCHIPoBLEUnsubscribe: {
        ChipLogProgress(DeviceLayer, "_OnPlatformEvent kCHIPoBLEUnsubscribe");
        HandleUnsubscribeReceived(event->CHIPoBLEUnsubscribe.ConId, &CHIP_BLE_SVC_ID, &Ble::CHIP_BLE_CHAR_2_UUID);
        break;
    }

    case DeviceEventType::kCHIPoBLEWriteReceived: {
        ChipLogProgress(DeviceLayer, "_OnPlatformEvent kCHIPoBLEWriteReceived");
        HandleWriteReceived(event->CHIPoBLEWriteReceived.ConId, &CHIP_BLE_SVC_ID, &Ble::CHIP_BLE_CHAR_1_UUID,
                            PacketBufferHandle::Adopt(event->CHIPoBLEWriteReceived.Data));
        break;
    }

    case DeviceEventType::kCHIPoBLENotifyConfirm: {
        ChipLogProgress(DeviceLayer, "_OnPlatformEvent kCHIPoBLENotifyConfirm");
        HandleIndicationConfirmation(event->CHIPoBLENotifyConfirm.ConId, &CHIP_BLE_SVC_ID, &Ble::CHIP_BLE_CHAR_2_UUID);
        break;
    }
    case DeviceEventType::kCHIPoBLEIndicateConfirm: {
        ChipLogProgress(DeviceLayer, "_OnPlatformEvent kCHIPoBLEIndicateConfirm");
        HandleIndicationConfirmation(event->CHIPoBLEIndicateConfirm.ConId, &CHIP_BLE_SVC_ID, &Ble::CHIP_BLE_CHAR_2_UUID);
        break;
    }

    case DeviceEventType::kCHIPoBLEConnectionError: {
        ChipLogProgress(DeviceLayer, "_OnPlatformEvent kCHIPoBLEConnectionError");
        HandleConnectionError(event->CHIPoBLEConnectionError.ConId, event->CHIPoBLEConnectionError.Reason);
        break;
    }

    case DeviceEventType::kServiceProvisioningChange:
    case DeviceEventType::kWiFiConnectivityChange:
        // Force the advertising configuration to be refreshed to reflect new provisioning state.
        ChipLogProgress(DeviceLayer, "Updating advertising data");
        DriveBLEState();
        break;
    default:
        ChipLogProgress(DeviceLayer, "_OnPlatformEvent default:  event->Type = 0x%x", event->Type);
        break;
    }
}

CHIP_ERROR BLEManagerImpl::SubscribeCharacteristic(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId,
                                                   const ChipBleUUID * charId)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    ChipLogProgress(DeviceLayer, "BLEManagerImpl::SubscribeCharacteristic() not supported");
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR BLEManagerImpl::UnsubscribeCharacteristic(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId,
                                                     const ChipBleUUID * charId)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    ChipLogProgress(DeviceLayer, "BLEManagerImpl::UnsubscribeCharacteristic() not supported");
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR BLEManagerImpl::CloseConnection(BLE_CONNECTION_OBJECT conId)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    MatterCloseConnection(conId);
    return CHIP_NO_ERROR;
}

uint16_t BLEManagerImpl::GetMTU(BLE_CONNECTION_OBJECT conId) const
{
    CHIPoBLEConState * p_conn;

    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    /* Check if target connection state exists. */
    p_conn = BLEManagerImpl::sInstance.GetConnectionState(conId);

    if (!p_conn)
    {
        return 0;
    }
    else
    {
        log_i("%s:%s:%d p_conn->Mtu=%d\r\n", "BLEManagerImpl", __func__, __LINE__, p_conn->Mtu);
        return p_conn->Mtu;
    }
}

CHIP_ERROR BLEManagerImpl::SendWriteRequest(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId,
                                            PacketBufferHandle pBuf)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    ChipLogError(DeviceLayer, "BLEManagerImpl::SendWriteRequest() not supported");
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

void BLEManagerImpl::NotifyChipConnectionClosed(BLE_CONNECTION_OBJECT conId)
{
    // Nothing to do
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
}

CHIP_ERROR BLEManagerImpl::SendIndication(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId,
                                          PacketBufferHandle data)
{
    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    CHIP_ERROR err              = CHIP_NO_ERROR;
    CHIPoBLEConState * conState = GetConnectionState(conId);

    VerifyOrExit(conState != NULL, err = CHIP_ERROR_INVALID_ARGUMENT);

    MatterTXCharSendIndication(conId, data->DataLength(), data->Start());

exit:
    return err;
}

/*******************************************************************************
 * Private functions
 *******************************************************************************/
void BLEManagerImpl::DriveBLEState(void)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
    // Perform any initialization actions that must occur after the CHIP task is running.
    if (!mFlags.Has(Flags::kFlag_StackInitialized))
    {
        ChipLogError(DeviceLayer, "stack not initialized");
        return;
    }
    ChipLogProgress(DeviceLayer, "%s:%s:%d kFlag_AdvertisingEnabled=%d kFlag_FastAdvertisingEnabled=%d", "BLEManagerImpl", __func__,
                    __LINE__, mFlags.Has(Flags::kFlag_AdvertisingEnabled), mFlags.Has(Flags::kFlag_FastAdvertisingEnabled));

    // If the application has enabled CHIPoBLE and BLE advertising...
    if (mServiceMode == ConnectivityManager::kCHIPoBLEServiceMode_Enabled &&
        mFlags.Has(Flags::kFlag_AdvertisingEnabled)
#if CHIP_DEVICE_CONFIG_CHIPOBLE_SINGLE_CONNECTION
        // and no connections are active...
        && (mNumCons == 0)
#endif
    )
    {
        log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
        // Start/re-start SoftDevice advertising if not already advertising, or if the
        // advertising state of the SoftDevice needs to be refreshed.
        if (!mFlags.Has(Flags::kFlag_Advertising))
        {
            ChipLogProgress(DeviceLayer, "CHIPoBLE advertising started");
            MatterBleStartAdv(mFlags.Has(Flags::kFlag_FastAdvertisingEnabled));
        }
        else if (mFlags.Has(Flags::kFlag_AdvertisingRestarted) && mFlags.Has(Flags::kFlag_Advertising))
        {
            ChipLogProgress(DeviceLayer, "CHIPoBLE stop advertising to restart");
            MatterBleStopAdv();
        }
    }
    // Otherwise, stop advertising if currently active.
    else
    {
        log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);
        if (mFlags.Has(Flags::kFlag_Advertising))
        {
            ChipLogProgress(DeviceLayer, "CHIPoBLE stop advertising");
            MatterBleStopAdv();
        }
    }

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "Disabling CHIPoBLE service due to error: %s", ErrorStr(err));
        mServiceMode = ConnectivityManager::kCHIPoBLEServiceMode_Disabled;
    }
}

// TODO: param len and data
void BLEManagerImpl::SetAdvertisingData(uint8_t * data, uint8_t * len)
{
    CHIP_ERROR err;
    uint8_t advData[CHIP_MAX_ADV_DATA_LEN];
    uint8_t index = 0;
    uint16_t discriminator = 0;
    chip::Ble::ChipBLEDeviceIdentificationInfo deviceIdInfo;

    log_i("%s:%s:%d\r\n", "BLEManagerImpl", __func__, __LINE__);

    err = ConfigurationMgr().GetBLEDeviceIdentificationInfo(deviceIdInfo);
    if (err != CHIP_NO_ERROR)
    {
        log_i("GetBLEDeviceIdentificationInfo error\r\n");
        return;
    }
    discriminator = deviceIdInfo.GetDeviceDiscriminator();
    if (err != CHIP_NO_ERROR)
    {
        log_i("GetSetupDiscriminator error err=%s\r\n", ErrorStr(err));
        return;
    }

    (void)snprintf_s(mDeviceName, sizeof(mDeviceName), sizeof(mDeviceName), "%s%04u", "HISI-MATTER-", discriminator);
    mDeviceName[32] = 0;
    ChipLogError(DeviceLayer, "%s:%s:%d mDeviceName=%s", "BLEManagerImpl", __func__, __LINE__, mDeviceName);

    (void)memset_s(advData, sizeof(advData), 0, sizeof(advData));
    advData[index++] = 0x0B;                                   // length
    advData[index++] = 0x16; // AD type: (Service Data - 16-bit UUID)
    advData[index++] = ShortUUID_CHIPoBLEService[0];           // AD value
    advData[index++] = ShortUUID_CHIPoBLEService[1];           // AD value

    if (index + sizeof(deviceIdInfo) > CHIP_MAX_ADV_DATA_LEN)
    {
        ChipLogError(DeviceLayer, "SetAdvertisingData advdata extend len=%d", index);
        return;
    }
#if CHIP_ENABLE_ADDITIONAL_DATA_ADVERTISING
    deviceIdInfo.SetAdditionalDataFlag(true);
#endif
    (void)memcpy_s(&advData[index], sizeof(deviceIdInfo), &deviceIdInfo, sizeof(deviceIdInfo));
    index = static_cast<uint8_t>(index + sizeof(deviceIdInfo));

    if (index + strlen(mDeviceName) + 2 > CHIP_MAX_ADV_DATA_LEN)
    {
        ChipLogError(DeviceLayer, "SetAdvertisingData advdata extend len=%d", index);
        return;
    }
    advData[index++] = strlen(mDeviceName) + 1;
    advData[index++] = 0x09;

    (void)memcpy_s(&advData[index], strlen(mDeviceName), mDeviceName, strlen(mDeviceName));
    index = static_cast<uint8_t>(index + strlen(mDeviceName));

    *len = index;
    (void)memcpy_s(data, index, advData, index);
    return;
}

// TODO: param len and data
void BLEManagerImpl::SetScanRspData(uint8_t * data, uint8_t * len)
{
    uint8_t advData[CHIP_MAX_ADV_DATA_LEN];
    uint8_t index = 0;

    advData[index++] = strlen(mDeviceName) + 1;
    advData[index++] = 0x09;
    if (index + strlen(mDeviceName) > CHIP_MAX_ADV_DATA_LEN)
    {
        ChipLogError(DeviceLayer, "advdata extend len=%d", index);
        return;
    }
    (void)memcpy_s(&advData[index], strlen(mDeviceName), mDeviceName, strlen(mDeviceName));
    index = static_cast<uint8_t>(index + strlen(mDeviceName));

    *len = index;
    (void)memcpy_s(data, index, advData, index);
    return;
}

void BLEManagerImpl::SetAdvStartFlag(void)
{
    CHIP_ERROR err;

    mFlags.Set(Flags::kFlag_Advertising, true);
    mFlags.Set(Flags::kFlag_AdvertisingRestarted, false);

    ChipDeviceEvent advChange;
    advChange.Type                             = DeviceEventType::kCHIPoBLEAdvertisingChange;
    advChange.CHIPoBLEAdvertisingChange.Result = kActivity_Started;
    err                                        = PlatformMgr().PostEvent(&advChange);

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "SetAdvStartFlag to error: %s", ErrorStr(err));
    }
}

void BLEManagerImpl::SetAdvEndFlag(void)
{
    CHIP_ERROR err;

    mFlags.Set(Flags::kFlag_Advertising, false);

    ChipDeviceEvent advChange;
    advChange.Type                             = DeviceEventType::kCHIPoBLEAdvertisingChange;
    advChange.CHIPoBLEAdvertisingChange.Result = kActivity_Stopped;
    err                                        = PlatformMgr().PostEvent(&advChange);

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "SetAdvEndFlag to error: %s", ErrorStr(err));
    }
    if (mFlags.Has(Flags::kFlag_AdvertisingRestarted))
    {
        PlatformMgr().ScheduleWork(DriveBLEState, 0);
    }
}

void BLEManagerImpl::SetStackInit(void)
{
    mFlags.Set(Flags::kFlag_StackInitialized, true);
    PlatformMgr().ScheduleWork(DriveBLEState, 0);
}

BLEManagerImpl::CHIPoBLEConState * BLEManagerImpl::AllocConnectionState(uint16_t conId)
{
    log_i("%s:%s:%d conId=%d\r\n", "BLEManagerImpl", __func__, __LINE__, conId);
    for (uint16_t i = 0; i < kMaxConnections; i++)
    {
        if (mCons[i].connected == false)
        {
            mCons[i].ConId     = conId;
            mCons[i].Mtu       = 500;
            mCons[i].connected = true;

            mNumCons++;

            return &mCons[i];
        }
    }
    ChipLogError(DeviceLayer, "Failed to allocate CHIPoBLEConState");
    return NULL;
}

BLEManagerImpl::CHIPoBLEConState * BLEManagerImpl::GetConnectionState(uint16_t conId)
{
    for (uint16_t i = 0; i < kMaxConnections; i++)
    {
        if (mCons[i].ConId == conId)
        {
            return &mCons[i];
        }
    }
    ChipLogError(DeviceLayer, "Failed to find CHIPoBLEConState");
    return NULL;
}

bool BLEManagerImpl::ReleaseConnectionState(uint16_t conId)
{
    log_i("%s:%s:%d conId=%d\r\n", "BLEManagerImpl", __func__, __LINE__, conId);
    for (uint16_t i = 0; i < kMaxConnections; i++)
    {
        if (mCons[i].ConId == conId)
        {
            (void)memset_s(&mCons[i], sizeof(CHIPoBLEConState), 0, sizeof(CHIPoBLEConState));
            mNumCons--;
            return true;
        }
    }
    ChipLogError(DeviceLayer, "Failed to delete CHIPoBLEConState");
    return false;
}

void BLEManagerImpl::SetConnectionMtu(uint16_t conId, uint16_t mtu)
{
    CHIPoBLEConState * pConnection = GetConnectionState(conId);
    if (pConnection)
    {
        pConnection->Mtu = mtu;
        log_i("%s:%s:%d set conId=%d mtu=%d\r\n", "BLEManagerImpl", __func__, __LINE__, conId, mtu);
        return;
    }
    ChipLogError(DeviceLayer, "set mtu failed");
}

void BLEManagerImpl::DriveBLEState(intptr_t arg)
{
    sInstance.DriveBLEState();
}

bool BLEManagerImpl::HandleRXCharWrite(uint8_t connection_id, uint16_t length, uint8_t * value)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    PacketBufferHandle buf;

    log_i("%s:%s:%d Write request received for CHIPoBLE RX characteristic (con %u, len %u)\r\n", "BLEManagerImpl", __func__,
          __LINE__, connection_id, length);
    
    // Copy the data to a packet buffer.
    buf = System::PacketBufferHandle::NewWithData(value, length, 0, 0);
    VerifyOrExit(!buf.IsNull(), err = CHIP_ERROR_NO_MEMORY);

    // Post an event to the Chip queue to deliver the data into the Chip stack.
    ChipDeviceEvent event;
    event.Type                        = DeviceEventType::kCHIPoBLEWriteReceived;
    event.CHIPoBLEWriteReceived.ConId = connection_id;
    event.CHIPoBLEWriteReceived.Data  = std::move(buf).UnsafeRelease();
    err                               = PlatformMgr().PostEvent(&event);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "HandleRXCharWrite() failed: %s", ErrorStr(err));
        return false;
    }
    else
        return true;
}

void BLEManagerImpl::HandleTXCharRead(uint8_t connection_id, uint16_t *length, uint8_t * value)
{
    (void)value;
    log_i("%s:%s:%d read request received for CHIPoBLE TX \r\n", "BLEManagerImpl", __func__, __LINE__);
    *length = 0;
}

void BLEManagerImpl::HandleTXCharCCCDRead(uint8_t connection_id, uint16_t * length, uint8_t * value)
{
    CHIPoBLEConState * conState = NULL;
    log_i("%s:%s:%d read request received for CHIPoBLE TX CCCD\r\n", "BLEManagerImpl", __func__, __LINE__);
    // Find the connection state record.
    conState = GetConnectionState(connection_id);
    if (NULL != conState)
    {
        *length  = 2;
        value[0] = conState->Subscribed ? 1 : 0;
        log_i("%s:%s:%d read request received for CHIPoBLE TX CCCD conState->Subscribed=%d\r\n", "BLEManagerImpl", __func__,
              __LINE__, conState->Subscribed);
    }
}

bool BLEManagerImpl::HandleTXCharCCCDWrite(uint8_t connection_id, uint16_t length, uint8_t * value)
{
    CHIP_ERROR err          = CHIP_NO_ERROR;
    bool indicationsEnabled = false;
    CHIPoBLEConState * conState;

    conState = GetConnectionState(connection_id);
    VerifyOrExit(conState != NULL, err = CHIP_ERROR_NO_MEMORY);
    log_i("%s:%s:%d length=%d value[0]=%d value[1]=%d\r\n", "BLEManagerImpl", __func__, __LINE__, length, value[0], value[1]);
    // Determine if the client is enabling or disabling indications.
    indicationsEnabled = ((length > 0) && (value[0] != 0));

    // Post an event to the Chip queue to process either a CHIPoBLE Subscribe or Unsubscribe based on
    // whether the client is enabling or disabling indications.

    ChipDeviceEvent event;
    event.Type = (indicationsEnabled) ? DeviceEventType::kCHIPoBLESubscribe : DeviceEventType::kCHIPoBLEUnsubscribe;
    event.CHIPoBLESubscribe.ConId = connection_id;
    err                           = PlatformMgr().PostEvent(&event);

    conState->Subscribed = indicationsEnabled;
    log_i("\r\nCHIPoBLE %s received\r\n", indicationsEnabled ? "subscribe" : "unsubscribe");

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "HandleTXCharCCCDWrite() failed: %s", ErrorStr(err));
        return false;
    }
    else
        return true;
}

#if CHIP_ENABLE_ADDITIONAL_DATA_ADVERTISING
void BLEManagerImpl::HandleC3CharRead(uint8_t connection_id, uint16_t * p_len, uint8_t * p_value)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    CHIPoBLEConState * conState;
    PacketBufferHandle bufferHandle;
    BitFlags<AdditionalDataFields> additionalDataFields;
    AdditionalDataPayloadGeneratorParams additionalDataPayloadParams;

    conState = GetConnectionState(connection_id);
    if (conState == NULL)
    {
        log_i("%s:%d,exit", __func__, __LINE__);
        return;
    }
#if CHIP_ENABLE_ROTATING_DEVICE_ID && defined(CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID)
    uint8_t rotatingDeviceIdUniqueId[ConfigurationManager::kRotatingDeviceIDUniqueIDLength] = {};
    MutableByteSpan rotatingDeviceIdUniqueIdSpan(rotatingDeviceIdUniqueId);
    err = DeviceLayer::GetDeviceInstanceInfoProvider()->GetRotatingDeviceIdUniqueId(rotatingDeviceIdUniqueIdSpan);
    SuccessOrExit(err);
    err = ConfigurationMgr().GetLifetimeCounter(additionalDataPayloadParams.rotatingDeviceIdLifetimeCounter);
    SuccessOrExit(err);
    additionalDataPayloadParams.rotatingDeviceIdUniqueId = rotatingDeviceIdUniqueIdSpan;
    additionalDataFields.Set(AdditionalDataFields::RotatingDeviceId);
#endif /* CHIP_ENABLE_ROTATING_DEVICE_ID && defined(CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID) */
    err = AdditionalDataPayloadGenerator().generateAdditionalDataPayload(additionalDataPayloadParams, bufferHandle,
                                                                         additionalDataFields);
    SuccessOrExit(err);
    *p_len = bufferHandle->DataLength();
    (void)memcpy_s(p_value, *p_len, bufferHandle->Start(), *p_len);

    log_i("%s:%s:%d length=%d\r\n", "BLEManagerImpl", __func__, __LINE__, *p_len);
exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "Failed to generate TLV encoded Additional Data (%s)", __func__);
    }
    return;
}
#endif /* CHIP_ENABLE_ADDITIONAL_DATA_ADVERTISING */

void BLEManagerImpl::SendIndicationConfirm(uint16_t conId)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    ChipDeviceEvent event;
    CHIPoBLEConState * pConnection = GetConnectionState(conId);
    if (pConnection)
    {
        event.Type                        = DeviceEventType::kCHIPoBLENotifyConfirm;
        event.CHIPoBLENotifyConfirm.ConId = conId;
        err                               = PlatformMgr().PostEvent(&event);
        if (err != CHIP_NO_ERROR)
        {
            log_i("notify confirm failed\r\n");
        }
    }
}

void BLEManagerImpl::HandleFastAdvertisementTimer()
{
    System::Clock::Timestamp currentTimestamp = System::SystemClock().GetMonotonicTimestamp();

    if (currentTimestamp - mAdvertiseStartTime >= kFastAdvertiseTimeout)
    {
        mFlags.Set(Flags::kFlag_FastAdvertisingEnabled, 0);
        mFlags.Set(Flags::kFlag_AdvertisingRestarted, 1);
        PlatformMgr().ScheduleWork(DriveBLEState, 0);
    }
}

void BLEManagerImpl::HandleFastAdvertisementTimer(System::Layer * systemLayer, void * context)
{
    static_cast<BLEManagerImpl *>(context)->HandleFastAdvertisementTimer();
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
#endif // CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
