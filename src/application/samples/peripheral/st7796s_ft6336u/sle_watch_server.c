#include "sle_watch_server.h"

#include <string.h>
#include "common_def.h"
#include "mqtt_task.h"
#include "nv.h"
#include "securec.h"
#include "sle_common.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_ssap_server.h"
#include "soc_osal.h"
#include "watch_model.h"
#include "watchdog.h"

#define SLE_UUID_SERVER_SERVICE 0x060B
#define SLE_UUID_SERVER_NTF_REPORT 0x1122
#define SLE_UUID_TEST_PROPERTIES (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)
#define SLE_UUID_TEST_DESCRIPTOR (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)
#define SLE_ADV_HANDLE_DEFAULT 1
#define UUID_LEN_2 2
#define SLE_CONN_INTERVAL_DEFAULT 0xA0
#define SLE_CONN_TIMEOUT_DEFAULT 0x1f4
#define SLE_MTU_SIZE 1500
#define SLE_TASK_STACK_SIZE 0x2000
#define SLE_TASK_PRIORITY 26
#define SLE_ADV_TX_POWER 20
#define SLE_ADV_INTERVAL_DEFAULT 0xC8
#define SLE_CONN_MAX_LATENCY 0x1F3
#define SLE_ADV_DATA_TYPE_DISCOVERY_LEN 1
#define SLE_ADV_DATA_TYPE_UUID_LEN 2
#define SLE_ADV_DATA_TYPE_TX_POWER_LEN 1
#define SLE_ADV_DATA_LOCAL_NAME_LEN 16
#define OCTET_BIT_LEN 8

#define encode2byte_little(_ptr, data) \
    do { \
        *(uint8_t *)((_ptr) + 1) = (uint8_t)((data) >> 8); \
        *(uint8_t *)(_ptr) = (uint8_t)(data); \
    } while (0)

typedef enum {
    SLE_ADV_CHANNEL_MAP_77 = 0x01,
    SLE_ADV_CHANNEL_MAP_78 = 0x02,
    SLE_ADV_CHANNEL_MAP_79 = 0x04,
    SLE_ADV_CHANNEL_MAP_DEFAULT = 0x07
} sle_adv_channel_map_t;

typedef enum {
    SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL = 0x01,
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_16BIT_SERVICE_UUIDS = 0x05,
    SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x0B,
    SLE_ADV_DATA_TYPE_TX_POWER_LEVEL = 0x0C,
} sle_adv_data_type_t;

static char g_sle_uuid_app_uuid[UUID_LEN_2] = {0x0, 0x0};
static char g_sle_property_value[OCTET_BIT_LEN] = {0};
static uint16_t g_sle_conn_hdl = 0;
static uint8_t g_server_id = 0;
static uint16_t g_service_handle = 0;
static uint16_t g_property_handle = 0;
static uint8_t g_connected_count = 0;
static uint8_t g_started = 0;

static uint8_t g_sle_announce_data[] = {
    SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL,
    SLE_ADV_DATA_TYPE_DISCOVERY_LEN,
    SLE_ANNOUNCE_LEVEL_NORMAL,
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_16BIT_SERVICE_UUIDS,
    SLE_ADV_DATA_TYPE_UUID_LEN,
    0x0B, 0x06,
};

static uint8_t g_sle_scan_rsp_data[] = {
    SLE_ADV_DATA_TYPE_TX_POWER_LEVEL,
    SLE_ADV_DATA_TYPE_TX_POWER_LEN,
    SLE_ADV_TX_POWER,
    SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME,
    SLE_ADV_DATA_LOCAL_NAME_LEN,
    's', 'l', 'e', '_', 's', 'p', 'e', 'e', 'd', '_', 's', 'e', 'r', 'v', 'e', 'r'
};

static uint8_t g_sle_uuid_base[] = {
    0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
    0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void sle_uuid_set_base(sle_uuid_t *out)
{
    (void)memcpy_s(out->uuid, SLE_UUID_LEN, g_sle_uuid_base, SLE_UUID_LEN);
    out->len = UUID_LEN_2;
}

static void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    sle_uuid_set_base(out);
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[14], u2);
}

errcode_t sle_watch_server_send(const uint8_t *data, uint16_t len)
{
    ssaps_ntf_ind_t param = {0};

    if ((data == NULL) || (len == 0) || (g_sle_conn_hdl == 0)) {
        return ERRCODE_SLE_FAIL;
    }

    param.handle = g_property_handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.value = (uint8_t *)data;
    param.value_len = len;
    return ssaps_notify_indicate(g_server_id, g_sle_conn_hdl, &param);
}

static void ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para,
                                   errcode_t status)
{
    (void)server_id;
    (void)conn_id;
    (void)read_cb_para;
    (void)status;
    watch_model_add_log(WATCH_LOG_SLE, "SLE", "read request");
}

