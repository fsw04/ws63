#include "wifi_task.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "lwip/netifapi.h"
#include "mqtt_task.h"
#include "nv.h"
#include "securec.h"
#include "soc_osal.h"
#include "../model/watch_model.h"
#include "wifi_device.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "wifi_provision.h"

#define WIFI_STA_IP_MAX_GET_TIMES 15
#define WIFI_RSSI_DEFAULT (-42)
#define WIFI_PROFILE_NV_KEY 0x5001
#define WIFI_PROFILE_MAGIC 0x57434657U
#define WIFI_PROFILE_VERSION 1
#define WIFI_TASK_QUEUE_LEN 4
#define WIFI_TASK_STACK_SIZE 0x1800
#define WIFI_TASK_PRIORITY 24

typedef enum {
    WIFI_REQ_CONNECT = 0,
    WIFI_REQ_DISCONNECT,
} wifi_request_type_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    char ssid[WIFI_MAX_SSID_LEN];
    char pwd[WIFI_MAX_KEY_LEN];
} wifi_profile_store_t;

typedef struct {
    wifi_request_type_t type;
    wifi_profile_t profile;
} wifi_task_request_t;

static unsigned long g_wifi_msg_queue = 0;
static uint8_t g_wifi_task_started = 0;

static void ip_to_text(uint32_t addr, char *buf, uint32_t len)
{
    (void)snprintf_s(buf, len, len - 1, "%u.%u.%u.%u",
                     (addr & 0x000000ff),
                     (addr & 0x0000ff00) >> 8,
                     (addr & 0x00ff0000) >> 16,
                     (addr & 0xff000000) >> 24);
}

static errcode_t wifi_profile_save(const char *ssid, const char *pwd)
{
    wifi_profile_store_t store = {0};
    errcode_t write_ret;
    errcode_t flush_ret;

    if ((ssid == NULL) || (pwd == NULL) || (ssid[0] == '\0')) {
        return ERRCODE_FAIL;
    }

    store.magic = WIFI_PROFILE_MAGIC;
    store.version = WIFI_PROFILE_VERSION;
    if (strncpy_s(store.ssid, sizeof(store.ssid), ssid, sizeof(store.ssid) - 1) != EOK) {
        return ERRCODE_FAIL;
    }
    if (strncpy_s(store.pwd, sizeof(store.pwd), pwd, sizeof(store.pwd) - 1) != EOK) {
        return ERRCODE_FAIL;
    }

    write_ret = uapi_nv_write(WIFI_PROFILE_NV_KEY, (uint8_t *)&store, sizeof(store));
    osal_printk("[WIFI] profile save key=0x%x ssid=%s len=%u write_ret=0x%x\r\n",
                (unsigned int)WIFI_PROFILE_NV_KEY, store.ssid,
                (unsigned int)sizeof(store), (unsigned int)write_ret);
    if (write_ret != ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "save profile failed");
        return ERRCODE_FAIL;
    }
    flush_ret = uapi_nv_flush();
    osal_printk("[WIFI] profile flush key=0x%x ret=0x%x\r\n",
                (unsigned int)WIFI_PROFILE_NV_KEY, (unsigned int)flush_ret);
    if (flush_ret != ERRCODE_SUCC) {
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "flush profile failed");
        return ERRCODE_FAIL;
    }
    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "profile saved");
    return ERRCODE_SUCC;
}

bool wifi_task_get_saved_profile(wifi_profile_t *profile)
{
    wifi_profile_store_t store = {0};
    uint16_t value_len = 0;
    errcode_t read_ret;

    if (profile == NULL) {
        return false;
    }

    read_ret = uapi_nv_read(WIFI_PROFILE_NV_KEY, sizeof(store), &value_len, (uint8_t *)&store);
    store.ssid[sizeof(store.ssid) - 1] = '\0';
    store.pwd[sizeof(store.pwd) - 1] = '\0';
    osal_printk("[WIFI] profile read key=0x%x ret=0x%x value_len=%u magic=0x%x version=%u ssid=%s\r\n",
                (unsigned int)WIFI_PROFILE_NV_KEY, (unsigned int)read_ret,
                (unsigned int)value_len, (unsigned int)store.magic,
                (unsigned int)store.version, store.ssid);
    if (read_ret != ERRCODE_SUCC) {
        profile->ssid[0] = '\0';
        profile->pwd[0] = '\0';
        return false;
    }

    if ((value_len != sizeof(store)) || (store.magic != WIFI_PROFILE_MAGIC) ||
        (store.version != WIFI_PROFILE_VERSION) || (store.ssid[0] == '\0')) {
        osal_printk("[WIFI] profile invalid key=0x%x value_len=%u expected=%u magic=0x%x version=%u\r\n",
                    (unsigned int)WIFI_PROFILE_NV_KEY, (unsigned int)value_len,
                    (unsigned int)sizeof(store), (unsigned int)store.magic,
                    (unsigned int)store.version);
        profile->ssid[0] = '\0';
        profile->pwd[0] = '\0';
        return false;
    }

    if (strncpy_s(profile->ssid, sizeof(profile->ssid), store.ssid, sizeof(profile->ssid) - 1) != EOK) {
        return false;
    }
    if (strncpy_s(profile->pwd, sizeof(profile->pwd), store.pwd, sizeof(profile->pwd) - 1) != EOK) {
        return false;
    }
    osal_printk("[WIFI] profile loaded key=0x%x ssid=%s\r\n",
                (unsigned int)WIFI_PROFILE_NV_KEY, profile->ssid);
    return true;
}

