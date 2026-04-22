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

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdio.h>
#include "bts_def.h"
#include "securec.h"
#include "errcode.h"
#include "bts_def.h"
#include "bts_le_gap.h"
#include "bts_gatt_stru.h"
#include "bts_gatt_server.h"
#include "BLEGattSvc.h"

#include <platform/ConfigurationManager.h>
#include <platform/internal/BLEManager.h>
using namespace chip::DeviceLayer;
using namespace chip::DeviceLayer::Internal;

/**
 * @if Eng
 * @brief Definitaion value range for adv addr type.
 * @else
 * @brief Ble 广播地址类型。
 * @endif
 */
typedef enum {
    BLE_PUBLIC_DEVICE_ADDRESS =                             0x00,
    BLE_RANDOM_DEVICE_ADDRESS =                             0x01,
    BLE_PUBLIC_IDENTITY_ADDRESS =                           0x02,
    BLE_RANDOM_STATIC_IDENTITY_ADDRESS =                    0x03
} BleAddressType;

/**
 * @if Eng
 * @brief Definitaion value range for adv channel map.
 * @else
 * @brief Ble 广播信道范围。
 * @endif
 */
typedef enum BleAdvChannelMap {
    BLE_ADV_CHANNEL_MAP_CH_37 =                      0x01,
    BLE_ADV_CHANNEL_MAP_CH_38 =                      0x02,
    BLE_ADV_CHANNEL_MAP_CH_39 =                      0x04,
    BLE_ADV_CHANNEL_MAP_CH_37_CH_38 =                0x03,
    BLE_ADV_CHANNEL_MAP_CH_37_CH_39 =                0x05,
    BLE_ADV_CHANNEL_MAP_CH_38_CH_39 =                0x06,
    BLE_ADV_CHANNEL_MAP_CH_DEFAULT =                 0x07
} BleAdvChannelMapType;

/**
 * @if Eng
 * @brief Definitaion value range for adv type.
 * @else
 * @brief Ble adv 类型范围。
 * @endif
 */
typedef enum BleAdverting {
    BLE_ADV_TYPE_CONNECTABLE_UNDIRECTED =                            0x00,
    BLE_ADV_TYPE_CONNECTABLE_HIGH_DUTY_CYCLE_DIRECTED =              0x01,
    BLE_ADV_TYPE_SCANNABLE_UNDIRECTED =                              0x02,
    BLE_ADV_TYPE_NON_CONNECTABLE_UNDIRECTED =                        0x03,
    BLE_ADV_TYPE_CONNECTABLE_LOW_DUTY_CYCLE_DIRECTED =               0x04
} BleAdvertingType;

/**
 * @if Eng
 * @brief Definitaion value range for typedef struct ble_adv_para.adv_filter_policy.
 * @else
 * @brief Ble adv filter policy定义值范围。
 * @endif
 */
typedef enum BleAdvFilterPolicy {
    BLE_ADV_FILTER_POLICY_SCAN_ANY_CONNECT_ANY =                     0x00,
    BLE_ADV_FILTER_POLICY_SCAN_WHITE_LIST_CONNECT_ANY =              0x01,
    BLE_ADV_FILTER_POLICY_SCAN_ANY_CONNECT_WHITE_LIST =              0x02,
    BLE_ADV_FILTER_POLICY_SCAN_WHITE_LIST_CONNECT_WHITE_LIST =       0x03
} BleAdvFilterPolicyType;

typedef enum MatterSvcIdx {
    MATTER_CHARAC_IDX_RX = 0,   // C1, Write Character
    MATTER_CHARAC_IDX_TX,       // C2, Indicate Character
    MATTER_CHARAC_IDX_C3,       // C3, Read Character
    MATTER_DESC_IDX_CCCD,       // CCCD
    MATTER_IDX_INVALID = 0xFF   // invalid index
} MatterSvcIdxType;

typedef struct _CharacHandleMap {
    MatterSvcIdxType index;
    uint16_t handle;
} CharacHandleMap;