static void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para,
                                    errcode_t status)
{
    (void)server_id;
    (void)conn_id;
    (void)status;

    if ((write_cb_para == NULL) || (write_cb_para->length == 0) || (write_cb_para->value == NULL)) {
        return;
    }

    watch_model_add_log(WATCH_LOG_DEVICE, "设备", "SLE data received");
    if (g_mqtt_msg_queue != 0) {
        (void)osal_msg_queue_write_copy(g_mqtt_msg_queue, write_cb_para->value, write_cb_para->length, 0);
    }
}

static void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id, ssap_exchange_info_t *mtu_size,
                                  errcode_t status)
{
    (void)server_id;
    (void)conn_id;
    (void)mtu_size;
    (void)status;
    watch_model_add_log(WATCH_LOG_SLE, "SLE", "MTU updated");
}

static void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    (void)server_id;
    (void)handle;
    if (status == ERRCODE_SLE_SUCCESS) {
        watch_model_add_log(WATCH_LOG_SLE, "SLE", "service started");
    }
}

static void sle_ssaps_register_cbks(void)
{
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.start_service_cb = ssaps_start_service_cbk;
    ssaps_cbk.mtu_changed_cb = ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb = ssaps_write_request_cbk;
    ssaps_register_callbacks(&ssaps_cbk);
}

static errcode_t sle_uuid_server_service_add(void)
{
    sle_uuid_t service_uuid = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    return ssaps_add_service_sync(g_server_id, &service_uuid, true, &g_service_handle);
}

static errcode_t sle_uuid_server_property_add(void)
{
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};
    errcode_t ret;

    property.permissions = SLE_UUID_TEST_PROPERTIES;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);
    property.value = osal_vmalloc(sizeof(g_sle_property_value));
    property.value_len = sizeof(g_sle_property_value);
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE |
                                  SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    if (property.value == NULL) {
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value,
                 sizeof(g_sle_property_value)) != EOK) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    osal_vfree(property.value);
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ret;
    }

    descriptor.permissions = SLE_UUID_TEST_DESCRIPTOR;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);
    return ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
}

static errcode_t sle_uuid_server_add(void)
{
    sle_uuid_t app_uuid = {0};
    errcode_t ret;

    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != EOK) {
        return ERRCODE_SLE_FAIL;
    }

    ret = ssaps_register_server(&app_uuid, &g_server_id);
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ret;
    }
    if (sle_uuid_server_service_add() != ERRCODE_SLE_SUCCESS) {
        return ERRCODE_SLE_FAIL;
    }
    if (sle_uuid_server_property_add() != ERRCODE_SLE_SUCCESS) {
        return ERRCODE_SLE_FAIL;
    }
    return ssaps_start_service(g_server_id, g_service_handle);
}

static void sle_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr, sle_acb_state_t conn_state,
                                          sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    (void)addr;
    (void)pair_state;
    (void)disc_reason;

    g_sle_conn_hdl = conn_id;
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        sle_connection_param_update_t param = {0};
        param.conn_id = conn_id;
        param.interval_min = SLE_CONN_INTERVAL_DEFAULT;
        param.interval_max = SLE_CONN_INTERVAL_DEFAULT;
        param.max_latency = 0;
        param.supervision_timeout = SLE_CONN_TIMEOUT_DEFAULT;
        (void)sle_update_connect_param(&param);
        if (g_connected_count < WATCH_MODEL_MAX_DEVICES) {
            g_connected_count++;
        }
        watch_model_set_sle(WATCH_LINK_BROADCASTING, "sle_speed_server", g_connected_count);
        watch_model_add_log(WATCH_LOG_SLE, "SLE", "device connected");
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        if (g_connected_count > 0) {
            g_connected_count--;
        }
        watch_model_set_sle(WATCH_LINK_BROADCASTING, "sle_speed_server", g_connected_count);
        watch_model_add_log(WATCH_LOG_WARNING, "警告", "SLE device offline");
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    }
}

static void sle_auth_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status,
                                  const sle_auth_info_evt_t *evt)
{
    (void)conn_id;
    (void)evt;
    if (status != ERRCODE_SLE_SUCCESS) {
        (void)sle_remove_paired_remote_device(addr);
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    }
}

static void sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    (void)conn_id;
    if (status != ERRCODE_SLE_SUCCESS) {
        (void)sle_remove_paired_remote_device(addr);
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    }
}

static void sle_conn_register_cbks(void)
{
    sle_connection_callbacks_t conn_cbks = {0};
    conn_cbks.connect_state_changed_cb = sle_connect_state_changed_cbk;
    conn_cbks.auth_complete_cb = sle_auth_complete_cbk;
    conn_cbks.pair_complete_cb = sle_pair_complete_cbk;
    sle_connection_register_callbacks(&conn_cbks);
}

