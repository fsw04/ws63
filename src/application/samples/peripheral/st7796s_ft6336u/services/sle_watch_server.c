#include "sle_watch_server.h"

#include <stdbool.h>
#include <string.h>
#include "common_def.h"
#include "nv.h"
#include "securec.h"
#include "sle_common.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_ssap_client.h"
#include "sle_ssap_server.h"
#include "soc_osal.h"
#include "vitals_report.h"
#include "../business/watch_borrow.h"
#include "../model/watch_model.h"
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
#define SLE_DATA_LEN 1500
#define SLE_SEEK_INTERVAL_DEFAULT 100
#define SLE_SEEK_WINDOW_DEFAULT 100
#define SLE_WATCH_SSAPC_CLIENT_ID 0
#define SLE_WATCH_SSAPC_EXCHANGE_CLIENT_ID 1
#define SLE_WATCH_REMOTE_VALUE_HANDLE_FALLBACK 17
#define SLE_ADV_DATA_TYPE_DISCOVERY_LEN 1
#define SLE_ADV_DATA_TYPE_UUID_LEN 2
#define SLE_ADV_DATA_TYPE_TX_POWER_LEN 1
#define SLE_ADV_DATA_LOCAL_NAME_LEN 16
#define WATCH_SCAN_NAME_PREFIX "watch-"
#define WATCH_SCAN_NAME_PREFIX_LEN 6
#define WATCH_SCAN_NAME_LEN 8
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
static uint8_t g_sle_conn_valid = 0;
static uint8_t g_server_id = 0;
static uint16_t g_service_handle = 0;
static uint16_t g_property_handle = 0;
static uint8_t g_connected_count = 0;
static uint8_t g_started = 0;
static uint8_t g_sle_ready = 0;
static uint8_t g_scan_active = 0;
static uint8_t g_connect_after_scan_stop = 0;
static uint8_t g_pending_connect_index = WATCH_MODEL_MAX_SCAN_RESULTS;
static sle_addr_t g_scan_addrs[WATCH_MODEL_MAX_SCAN_RESULTS];
static uint16_t g_device_conn_hdl[WATCH_MODEL_MAX_DEVICES];
static uint8_t g_device_conn_valid[WATCH_MODEL_MAX_DEVICES];
static uint16_t g_device_remote_value_hdl[WATCH_MODEL_MAX_DEVICES];

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