/*
 * MACRO DEFINES
 ****************************************************************************************
 */
#define SVC_UUID_LEN 16
#define CHARAC_UUID_LEN 16
#define CCCD_UUID_LEN 2
#define MATTER_CHARAC_CNT_MAX 4
#define MAX_READ_REQ_LEN 512
#define WAIT_TIME_MS 20
#define MAX_TRY_COUNT 100

#define MAX_ADV_DATA_LEN 251
/* Ble adv min interval */
#define BLE_ADV_MIN_INTERVAL 0x30
/* Ble adv max interval */
#define BLE_ADV_MAX_INTERVAL 0x60
/* Ble adv handle */
#define BTH_GAP_BLE_ADV_HANDLE_DEFAULT 0x01
/* Ble adv duration */
#define BTH_GAP_BLE_ADV_FOREVER_DURATION 0

#define UUID_CHIPoBLECharact_RX                                                                             \
    {                                                                                                       \
        0x11, 0x9D, 0x9F, 0x42, 0x9C, 0x4F, 0x9F, 0x95, 0x59, 0x45, 0x3D, 0x26, 0xF5, 0x2E, 0xEE, 0x18      \
    }

#define ChipUUID_CHIPoBLECharact_TX                                                                         \
    {                                                                                                       \
        0x12, 0x9D, 0x9F, 0x42, 0x9C, 0x4F, 0x9F, 0x95, 0x59, 0x45, 0x3D, 0x26, 0xF5, 0x2E, 0xEE, 0x18      \
    }


#define UUID_CHIPoBLEChar_C3                                                                                 \
    {                                                                                                        \
        0x04, 0x8F, 0x21, 0x83, 0x8A, 0x74, 0x7D, 0xB8, 0xF2, 0x45, 0x72, 0x87, 0x38, 0x02, 0x63, 0x64       \
    }

#define ATT_DECL_PRIMARY_SERVICE                                \
    {                                                           \
        0x00, 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0    \
    }
#define ATT_DECL_CHARACTERISTIC                                 \
    {                                                            \
        0x03, 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0     \
    }
#define ATT_DESC_CLIENT_CHAR_CFG                                 \
    {                                                            \
        0x29, 0x02                                               \
    }


#define log_i(format, ...) // ChipLogProgress(DeviceLayer, format, ##__VA_ARGS__)

/* ble connect handle */
static uint16_t g_conn_hdl = 0;
static uint8_t g_server_id = 0;
static uint8_t g_srvcHandle = 0;

static bd_addr_t g_ble_addr = { 0 };
/* C2 indicate handle */
static uint16_t g_C2IndicateHandle = 0;
static uint8_t g_charaValue[] = {0x0, 0x0};
static uint8_t cccDataVal[] = {0x0, 0x0};

static const uint8_t g_svcUuid[] = { 0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                                     0x00, 0x10, 0x00, 0x00, 0xF6, 0xFF, 0x00, 0x00 };
static uint8_t g_syncFlag = 0;
static uint8_t g_ServiceStartFlag = 0;
static CharacHandleMap g_characHdlTable[MATTER_CHARAC_CNT_MAX];

static errcode_t CompareServiceUuid(bt_uuid_t *uuid1, bt_uuid_t *uuid2)
{
    if (uuid1->uuid_len != uuid2->uuid_len) {
        return ERRCODE_BT_FAIL;
    }
    if (memcmp(uuid1->uuid, uuid2->uuid, uuid1->uuid_len) != 0) {
        return ERRCODE_BT_FAIL;
    }
    return ERRCODE_BT_SUCCESS;
}

static void WaitSyncFlag(void)
{
    uint32_t count = 0;
    while ((g_syncFlag != 1) && (count < MAX_TRY_COUNT))  {
        osDelay(WAIT_TIME_MS);
        count++;
    }

    if (count >= MAX_TRY_COUNT) {
        log_i("wait sync flag overtime!\n");
    }
    g_syncFlag = 0;
}

