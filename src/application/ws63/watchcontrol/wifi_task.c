#include "wifi_task.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "lwip/netifapi.h"
#include "soc_osal.h"
#include "string.h"

#define WIFI_STA_IP_MAX_GET_TIMES 15

errcode_t wifi_connect_start(const char *ssid, const char *pwd)
{
    wifi_sta_config_stru expected_bss = {0};
    struct netif *netif_p = NULL;
    
    // 1. 等待底层 Wi-Fi 初始化完成
    while (wifi_is_wifi_inited() == 0) {
        osal_msleep(100);
    }
    
    // 2. 使能 STA 模式
    if (wifi_sta_enable() != ERRCODE_SUCC) {
        osal_printk("[WIFI] STA enable fail!\r\n");
        return ERRCODE_FAIL;
    }
    osal_printk("[WIFI] STA enable success. Start connecting...\r\n");
    osal_msleep(500);

    // 3. 配置目标路由器的 SSID 和 密码
    if (memcpy_s(expected_bss.ssid, WIFI_MAX_SSID_LEN, ssid, strlen(ssid)) != EOK) return ERRCODE_FAIL;
    if (memcpy_s(expected_bss.pre_shared_key, WIFI_MAX_KEY_LEN, pwd, strlen(pwd)) != EOK) return ERRCODE_FAIL;
    
    expected_bss.security_type = WIFI_SEC_TYPE_WPA2PSK; // 根据实际热点修改安全类型
    expected_bss.ip_type = DHCP;
    
    // 4. 发起连接请求
    if (wifi_sta_connect(&expected_bss) != ERRCODE_SUCC) {
        osal_printk("[WIFI] STA connect request fail!\r\n");
        return ERRCODE_FAIL;
    }

    // 5. 启动 DHCP 客户端并等待分配 IP
    netif_p = netifapi_netif_find("wlan0");
    if (netif_p == NULL || netifapi_dhcp_start(netif_p) != ERR_OK) {
        return ERRCODE_FAIL;
    }

    for (uint8_t i = 0; i < WIFI_STA_IP_MAX_GET_TIMES; i++) {
        osal_msleep(1000);
        if (netif_p->ip_addr.u_addr.ip4.addr != 0) {
            osal_printk("[WIFI] STA IP: %u.%u.%u.%u\r\n", 
                (netif_p->ip_addr.u_addr.ip4.addr & 0x000000ff),
                (netif_p->ip_addr.u_addr.ip4.addr & 0x0000ff00) >> 8,
                (netif_p->ip_addr.u_addr.ip4.addr & 0x00ff0000) >> 16,
                (netif_p->ip_addr.u_addr.ip4.addr & 0xff000000) >> 24);
            return ERRCODE_SUCC;
        }
    }
    
    osal_printk("[WIFI] STA get IP timeout!\r\n");
    return ERRCODE_FAIL;
}