static uint8_t g_sle_local_addr[SLE_ADDR_LEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

static void sle_addr_to_text(const sle_addr_t *addr, char *buf, uint32_t len)
{
    if ((addr == NULL) || (buf == NULL) || (len == 0)) {
        return;
    }

    (void)snprintf_s(buf, len, len - 1, "%02X:%02X:%02X:%02X:%02X:%02X",
                     addr->addr[0], addr->addr[1], addr->addr[2],
                     addr->addr[3], addr->addr[4], addr->addr[5]);
}

static bool sle_addr_text_matches(const sle_addr_t *addr, const char *text)
{
    char addr_text[WATCH_MODEL_SHORT_LEN];

    if ((addr == NULL) || (text == NULL)) {
        return false;
    }

    sle_addr_to_text(addr, addr_text, sizeof(addr_text));
    return strncmp(addr_text, text, sizeof(addr_text)) == 0;
}

static bool sle_watch_name_is_target(const char *name)
{
    if (name == NULL) {
        return false;
    }

    return (strncmp(name, WATCH_SCAN_NAME_PREFIX, WATCH_SCAN_NAME_PREFIX_LEN) == 0) &&
           (name[WATCH_SCAN_NAME_PREFIX_LEN] >= '0') && (name[WATCH_SCAN_NAME_PREFIX_LEN] <= '9') &&
           (name[WATCH_SCAN_NAME_PREFIX_LEN + 1] >= '0') && (name[WATCH_SCAN_NAME_PREFIX_LEN + 1] <= '9') &&
           (name[WATCH_SCAN_NAME_LEN] == '\0');
}

static bool sle_watch_parse_type_len_name(const uint8_t *data, uint8_t data_len, char *name, uint32_t name_len)
{
    uint8_t index = 0;

    if ((data == NULL) || (name == NULL) || (name_len == 0)) {
        return false;
    }

    while (index + 1 < data_len) {
        uint8_t type = data[index];
        uint8_t len = data[index + 1];

        if ((len == 0) || ((uint16_t)index + 2 + len > data_len)) {
            return false;
        }

        if (type == SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME) {
            const uint8_t *value = &data[index + 2];
            uint8_t copy_len = len < (name_len - 1) ? len : (uint8_t)(name_len - 1);
            if (memcpy_s(name, name_len, value, copy_len) != EOK) {
                name[0] = '\0';
                return false;
            }
            name[copy_len] = '\0';
            return true;
        }

        index = (uint8_t)(index + 2 + len);
    }

    return false;
}

static bool sle_watch_parse_len_type_name(const uint8_t *data, uint8_t data_len, char *name, uint32_t name_len)
{
    uint8_t index = 0;

    if ((data == NULL) || (name == NULL) || (name_len == 0)) {
        return false;
    }

    while (index + 1 < data_len) {
        uint8_t len = data[index];
        uint8_t type = data[index + 1];

        if ((len <= 1) || ((uint16_t)index + 1 + len > data_len)) {
            return false;
        }

        if (type == SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME) {
            const uint8_t *value = &data[index + 2];
            uint8_t value_len = len - 1;
            uint8_t copy_len = value_len < (name_len - 1) ? value_len : (uint8_t)(name_len - 1);
            if (memcpy_s(name, name_len, value, copy_len) != EOK) {
                name[0] = '\0';
                return false;
            }
            name[copy_len] = '\0';
            return true;
        }

        index = (uint8_t)(index + 1 + len);
    }

    return false;
}

static bool sle_watch_parse_local_name(const uint8_t *data, uint8_t data_len, char *name, uint32_t name_len)
{
    return sle_watch_parse_type_len_name(data, data_len, name, name_len) ||
           sle_watch_parse_len_type_name(data, data_len, name, name_len);
}

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

static uint16_t sle_uuid_get_u2(const sle_uuid_t *uuid)
{
    if ((uuid == NULL) || (uuid->len != UUID_LEN_2)) {
        return 0;
    }
    return (uint16_t)(((uint16_t)uuid->uuid[15] << 8) | uuid->uuid[14]);
}

static int8_t sle_watch_find_device_by_conn_id(uint16_t conn_id)
{
    for (uint8_t i = 0; i < WATCH_MODEL_MAX_DEVICES; i++) {
        if ((g_device_conn_valid[i] != 0) && (g_device_conn_hdl[i] == conn_id)) {
            return (int8_t)i;
        }
    }
    return -1;
}

static void sle_watch_set_device_online(const sle_addr_t *addr, uint16_t conn_id, bool online)
{
    watch_model_snapshot_t model;

    if (addr == NULL) {
        return;
    }

    watch_model_get_snapshot(&model);
    for (uint8_t i = 0; i < model.device_count; i++) {
        if (sle_addr_text_matches(addr, model.devices[i].address)) {
            watch_device_t device = model.devices[i];
            device.connected = online ? 1 : 0;
            device.online = online ? 1 : 0;
            if (online) {
                g_device_conn_hdl[i] = conn_id;
                g_device_conn_valid[i] = 1;
                g_device_remote_value_hdl[i] = 0;
            } else {
                g_device_conn_hdl[i] = 0;
                g_device_conn_valid[i] = 0;
                g_device_remote_value_hdl[i] = 0;
            }
            watch_model_update_device(i, &device);
            return;
        }
    }
}

static void sle_watch_refresh_connected_count(void)
{
    watch_model_snapshot_t model;
    uint8_t count = 0;

    watch_model_get_snapshot(&model);
    for (uint8_t i = 0; i < model.device_count; i++) {
        if (model.devices[i].online != 0) {
            count++;
        }
    }
    g_connected_count = count;
    watch_model_set_sle(WATCH_LINK_BROADCASTING, "sle_speed_server", g_connected_count);
}

static void sle_watch_request_client_exchange(uint16_t conn_id)
{
    ssap_exchange_info_t info = {0};
    errcode_t ret;

    info.mtu_size = SLE_MTU_SIZE;
    info.version = 1;
    ret = ssapc_exchange_info_req(SLE_WATCH_SSAPC_EXCHANGE_CLIENT_ID, conn_id, &info);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[SLE] ssapc exchange req failed conn=%u ret=0x%x\r\n",
                    (unsigned int)conn_id, (unsigned int)ret);
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "client MTU req failed");
    }
}