static void WaitServiceStart(void)
{
    uint32_t maxCount = MAX_TRY_COUNT;
    uint32_t count = 0;
    while ((g_ServiceStartFlag != 1) && (count < maxCount))  {
        osDelay(WAIT_TIME_MS);
        count++;
    }

    if (count >= maxCount) {
        log_i("wait sync flag overtime!\n");
    }
}

static void BleAddDescriptorCCC(uint32_t server_id, uint32_t srvc_handle)
{
    uint8_t uuid[CCCD_UUID_LEN] = ATT_DESC_CLIENT_CHAR_CFG;
    gatts_add_desc_info_t descriptor = { 0 };

    (void)memcpy_s(descriptor.desc_uuid.uuid, BT_UUID_MAX_LEN, uuid, CCCD_UUID_LEN);
    descriptor.desc_uuid.uuid_len = CCCD_UUID_LEN; // use short uuid for CCCD
    descriptor.permissions = GATT_ATTRIBUTE_PERMISSION_READ | GATT_ATTRIBUTE_PERMISSION_WRITE;
    descriptor.value_len = sizeof(cccDataVal);
    descriptor.value = cccDataVal;
    gatts_add_descriptor(server_id, srvc_handle, &descriptor);

    WaitSyncFlag();
}

static void AddMatterCharacByInx(MatterSvcIdxType idx, uint32_t server_id, uint32_t srvc_handle)
{
    gatts_add_chara_info_t character;
    uint8_t permissions = 0;
    uint8_t properties = 0;
    uint8_t uuidCharaRX[CHARAC_UUID_LEN] = UUID_CHIPoBLECharact_RX;
    uint8_t uuidCharaTX[CHARAC_UUID_LEN] = ChipUUID_CHIPoBLECharact_TX;
    uint8_t uuidCharaC3[CHARAC_UUID_LEN] = UUID_CHIPoBLEChar_C3;

    (void)memset_s(&character, sizeof(gatts_add_chara_info_t), 0, sizeof(gatts_add_chara_info_t));

    if (idx == MATTER_CHARAC_IDX_RX) {
        (void)memcpy_s(character.chara_uuid.uuid, BT_UUID_MAX_LEN, uuidCharaRX, CHARAC_UUID_LEN);
        properties = GATT_CHARACTER_PROPERTY_BIT_WRITE;
        permissions = GATT_ATTRIBUTE_PERMISSION_WRITE | GATT_ATTRIBUTE_PERMISSION_READ;
    } else if (idx == MATTER_CHARAC_IDX_TX) {
        (void)memcpy_s(character.chara_uuid.uuid, BT_UUID_MAX_LEN, uuidCharaTX, CHARAC_UUID_LEN);
        properties = GATT_CHARACTER_PROPERTY_BIT_INDICATE | GATT_CHARACTER_PROPERTY_BIT_WRITE_NO_RSP | GATT_CHARACTER_PROPERTY_BIT_READ;
        permissions = GATT_ATTRIBUTE_PERMISSION_WRITE | GATT_ATTRIBUTE_PERMISSION_READ;
    } else if (idx == MATTER_CHARAC_IDX_C3) {
        (void)memcpy_s(character.chara_uuid.uuid, BT_UUID_MAX_LEN, uuidCharaC3, CHARAC_UUID_LEN);
        properties = GATT_CHARACTER_PROPERTY_BIT_READ;
        permissions = GATT_ATTRIBUTE_PERMISSION_READ;
    }

    character.chara_uuid.uuid_len = CHARAC_UUID_LEN;
    character.value_len = sizeof(g_charaValue);
    character.value = g_charaValue;
    character.properties = properties;
    character.permissions = permissions;
    gatts_add_characteristic(server_id, srvc_handle, &character);

    WaitSyncFlag();
}