errcode_t wifi_connect_start(const char *ssid, const char *pwd)
{
    wifi_sta_config_stru expected_bss = {0};
    struct netif *netif_p = NULL;
    char ip_text[WATCH_MODEL_SHORT_LEN] = {0};

    if ((ssid == NULL) || (pwd == NULL)) {
        return ERRCODE_FAIL;
    }

    watch_model_set_wifi(WATCH_LINK_CONNECTING, ssid, "--", WIFI_RSSI_DEFAULT);
    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "waiting for WiFi service");
    while (wifi_is_wifi_inited() == 0) {
        osal_msleep(100);
    }

    if (wifi_sta_enable() != ERRCODE_SUCC) {
        watch_model_set_wifi(WATCH_LINK_ERROR, ssid, "--", WIFI_RSSI_DEFAULT);
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "STA enable failed");
        return ERRCODE_FAIL;
    }
    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "STA enabled, connecting");
    osal_msleep(500);

    if (strncpy_s((char *)expected_bss.ssid, WIFI_MAX_SSID_LEN, ssid, WIFI_MAX_SSID_LEN - 1) != EOK) {
        return ERRCODE_FAIL;
    }
    if (strncpy_s((char *)expected_bss.pre_shared_key, WIFI_MAX_KEY_LEN, pwd, WIFI_MAX_KEY_LEN - 1) != EOK) {
        return ERRCODE_FAIL;
    }

    expected_bss.security_type = WIFI_SEC_TYPE_WPA2PSK;
    expected_bss.ip_type = DHCP;

    if (wifi_sta_connect(&expected_bss) != ERRCODE_SUCC) {
        watch_model_set_wifi(WATCH_LINK_ERROR, ssid, "--", WIFI_RSSI_DEFAULT);
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "connect request failed");
        return ERRCODE_FAIL;
    }

    netif_p = netifapi_netif_find("wlan0");
    if ((netif_p == NULL) || (netifapi_dhcp_start(netif_p) != ERR_OK)) {
        watch_model_set_wifi(WATCH_LINK_ERROR, ssid, "--", WIFI_RSSI_DEFAULT);
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "DHCP start failed");
        return ERRCODE_FAIL;
    }

    for (uint8_t i = 0; i < WIFI_STA_IP_MAX_GET_TIMES; i++) {
        osal_msleep(1000);
        if (netif_p->ip_addr.u_addr.ip4.addr != 0) {
            ip_to_text(netif_p->ip_addr.u_addr.ip4.addr, ip_text, sizeof(ip_text));
            watch_model_set_wifi(WATCH_LINK_CONNECTED, ssid, ip_text, WIFI_RSSI_DEFAULT);
            watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "connected and got IP");
            osal_printk("[WIFI] STA IP: %s\r\n", ip_text);
            return ERRCODE_SUCC;
        }
    }

    watch_model_set_wifi(WATCH_LINK_ERROR, ssid, "--", WIFI_RSSI_DEFAULT);
    watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "DHCP timeout");
    return ERRCODE_FAIL;
}

static errcode_t wifi_disconnect_start(void)
{
    watch_model_snapshot_t model;
    const char *ssid = "";

    watch_model_get_snapshot(&model);
    ssid = model.wifi_ssid;
    if (ssid[0] == '\0') {
        ssid = "--";
    }

    mqtt_task_stop();
    (void)wifi_sta_disconnect();
    wifi_provision_stop();
    (void)wifi_sta_disable();
    watch_model_set_wifi(WATCH_LINK_DISCONNECTED, ssid, "--", WIFI_RSSI_DEFAULT);
    watch_model_set_mqtt(WATCH_LINK_DISCONNECTED, model.mqtt_broker, model.mqtt_topic, "--");
    watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "disconnected");
    return ERRCODE_SUCC;
}