errcode_t sle_watch_server_send(const uint8_t *data, uint16_t len)
{
    ssaps_ntf_ind_t param = {0};

    if ((data == NULL) || (len == 0) || (g_sle_conn_valid == 0)) {
        return ERRCODE_SLE_FAIL;
    }

    param.handle = g_property_handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.value = (uint8_t *)data;
    param.value_len = len;
    return ssaps_notify_indicate(g_server_id, g_sle_conn_hdl, &param);
}

errcode_t sle_watch_send_to_device(uint8_t device_index, const uint8_t *data, uint16_t len)
{
    ssapc_write_param_t param = {0};
    errcode_t ret;
    uint16_t handle;

    if ((data == NULL) || (len == 0) || (device_index >= WATCH_MODEL_MAX_DEVICES)) {
        return ERRCODE_SLE_FAIL;
    }
    if (g_device_conn_valid[device_index] == 0) {
        osal_printk("[SLE] ssapc write skipped, device=%u not connected\r\n", (unsigned int)device_index);
        return ERRCODE_SLE_FAIL;
    }

    handle = g_device_remote_value_hdl[device_index];
    if (handle == 0) {
        handle = SLE_WATCH_REMOTE_VALUE_HANDLE_FALLBACK;
    }

    param.handle = handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.data = (uint8_t *)data;
    param.data_len = len;
    ret = ssapc_write_cmd(SLE_WATCH_SSAPC_CLIENT_ID, g_device_conn_hdl[device_index], &param);
    osal_printk("[SLE] ssapc write cmd device=%u conn=%u handle=%u len=%u ret=0x%x\r\n",
                (unsigned int)device_index, (unsigned int)g_device_conn_hdl[device_index],
                (unsigned int)handle, (unsigned int)len, (unsigned int)ret);
    return ret;
}

errcode_t sle_watch_send_identity_to_device(uint8_t device_index, const char *name, const char *id_number)
{
    char payload[96];
    int n;

    if ((name == NULL) || (id_number == NULL)) {
        return ERRCODE_SLE_FAIL;
    }
    n = snprintf_s(payload, sizeof(payload), sizeof(payload) - 1, "ID:%s,%s", name, id_number);
    if (n <= 0) {
        return ERRCODE_SLE_FAIL;
    }
    return sle_watch_send_to_device(device_index, (const uint8_t *)payload, (uint16_t)(n + 1));
}

errcode_t sle_watch_send_unbind_to_device(uint8_t device_index)
{
    const char payload[] = "UNBIND";

    return sle_watch_send_to_device(device_index, (const uint8_t *)payload, (uint16_t)sizeof(payload));
}