static void BleAddCharactersAndDescriptors(uint32_t server_id, uint32_t srvc_handle)
{
    log_i("%s: beginning add characteristic\r\n", __func__);
    AddMatterCharacByInx(MATTER_CHARAC_IDX_RX, server_id, srvc_handle);  // Add C1, Write
    AddMatterCharacByInx(MATTER_CHARAC_IDX_TX, server_id, srvc_handle);  // Add C2, Indicate
    BleAddDescriptorCCC(server_id, srvc_handle);                         // Add CCCD for C2
    AddMatterCharacByInx(MATTER_CHARAC_IDX_C3, server_id, srvc_handle);  // Add C3, Read
}

static void BleAddServiceCb(uint8_t server_id, bt_uuid_t *uuid, uint16_t handle, errcode_t status)
{
    bt_uuid_t service_uuid = {0};
    log_i("%s: server: %d, status: %d, srv_handle: %d, uuid_len: %d,uuid:", __func__,
          server_id, status, handle, uuid->uuid_len);
    for (int8_t i = 0; i < uuid->uuid_len ; i++) {
        log_i("%02x", (uint8_t)uuid->uuid[i]);
    }
    log_i("\n");

    (void)memcpy_s(service_uuid.uuid, BT_UUID_MAX_LEN, g_svcUuid, SVC_UUID_LEN);
    service_uuid.uuid_len = SVC_UUID_LEN;
    if (CompareServiceUuid(uuid, &service_uuid) == ERRCODE_BT_SUCCESS) {
        g_srvcHandle = handle;
    } else {
        log_i("%s:unknown service uuid\r\n", __func__);
        return;
    }
}

static void RecordCharacDescHandle(bt_uuid_t *uuid, uint16_t handle)
{
    static uint8_t idx = 0;
    bt_uuid_t uuidCharaRX = {CHARAC_UUID_LEN, UUID_CHIPoBLECharact_RX};
    bt_uuid_t uuidCharaTX = {CHARAC_UUID_LEN, ChipUUID_CHIPoBLECharact_TX};
    bt_uuid_t uuidCharaC3 = {CHARAC_UUID_LEN, UUID_CHIPoBLEChar_C3};
    bt_uuid_t uuidCCCD = { CCCD_UUID_LEN, ATT_DESC_CLIENT_CHAR_CFG};

    if (idx > MATTER_CHARAC_CNT_MAX) {
        log_i("Warning, character count over the max cnt \n");
        return;
    }
    
    if (CompareServiceUuid(uuid, &uuidCharaRX) == ERRCODE_BT_SUCCESS) {
        g_characHdlTable[idx].index = MATTER_CHARAC_IDX_RX;
        g_characHdlTable[idx].handle = handle;
        idx++;
    } else if (CompareServiceUuid(uuid, &uuidCharaTX) == ERRCODE_BT_SUCCESS) {
        g_characHdlTable[idx].index = MATTER_CHARAC_IDX_TX;
        g_characHdlTable[idx].handle = handle;

        // indicate handle
        g_C2IndicateHandle = handle;
        log_i("g_C2IndicateHandle handle:%d \n", g_C2IndicateHandle);

        idx++;
    } else if (CompareServiceUuid(uuid, &uuidCharaC3) == ERRCODE_BT_SUCCESS) {
        g_characHdlTable[idx].index = MATTER_CHARAC_IDX_C3;
        g_characHdlTable[idx].handle = handle;
        idx++;
    } else if (CompareServiceUuid(uuid, &uuidCCCD) == ERRCODE_BT_SUCCESS) {
        g_characHdlTable[idx].index = MATTER_DESC_IDX_CCCD;
        g_characHdlTable[idx].handle = handle;
        idx++;
    } else {
        log_i("uuid not match \n");
    }
}