static void wifi_task_handle_connect(const wifi_profile_t *profile)
{
    if ((profile == NULL) || (profile->ssid[0] == '\0')) {
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "no profile saved");
        return;
    }

    (void)wifi_profile_save(profile->ssid, profile->pwd);
    mqtt_task_stop();
    osal_msleep(100);
    (void)wifi_sta_disconnect();
    wifi_provision_stop();
    if (wifi_connect_start(profile->ssid, profile->pwd) == ERRCODE_SUCC) {
        mqtt_task_start();
    }
}

static void *wifi_main_task(const char *arg)
{
    wifi_task_request_t req;
    unsigned int read_size;

    (void)arg;

    while (1) {
        read_size = sizeof(req);
        if (osal_msg_queue_read_copy(g_wifi_msg_queue, &req, &read_size, OSAL_WAIT_FOREVER) != 0) {
            continue;
        }
        if (read_size != sizeof(req)) {
            continue;
        }

        if (req.type == WIFI_REQ_CONNECT) {
            wifi_task_handle_connect(&req.profile);
        } else if (req.type == WIFI_REQ_DISCONNECT) {
            (void)wifi_disconnect_start();
        }
    }

    return NULL;
}

static errcode_t wifi_task_post(const wifi_task_request_t *req)
{
    if ((req == NULL) || (g_wifi_msg_queue == 0)) {
        return ERRCODE_FAIL;
    }

    if (osal_msg_queue_write_copy(g_wifi_msg_queue, (void *)req, sizeof(*req), 0) != 0) {
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "request queue full");
        return ERRCODE_FAIL;
    }
    return ERRCODE_SUCC;
}

errcode_t wifi_task_request_connect(const char *ssid, const char *pwd)
{
    wifi_task_request_t req = {0};

    if ((ssid == NULL) || (pwd == NULL) || (ssid[0] == '\0')) {
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "SSID empty");
        return ERRCODE_FAIL;
    }

    req.type = WIFI_REQ_CONNECT;
    if (strncpy_s(req.profile.ssid, sizeof(req.profile.ssid), ssid, sizeof(req.profile.ssid) - 1) != EOK) {
        return ERRCODE_FAIL;
    }
    if (strncpy_s(req.profile.pwd, sizeof(req.profile.pwd), pwd, sizeof(req.profile.pwd) - 1) != EOK) {
        return ERRCODE_FAIL;
    }
    watch_model_set_wifi(WATCH_LINK_CONNECTING, ssid, "--", WIFI_RSSI_DEFAULT);
    return wifi_task_post(&req);
}

errcode_t wifi_task_request_disconnect(void)
{
    wifi_task_request_t req = {0};

    req.type = WIFI_REQ_DISCONNECT;
    return wifi_task_post(&req);
}

void wifi_task_start(const char *default_ssid, const char *default_pwd)
{
    osal_task *task_handle = NULL;
    wifi_profile_t profile = {0};

    if (g_wifi_task_started != 0) {
        return;
    }

    if (g_wifi_msg_queue == 0) {
        (void)osal_msg_queue_create("wifi_ctrl", WIFI_TASK_QUEUE_LEN, &g_wifi_msg_queue, 0,
                                    sizeof(wifi_task_request_t));
        if (g_wifi_msg_queue == 0) {
            watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "queue create failed");
            return;
        }
    }

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)wifi_main_task, 0, "WatchWifiTask",
                                      WIFI_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, WIFI_TASK_PRIORITY);
        osal_kfree(task_handle);
        g_wifi_task_started = 1;
    }
    osal_kthread_unlock();

    if (g_wifi_task_started == 0) {
        watch_model_add_log(WATCH_LOG_WARNING, "WiFi", "task create failed");
        return;
    }

    if (wifi_task_get_saved_profile(&profile)) {
        (void)wifi_task_request_connect(profile.ssid, profile.pwd);
    } else if ((default_ssid != NULL) && (default_pwd != NULL) && (default_ssid[0] != '\0')) {
        (void)wifi_task_request_connect(default_ssid, default_pwd);
    } else {
        watch_model_add_log(WATCH_LOG_WIFI, "WiFi", "profile empty");
    }
}