static void sle_watch_parse_identity_reply(const char *payload)
{
    const char connected_prefix[] = "CONNECTED:";
    const char unbound_prefix[] = "UNBOUND";
    const char identity_ok[] = "IDENTITY_OK";
    const char *name_start;
    const char *comma;
    char name[WATCH_MODEL_NAME_LEN] = {0};
    char id_number[WATCH_MODEL_ID_LEN] = {0};

    if (payload == NULL) {
        return;
    }

    if (strncmp(payload, unbound_prefix, strlen(unbound_prefix)) == 0) {
        watch_borrow_on_unbound();
        return;
    }

    if (strncmp(payload, identity_ok, strlen(identity_ok)) == 0) {
        watch_borrow_on_identity_ack(NULL, NULL);
        return;
    }

    if (strncmp(payload, connected_prefix, strlen(connected_prefix)) != 0) {
        return;
    }

    name_start = payload + strlen(connected_prefix);
    comma = strchr(name_start, ',');
    if (comma != NULL) {
        uint32_t name_len = (uint32_t)(comma - name_start);
        if (name_len >= sizeof(name)) {
            name_len = sizeof(name) - 1;
        }
        (void)memcpy_s(name, sizeof(name), name_start, name_len);
        name[name_len] = '\0';
        (void)strncpy_s(id_number, sizeof(id_number), comma + 1, sizeof(id_number) - 1);
    } else {
        (void)strncpy_s(name, sizeof(name), name_start, sizeof(name) - 1);
    }

    watch_borrow_on_identity_ack(name, id_number);
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
    char payload[128];
    uint16_t copy_len;
    int8_t device_index;

    (void)server_id;
    (void)conn_id;
    (void)status;

    if ((write_cb_para == NULL) || (write_cb_para->length == 0) || (write_cb_para->value == NULL)) {
        return;
    }

    watch_model_add_log(WATCH_LOG_DEVICE, "设备", "SLE data received");
    copy_len = write_cb_para->length < (sizeof(payload) - 1) ? write_cb_para->length : (sizeof(payload) - 1);
    if (memcpy_s(payload, sizeof(payload), write_cb_para->value, copy_len) != EOK) {
        return;
    }
    payload[copy_len] = '\0';
    sle_watch_parse_identity_reply(payload);

    device_index = sle_watch_find_device_by_conn_id(conn_id);
    vitals_report_handle(device_index >= 0 ? (uint8_t)device_index : VITALS_REPORT_DEVICE_UNKNOWN,
                         write_cb_para->value, write_cb_para->length);
}

static void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id, ssap_exchange_info_t *mtu_size,
                                  errcode_t status)
{
    (void)server_id;
    osal_printk("[SLE] ssaps mtu changed conn=%u status=0x%x mtu=%u version=%u\r\n",
                (unsigned int)conn_id, (unsigned int)status,
                (unsigned int)(mtu_size != NULL ? mtu_size->mtu_size : 0),
                (unsigned int)(mtu_size != NULL ? mtu_size->version : 0));
    watch_model_add_log(WATCH_LOG_SLE, "SLE", "MTU updated");
}

static void ssapc_parse_peer_payload(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    char payload[512];
    uint16_t copy_len;
    int8_t device_index;

    if ((data == NULL) || (len == 0)) {
        return;
    }

    copy_len = len < (sizeof(payload) - 1) ? len : (sizeof(payload) - 1);
    if (memcpy_s(payload, sizeof(payload), data, copy_len) != EOK) {
        return;
    }
    payload[copy_len] = '\0';
    if (payload[0] == '{') {
        osal_printk("[SLE] report notify conn=%u len=%u json:%s\r\n",
                    (unsigned int)conn_id, (unsigned int)len, payload);
    }

    sle_watch_parse_identity_reply(payload);
    device_index = sle_watch_find_device_by_conn_id(conn_id);
    vitals_report_handle(device_index >= 0 ? (uint8_t)device_index : VITALS_REPORT_DEVICE_UNKNOWN, data, len);
}

static void ssapc_notification_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
                                   errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)status;

    if (data == NULL) {
        return;
    }
    watch_model_add_log(WATCH_LOG_DEVICE, "设备", "SLE notify received");
    ssapc_parse_peer_payload(conn_id, data->data, data->data_len);
}

static void ssapc_indication_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
                                 errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)status;

    if (data == NULL) {
        return;
    }
    watch_model_add_log(WATCH_LOG_DEVICE, "设备", "SLE indicate received");
    ssapc_parse_peer_payload(conn_id, data->data, data->data_len);
}

static void ssapc_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
                                    errcode_t status)
{
    ssapc_find_structure_param_t find_param = {0};

    (void)client_id;
    if (status != ERRCODE_SLE_SUCCESS) {
        osal_printk("[SLE] ssapc exchange failed conn=%u status=0x%x\r\n",
                    (unsigned int)conn_id, (unsigned int)status);
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "client MTU failed");
        return;
    }

    find_param.type = SSAP_FIND_TYPE_PRIMARY_SERVICE;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    osal_printk("[SLE] ssapc exchange ok conn=%u mtu=%u version=%u, find BS21 service\r\n",
                (unsigned int)conn_id,
                (unsigned int)(param != NULL ? param->mtu_size : 0),
                (unsigned int)(param != NULL ? param->version : 0));
    if (ssapc_find_structure(SLE_WATCH_SSAPC_CLIENT_ID, conn_id, &find_param) != ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "find service failed");
    }
}