static void  BleAddCharacterCb(uint8_t server_id, bt_uuid_t *uuid, uint16_t service_handle,
    gatts_add_character_result_t *result, errcode_t status)
{
    int8_t i = 0;
    log_i("%s: server: %d, status: %d, srv_hdl: %d "\
          "char_hdl: 0x%x, char_val_hdl: 0x%x, uuid_len: %d, uuid: ", __func__,
          server_id, status, service_handle, result->handle, result->value_handle, uuid->uuid_len);
    for (i = 0; i < uuid->uuid_len ; i++) {
        log_i("%02x", (uint8_t)uuid->uuid[i]);
    }
    log_i("\n");

    RecordCharacDescHandle(uuid, result->value_handle);
    g_syncFlag = 1;
}

static void  BleAddDescriptorCb(uint8_t server_id, bt_uuid_t *uuid, uint16_t service_handle,
    uint16_t handle, errcode_t status)
{
    int8_t i = 0;
    log_i("[uuid server] add descriptor cbk : server: %d, status: %d, srv_hdl: %d, desc_hdl: 0x%x ,"\
          "uuid_len: %d, uuid: ", server_id, status, service_handle, handle, uuid->uuid_len);
    for (i = 0; i < uuid->uuid_len ; i++) {
        log_i("%02x", (uint8_t)uuid->uuid[i]);
    }
    log_i("\n");
    RecordCharacDescHandle(uuid, handle);
    g_syncFlag = 1;
}

static void BleStartServiceCb(uint8_t server_id, uint16_t handle, errcode_t status)
{
    log_i("%s:start service cbk : server: %d status: %d srv_hdl: %d\n", __func__,
          server_id, status, handle);
    if (handle == g_srvcHandle) {
        g_ServiceStartFlag = 1;
        log_i("start matter service OK! \n");
    }
}

static void ProcessWriteDatabyHandle(uint16_t handle, uint8_t connId, uint16_t length, uint8_t * value)
{
    uint8_t index = MATTER_IDX_INVALID;
    for (int i = 0; i <= MATTER_CHARAC_CNT_MAX; i++) {
        if (handle == g_characHdlTable[i].handle) {
            index = g_characHdlTable[i].index;
            break;
        }
    }

    switch (index) {
        case MATTER_CHARAC_IDX_RX:
            BLEMgrImpl().HandleRXCharWrite(connId, length, value);
            break;
        case MATTER_DESC_IDX_CCCD:
        case MATTER_CHARAC_IDX_TX:
            BLEMgrImpl().HandleTXCharCCCDWrite(connId, length, value);
            break;
        case MATTER_CHARAC_IDX_C3:
            break;
        default:
            log_i("not matter svr index ,ignore\n");
            break;
    }
}

static void ProcessReadDatabyHandle(uint8_t serverId, uint16_t handle, uint8_t connId, uint16_t requestId, uint16_t offset)
{
    uint8_t index = MATTER_IDX_INVALID;
    uint8_t *buff = NULL;
    uint8_t tmp[MAX_READ_REQ_LEN] = {0};
    gatts_send_rsp_t param = { 0 };
    uint16_t length = MAX_READ_REQ_LEN;
    errcode_t ret = 0;
    
    // find character index
    for (int i = 0; i <= MATTER_CHARAC_CNT_MAX; i++) {
        if (handle == g_characHdlTable[i].handle) {
            index = g_characHdlTable[i].index;
            break;
        }
    }

    // get read data
    switch (index) {
        case MATTER_CHARAC_IDX_RX:
            break;
        case MATTER_DESC_IDX_CCCD:
            BLEMgrImpl().HandleTXCharCCCDRead(connId, &length, tmp);
            break;
        case MATTER_CHARAC_IDX_TX:
            BLEMgrImpl().HandleTXCharRead(connId, &length, tmp);
            break;
        case MATTER_CHARAC_IDX_C3:
            BLEMgrImpl().HandleC3CharRead(connId, &length, tmp);
            break;
        default:
            log_i("not matter svr index ,ignore\n");
            return;
    }

    // send read data
    if (length > offset) {
        length = length - offset;
        buff   = tmp + offset;
    } else {
        length = 0;
    }

    param.status = 0;
    param.offset = 0;
    param.value_len = length;
    param.value = buff;
    param.request_id = requestId;
    ret = gatts_send_response(serverId, connId, &param);
    if (ret != ERRCODE_BT_SUCCESS) {
        log_i("gatts_send_response fail %d.\n", ret);
    }
}