static void sle_ssaps_set_info(void)
{
    ssap_exchange_info_t info = {0};
    info.mtu_size = SLE_MTU_SIZE;
    info.version = 1;
    ssaps_set_info(g_server_id, &info);
}

static int sle_set_default_announce_param(void)
{
    sle_announce_param_t param = {0};
    uint8_t mac[SLE_ADDR_LEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announce_handle = SLE_ADV_HANDLE_DEFAULT;
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announce_channel_map = SLE_ADV_CHANNEL_MAP_DEFAULT;
    param.announce_interval_min = SLE_ADV_INTERVAL_DEFAULT;
    param.announce_interval_max = SLE_ADV_INTERVAL_DEFAULT;
    param.conn_interval_min = 0x14;
    param.conn_interval_max = 0x14;
    param.conn_max_latency = SLE_CONN_MAX_LATENCY;
    param.conn_supervision_timeout = SLE_CONN_TIMEOUT_DEFAULT;
    param.announce_tx_power = SLE_ADV_TX_POWER;
    param.own_addr.type = 0;
    (void)memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, mac, SLE_ADDR_LEN);
    return sle_set_announce_param(param.announce_handle, &param);
}

static int sle_set_default_announce_data(void)
{
    sle_announce_data_t data = {0};
    data.announce_data = g_sle_announce_data;
    data.announce_data_len = sizeof(g_sle_announce_data);
    data.seek_rsp_data = g_sle_scan_rsp_data;
    data.seek_rsp_data_len = sizeof(g_sle_scan_rsp_data);
    return sle_set_announce_data(SLE_ADV_HANDLE_DEFAULT, &data);
}

static errcode_t sle_uuid_server_adv_init(void)
{
    (void)sle_set_default_announce_param();
    (void)sle_set_default_announce_data();
    return sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
}

static void sle_announce_enable_cbk(uint32_t announce_id, errcode_t status)
{
    (void)announce_id;
    if (status == ERRCODE_SLE_SUCCESS) {
        watch_model_set_sle(WATCH_LINK_BROADCASTING, "sle_speed_server", g_connected_count);
        watch_model_add_log(WATCH_LOG_SLE, "SLE", "broadcasting");
    }
}

static void sle_announce_disable_cbk(uint32_t announce_id, errcode_t status)
{
    (void)announce_id;
    (void)status;
}

static void sle_announce_terminal_cbk(uint32_t announce_id)
{
    (void)announce_id;
    watch_model_add_log(WATCH_LOG_SLE, "SLE", "broadcast stopped");
}

void sle_enable_server_cbk(void)
{
    uint16_t nv_value_len = 0;
    uint8_t nv_value = 7;

    (void)uapi_nv_read(0x20A0, sizeof(uint16_t), &nv_value_len, &nv_value);
    if (nv_value != 7) {
        nv_value = 7;
        (void)uapi_nv_write(0x20A0, (uint8_t *)&nv_value, sizeof(nv_value));
    }

    if (sle_uuid_server_add() == ERRCODE_SLE_SUCCESS) {
        sle_ssaps_set_info();
        (void)sle_uuid_server_adv_init();
        watch_model_add_log(WATCH_LOG_SLE, "SLE", "server initialized");
    } else {
        watch_model_set_sle(WATCH_LINK_ERROR, "sle_speed_server", g_connected_count);
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "server init failed");
    }
}

static void sle_enable_cbk(errcode_t status)
{
    if (status == ERRCODE_SLE_SUCCESS) {
        sle_enable_server_cbk();
    } else {
        watch_model_set_sle(WATCH_LINK_ERROR, "sle_speed_server", g_connected_count);
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "enable failed");
    }
}

static void sle_announce_register_cbks(void)
{
    sle_announce_seek_callbacks_t seek_cbks = {0};
    seek_cbks.announce_enable_cb = sle_announce_enable_cbk;
    seek_cbks.announce_disable_cb = sle_announce_disable_cbk;
    seek_cbks.announce_terminal_cb = sle_announce_terminal_cbk;
    seek_cbks.sle_enable_cb = sle_enable_cbk;
    sle_announce_seek_register_callbacks(&seek_cbks);
}

static int sle_watch_server_init(void)
{
    if (g_started != 0) {
        return 0;
    }
    g_started = 1;

    uapi_watchdog_disable();
    watch_model_set_sle(WATCH_LINK_CONNECTING, "sle_speed_server", 0);
    watch_model_add_log(WATCH_LOG_SLE, "SLE", "enable server");
    sle_announce_register_cbks();
    sle_conn_register_cbks();
    sle_ssaps_register_cbks();
    (void)enable_sle();
    return 0;
}

static void *sle_watch_task(const char *arg)
{
    (void)arg;
    osal_msleep(1000);
    (void)sle_watch_server_init();
    return NULL;
}

void sle_watch_server_start(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)sle_watch_task, 0, "WatchSleTask", SLE_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_TASK_PRIORITY);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}