static void ssapc_find_structure_cbk(uint8_t client_id, uint16_t conn_id, ssapc_find_service_result_t *service,
                                     errcode_t status)
{
    ssapc_find_structure_param_t find_param = {0};

    (void)client_id;
    if ((status != ERRCODE_SLE_SUCCESS) || (service == NULL)) {
        return;
    }
    if (sle_uuid_get_u2(&service->uuid) != SLE_UUID_SERVER_SERVICE) {
        return;
    }

    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = service->start_hdl;
    find_param.end_hdl = service->end_hdl;
    if (ssapc_find_structure(SLE_WATCH_SSAPC_CLIENT_ID, conn_id, &find_param) != ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "find property failed");
    }
}

static void ssapc_find_property_cbk(uint8_t client_id, uint16_t conn_id, ssapc_find_property_result_t *property,
                                    errcode_t status)
{
    int8_t index;

    (void)client_id;
    if ((status != ERRCODE_SLE_SUCCESS) || (property == NULL)) {
        return;
    }
    if (sle_uuid_get_u2(&property->uuid) != SLE_UUID_SERVER_NTF_REPORT) {
        return;
    }

    index = sle_watch_find_device_by_conn_id(conn_id);
    if (index < 0) {
        return;
    }

    g_device_remote_value_hdl[(uint8_t)index] = property->handle;
    osal_printk("[SLE] BS21 property ready conn=%u handle=%u\r\n",
                (unsigned int)conn_id, (unsigned int)property->handle);
    watch_model_add_log(WATCH_LOG_SLE, "SLE", "device property ready");
}

static void ssapc_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                         ssapc_find_structure_result_t *structure_result, errcode_t status)
{
    int8_t index;

    (void)client_id;
    (void)structure_result;
    if (status != ERRCODE_SLE_SUCCESS) {
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "find complete failed");
        return;
    }

    index = sle_watch_find_device_by_conn_id(conn_id);
    if ((index >= 0) && (g_device_remote_value_hdl[(uint8_t)index] == 0)) {
        g_device_remote_value_hdl[(uint8_t)index] = SLE_WATCH_REMOTE_VALUE_HANDLE_FALLBACK;
        osal_printk("[SLE] BS21 property not found conn=%u, use fallback handle=%u\r\n",
                    (unsigned int)conn_id, (unsigned int)SLE_WATCH_REMOTE_VALUE_HANDLE_FALLBACK);
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "use fallback handle");
    }
}

static void ssapc_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result,
                                errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)write_result;

    watch_model_add_log(status == ERRCODE_SLE_SUCCESS ? WATCH_LOG_SLE : WATCH_LOG_WARNING,
                        "SLE", status == ERRCODE_SLE_SUCCESS ? "client write ok" : "client write failed");
}