static void BleWriteRequestCb(uint8_t server_id, uint16_t conn_id,
    gatts_req_write_cb_t *write_cb_para, errcode_t status)
{
    log_i("%s: server_id:%d conn_id:%d\n", __func__, server_id, conn_id);
    log_i("request_id:%d att_handle:%d offset:%d need_rsp:%d need_authorize:%d is_prep:%d\n",
          write_cb_para->request_id, write_cb_para->handle, write_cb_para->offset, write_cb_para->need_rsp,
          write_cb_para->need_authorize, write_cb_para->is_prep);
    log_i("data_len:%d data:\n", write_cb_para->length);
    for (uint8_t i = 0; i < write_cb_para->length; i++) {
        log_i("%02x ", write_cb_para->value[i]);
    }
    log_i("\n");
    log_i("status:%d\n", status);

    ProcessWriteDatabyHandle(write_cb_para->handle,
                            conn_id,
                            write_cb_para->length,
                            write_cb_para->value);
}

static void BleReadRequestCb(uint8_t server_id, uint16_t conn_id,
    gatts_req_read_cb_t *read_cb_para, errcode_t status)
{
    log_i("%s: server_id:%d conn_id:%d\n", __func__, server_id, conn_id);
    log_i("request_id:%d att_handle:%d offset:%d need_rsp:%d need_authorize:%d is_long:%d\n",
          read_cb_para->request_id, read_cb_para->handle, read_cb_para->offset, read_cb_para->need_rsp,
          read_cb_para->need_authorize, read_cb_para->is_long);
    log_i("status:%d\n", status);

    ProcessReadDatabyHandle(server_id,
                            read_cb_para->handle,
                            conn_id,
                            read_cb_para->request_id,
                            read_cb_para->offset);
}

static void BleStartAdvCb(uint8_t adv_id, adv_status_t status)
{
    log_i("%s: %d, status:%d\n", __func__, adv_id, status);
    BLEMgrImpl().SetAdvStartFlag();
}

static void BleStopAdvCb(uint8_t adv_id, adv_status_t status)
{
    log_i("%s: %d, status:%d\n", __func__, adv_id, status);
    BLEMgrImpl().SetAdvEndFlag();
}

static void BleConnStateChangeCb(uint16_t conn_id, bd_addr_t *addr, gap_ble_conn_state_t conn_state,
    gap_ble_pair_state_t pair_state, gap_ble_disc_reason_t disc_reason)
{
    log_i("->connect state change conn_id: %d, status: %d, pair_status:%d, disc_reason 0x%x\n",
           conn_id, conn_state, pair_state, disc_reason);
    log_i("addr: ");
    for (uint8_t i = 0; i < BD_ADDR_LEN; i++) {
        log_i("0x%2x ", addr->addr[i]);
    }
    log_i("\n");

    if (conn_state == GAP_BLE_STATE_CONNECTED) {
        ChipLogProgress(DeviceLayer, "matter_set_connection_id conId=%d", conn_id);
        BLEMgrImpl().AllocConnectionState(conn_id);
        g_conn_hdl = conn_id;
        (void)memcpy_s(g_ble_addr.addr, BD_ADDR_LEN, addr->addr, BD_ADDR_LEN);
        g_ble_addr.type = addr->type;
    }

    if (conn_state == GAP_BLE_STATE_DISCONNECTED) {
        BLEMgrImpl().ReleaseConnectionState(conn_id);
        gap_ble_disconnect_remote_device(&g_ble_addr);
        g_conn_hdl = 0;
        PlatformMgr().ScheduleWork(BLEMgrImpl().DriveBLEState, 0);
    }
}