static void sle_ssapc_register_cbks(void)
{
    ssapc_callbacks_t ssapc_cbk = {0};
    ssapc_cbk.exchange_info_cb = ssapc_exchange_info_cbk;
    ssapc_cbk.find_structure_cb = ssapc_find_structure_cbk;
    ssapc_cbk.find_structure_cmp_cb = ssapc_find_structure_cmp_cbk;
    ssapc_cbk.ssapc_find_property_cbk = ssapc_find_property_cbk;
    ssapc_cbk.write_cfm_cb = ssapc_write_cfm_cbk;
    ssapc_cbk.notification_cb = ssapc_notification_cbk;
    ssapc_cbk.indication_cb = ssapc_indication_cbk;
    ssapc_register_callbacks(&ssapc_cbk);
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
    (void)disc_reason;

    g_sle_conn_hdl = conn_id;
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        sle_connection_param_update_t param = {0};
        errcode_t data_len_ret;
        g_sle_conn_valid = 1;
        param.conn_id = conn_id;
        param.interval_min = SLE_CONN_INTERVAL_DEFAULT;
        param.interval_max = SLE_CONN_INTERVAL_DEFAULT;
        param.max_latency = 0;
        param.supervision_timeout = SLE_CONN_TIMEOUT_DEFAULT;
        (void)sle_update_connect_param(&param);
        data_len_ret = sle_set_data_len(conn_id, SLE_DATA_LEN);
        osal_printk("[SLE] set data len conn=%u len=%u ret=0x%x\r\n",
                    (unsigned int)conn_id, (unsigned int)SLE_DATA_LEN, (unsigned int)data_len_ret);
        if (pair_state == SLE_PAIR_NONE) {
            (void)sle_pair_remote_device(addr);
        }
        sle_watch_set_device_online(addr, conn_id, true);
        sle_watch_refresh_connected_count();
        if (pair_state == SLE_PAIR_PAIRED) {
            sle_watch_request_client_exchange(conn_id);
        }
        watch_model_add_log(WATCH_LOG_SLE, "SLE", "device connected");
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_sle_conn_valid = 0;
        sle_watch_set_device_online(addr, conn_id, false);
        sle_watch_refresh_connected_count();
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
    if (status != ERRCODE_SLE_SUCCESS) {
        (void)sle_remove_paired_remote_device(addr);
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
        return;
    }

    osal_printk("[SLE] pair complete conn=%u, start ssapc exchange\r\n", (unsigned int)conn_id);
    sle_watch_request_client_exchange(conn_id);
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
    (void)memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, g_sle_local_addr, SLE_ADDR_LEN);
    return sle_set_announce_param(param.announce_handle, &param);
}

static void sle_watch_set_local_addr(void)
{
    sle_addr_t addr = {0};

    addr.type = 0;
    (void)memcpy_s(addr.addr, SLE_ADDR_LEN, g_sle_local_addr, SLE_ADDR_LEN);
    (void)sle_set_local_addr(&addr);
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

errcode_t sle_watch_scan_start(void)
{
    sle_seek_param_t param = {0};
    errcode_t ret;

    watch_model_clear_scan_results();
    if (g_sle_ready == 0) {
        watch_model_set_scan_active(0);
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "scan requested before ready");
        return ERRCODE_FAIL;
    }

    watch_model_set_scan_active(1);
    watch_model_add_log(WATCH_LOG_SLE, "SLE", "scan watch devices");
    g_scan_active = 1;
    g_connect_after_scan_stop = 0;
    g_pending_connect_index = WATCH_MODEL_MAX_SCAN_RESULTS;
    (void)memset_s(g_scan_addrs, sizeof(g_scan_addrs), 0, sizeof(g_scan_addrs));

    (void)sle_stop_announce(SLE_ADV_HANDLE_DEFAULT);
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 0;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;

    ret = sle_set_seek_param(&param);
    if (ret != ERRCODE_SUCC) {
        g_scan_active = 0;
        watch_model_set_scan_active(0);
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "set scan param failed");
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
        return ret;
    }

    ret = sle_start_seek();
    if (ret != ERRCODE_SUCC) {
        g_scan_active = 0;
        watch_model_set_scan_active(0);
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "start scan failed");
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    }
    return ret;
}

errcode_t sle_watch_scan_stop(void)
{
    errcode_t ret;

    watch_model_set_scan_active(0);
    if (g_scan_active == 0) {
        return ERRCODE_SUCC;
    }

    g_scan_active = 0;
    ret = sle_stop_seek();
    if (ret != ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "stop scan failed");
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
        return ret;
    }

    watch_model_add_log(WATCH_LOG_SLE, "SLE", "scan stopped");
    return ret;
}