static void BleMtuChangedCb(uint8_t server_id, uint16_t conn_id, uint16_t mtu_size, errcode_t status)
{
    log_i("%s: %d, conn_id: %d, mtu_size: %d, status:%d \n", __func__,
          server_id, conn_id, mtu_size, status);
    BLEMgrImpl().SetConnectionMtu(conn_id, mtu_size);
}

static void BleIndicateConfirmCb(uint8_t server_id, uint16_t conn_id, errcode_t status)
{
    log_i("%s: server_id: %d, conn_id: %d, status:%d \n", __func__,
          server_id, conn_id, status);
    BLEMgrImpl().SendIndicationConfirm(conn_id);
}

#if WS63_RELEASE_VERSION
static void BleEnableCb(errcode_t status)
{
    log_i("%s:enable status: %d\n", __func__, status);
    BLEMgrImpl().SetStackInit();
}
#endif

void MatterBleStackOpen(void)
{
    ChipLogProgress(DeviceLayer, "MatterBleStackOpen");
#if WS63_RELEASE_VERSION
    if (ble_is_enable() == false) {
        enable_ble();
    } else {
        BLEMgrImpl().SetStackInit();
    }
#else
    enable_ble();
    BLEMgrImpl().SetStackInit();
#endif
}

void MatterInitCallback(void)
{
    errcode_t ret;
    gap_ble_callbacks_t gap_cb = {0};
    gatts_callbacks_t service_cb = {0};

    // register gap callbacks
    gap_cb.start_adv_cb = BleStartAdvCb;
    gap_cb.stop_adv_cb = BleStopAdvCb;
    gap_cb.conn_state_change_cb = BleConnStateChangeCb;
#if WS63_RELEASE_VERSION
    gap_cb.ble_enable_cb = BleEnableCb;
#endif
    ret = gap_ble_register_callbacks(&gap_cb);
    if (ret != ERRCODE_BT_SUCCESS) {
        log_i("%s:reg gap cbk failed \r\n", __func__);
        return;
    }

    // register gatts callbacks
    service_cb.add_service_cb = BleAddServiceCb;
    service_cb.add_characteristic_cb = BleAddCharacterCb;
    service_cb.add_descriptor_cb = BleAddDescriptorCb;
    service_cb.start_service_cb = BleStartServiceCb;
    service_cb.read_request_cb = BleReadRequestCb;
    service_cb.write_request_cb = BleWriteRequestCb;
    service_cb.mtu_changed_cb = BleMtuChangedCb;
    service_cb.indicate_confirm_cb = BleIndicateConfirmCb;
    ret = gatts_register_callbacks(&service_cb);
    if (ret != ERRCODE_BT_SUCCESS) {
        log_i("%s: reg service cbk failed \r\n", __func__);
        return;
    }
}