errcode_t sle_watch_connect_scan_result(uint8_t scan_index)
{
    errcode_t ret;

    if ((g_sle_ready == 0) || (scan_index >= WATCH_MODEL_MAX_SCAN_RESULTS)) {
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "connect requested before ready");
        return ERRCODE_FAIL;
    }

    ret = watch_model_add_device_from_scan(scan_index);
    if (ret != ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "add scan device failed");
        return ret;
    }

    g_pending_connect_index = scan_index;
    if (g_scan_active != 0) {
        g_connect_after_scan_stop = 1;
        watch_model_set_scan_active(0);
        ret = sle_stop_seek();
        if (ret != ERRCODE_SUCC) {
            g_connect_after_scan_stop = 0;
            g_pending_connect_index = WATCH_MODEL_MAX_SCAN_RESULTS;
            watch_model_add_log(WATCH_LOG_WARNING, "SLE", "stop scan before connect failed");
            (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
            return ret;
        }
        g_scan_active = 0;
        watch_model_add_log(WATCH_LOG_SLE, "SLE", "connect after scan stop");
        return ERRCODE_SUCC;
    }

    watch_model_add_log(WATCH_LOG_SLE, "SLE", "connect device");
    (void)sle_stop_announce(SLE_ADV_HANDLE_DEFAULT);
    ret = sle_connect_remote_device(&g_scan_addrs[scan_index]);
    if (ret != ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "connect device failed");
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    }
    return ret;
}

static void sle_seek_enable_cbk(errcode_t status)
{
    if (status != ERRCODE_SLE_SUCCESS) {
        g_scan_active = 0;
        watch_model_set_scan_active(0);
        watch_model_add_log(WATCH_LOG_WARNING, "SLE", "scan enable failed");
    }
}

static void sle_seek_disable_cbk(errcode_t status)
{
    errcode_t ret;

    g_scan_active = 0;
    watch_model_set_scan_active(0);
    if (g_connect_after_scan_stop != 0) {
        g_connect_after_scan_stop = 0;
        if ((status == ERRCODE_SLE_SUCCESS) && (g_pending_connect_index < WATCH_MODEL_MAX_SCAN_RESULTS)) {
            (void)sle_stop_announce(SLE_ADV_HANDLE_DEFAULT);
            ret = sle_connect_remote_device(&g_scan_addrs[g_pending_connect_index]);
            if (ret != ERRCODE_SUCC) {
                watch_model_add_log(WATCH_LOG_WARNING, "SLE", "connect device failed");
                (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
            } else {
                watch_model_add_log(WATCH_LOG_SLE, "SLE", "connect device");
            }
        } else {
            watch_model_add_log(WATCH_LOG_WARNING, "SLE", "scan stop before connect failed");
            (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
        }
        g_pending_connect_index = WATCH_MODEL_MAX_SCAN_RESULTS;
        return;
    }
    if (g_sle_ready != 0) {
        (void)sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    }
}

static void sle_seek_result_cbk(sle_seek_result_info_t *seek_result_data)
{
    watch_scan_device_t device = {0};
    uint8_t scan_index;

    if ((g_scan_active == 0) || (seek_result_data == NULL) || (seek_result_data->data == NULL) ||
        (seek_result_data->data_length == 0)) {
        return;
    }

    if (!sle_watch_parse_local_name(seek_result_data->data, seek_result_data->data_length, device.name,
                                    sizeof(device.name))) {
        return;
    }
    if (!sle_watch_name_is_target(device.name)) {
        return;
    }

    sle_addr_to_text(&seek_result_data->addr, device.address, sizeof(device.address));
    device.rssi = (int8_t)seek_result_data->rssi;
    if (watch_model_add_scan_result(&device, &scan_index) == ERRCODE_SUCC) {
        (void)memcpy_s(&g_scan_addrs[scan_index], sizeof(g_scan_addrs[scan_index]),
                       &seek_result_data->addr, sizeof(seek_result_data->addr));
    }
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
        sle_watch_set_local_addr();
        (void)sle_uuid_server_adv_init();
        g_sle_ready = 1;
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
    seek_cbks.seek_enable_cb = sle_seek_enable_cbk;
    seek_cbks.seek_disable_cb = sle_seek_disable_cbk;
    seek_cbks.seek_result_cb = sle_seek_result_cbk;
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
    sle_ssapc_register_cbks();
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