void MatterBleStartAdv(bool fast)
{
    uint8_t advData[MAX_ADV_DATA_LEN];
    uint8_t rspData[MAX_ADV_DATA_LEN];
    errcode_t ret;
    uint8_t advDataLen;
    uint8_t rspDataLen;
    gap_ble_adv_params_t adv_para = {0};
    gap_ble_config_adv_data_t cfg_adv_data;
    int adv_id = BTH_GAP_BLE_ADV_HANDLE_DEFAULT;

    // 1. set adv data
    (void)memset_s(advData, MAX_ADV_DATA_LEN, 0, MAX_ADV_DATA_LEN);
    (void)memset_s(rspData, MAX_ADV_DATA_LEN, 0, MAX_ADV_DATA_LEN);
    BLEMgrImpl().SetAdvertisingData((uint8_t *) advData, &advDataLen);
    BLEMgrImpl().SetScanRspData((uint8_t *) rspData, &rspDataLen);

    cfg_adv_data.adv_data = advData;
    cfg_adv_data.adv_length = advDataLen;
    cfg_adv_data.scan_rsp_data = rspData;
    cfg_adv_data.scan_rsp_length = rspDataLen;
    ret =  gap_ble_set_adv_data(BTH_GAP_BLE_ADV_HANDLE_DEFAULT, &cfg_adv_data);
    if (ret != ERRCODE_SUCC) {
        return;
    }

    // 2. set adv params
    if (fast) {
        adv_para.min_interval = CHIP_DEVICE_CONFIG_BLE_FAST_ADVERTISING_INTERVAL_MIN;
        adv_para.max_interval = CHIP_DEVICE_CONFIG_BLE_FAST_ADVERTISING_INTERVAL_MAX;
        ChipLogProgress(DeviceLayer, "fast advertising");
    } else {
        adv_para.min_interval = CHIP_DEVICE_CONFIG_BLE_SLOW_ADVERTISING_INTERVAL_MIN;
        adv_para.max_interval = CHIP_DEVICE_CONFIG_BLE_SLOW_ADVERTISING_INTERVAL_MAX;
        ChipLogProgress(DeviceLayer, "slow advertising");
    }
    adv_para.duration = BTH_GAP_BLE_ADV_FOREVER_DURATION;
    adv_para.peer_addr.type = BLE_PUBLIC_DEVICE_ADDRESS;
    adv_para.channel_map = BLE_ADV_CHANNEL_MAP_CH_DEFAULT;
    adv_para.adv_type = BLE_ADV_TYPE_CONNECTABLE_UNDIRECTED;
    adv_para.adv_filter_policy = BLE_ADV_FILTER_POLICY_SCAN_ANY_CONNECT_ANY;
    (void)memset_s(&adv_para.peer_addr.addr, BD_ADDR_LEN, 0, BD_ADDR_LEN);
    ret = gap_ble_set_adv_param(adv_id, &adv_para);
    if (ret != ERRCODE_SUCC) {
        return;
    }

    // 3. start adv
    ret = gap_ble_start_adv(adv_id);
    if (ret != ERRCODE_SUCC) {
        return;
    }
}

void MatterBleStopAdv(void)
{
    gap_ble_stop_adv(BTH_GAP_BLE_ADV_HANDLE_DEFAULT);
}

void MatterBleAddService(void)
{
    // register server
    bt_uuid_t app_uuid = { 2, {0x0, 0x0} }; // default server uuid
    gatts_register_server(&app_uuid, &g_server_id);

    // add service
    bt_uuid_t service_uuid = { 0 };
    (void)memcpy_s(service_uuid.uuid, BT_UUID_MAX_LEN, g_svcUuid, SVC_UUID_LEN);
    service_uuid.uuid_len = SVC_UUID_LEN;
    gatts_add_service(g_server_id, &service_uuid, true);

    (void)memset_s(g_characHdlTable, sizeof(CharacHandleMap) * MATTER_CHARAC_CNT_MAX,
                   0, sizeof(CharacHandleMap) * MATTER_CHARAC_CNT_MAX);
    BleAddCharactersAndDescriptors(g_server_id, g_srvcHandle);
    log_i("%s:start service,server id:%d,service handle:%d\r\n", __func__, g_server_id, g_srvcHandle);
    gatts_start_service(g_server_id, g_srvcHandle);
    WaitServiceStart();
}

void MatterCloseConnection(uint8_t conId)
{
    if (conId != g_conn_hdl) {
        ChipLogError(DeviceLayer, "wrong connection id");
    }
    gap_ble_disconnect_remote_device(&g_ble_addr);
}

void MatterTXCharSendIndication(uint8_t conId, uint16_t size, uint8_t * data)
{
    gatts_ntf_ind_t param = { 0 };
    errcode_t ret;
    ChipLogProgress(DeviceLayer, "%s: conId=%d size=%d \n", __func__,
                    conId, size);

    param.value = data;
    param.value_len = size;
    param.attr_handle = g_C2IndicateHandle;
    log_i("attr handle:%d \n", param.attr_handle);
    ret = gatts_notify_indicate(g_server_id, conId, &param);
    if (ret != ERRCODE_BT_SUCCESS) {
        log_i("Error:gatts_notify_indicate fail, ret:%x.\n", ret);
    }
}
