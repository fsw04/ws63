/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025-2025. All rights reserved. \n
 *
 * Description: Application core main function for radar socket \n
 * Author:  \n
 */

#include <stdlib.h>
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "td_base.h"
#include "td_type.h"
#include "uart.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "soc_osal.h"
#include "at_product.h"
#include "at_config.h"


#define RADAR_SOCKET_LOG                "[RADAR_UDP_SOCKET]"
#define RADAR_SOCKET_TASK_PRIO          23
#define RADAR_SOCKET_TASK_STACK_SIZE    0x1000

#define SOCK_TARGET_PORT                9998
#define OFFSET_TWENTY_FOUR              24
#define OFFSET_EIGHT                    8

#define TLV_HEAD_PART_LEN               1
#define TLV_TAG_PART_LEN                1
#define TLV_LEN_PART_LEN                2

#define TLV_MIN_LEN                     3 // Bytes
#define TLV_MAX_LEN                     100 // Bytes
#define TLV_HEADER                      0xAA

#define IP_LEN                          4
#define TLV_IP_LEN                      (IP_LEN + TLV_HEAD_PART_LEN + TLV_TAG_PART_LEN + TLV_LEN_PART_LEN)

#define TLV_IP_ADDR                     0xA2

#define TLV_LOCAL_IP                    0xC1

#define RADAR_SOCKET_BUFFER_SIZE        1024  // 字节
#define RADAR_MSG_TRANS_LEN_OFFSET      8

#define DELAY_200                       200
#define DELAY_1000                      1000

typedef enum {
    NUM_0,
    NUM_1,
    NUM_2,
    NUM_3,
    NUM_4,
    NUM_5
} NumEnum;

static int32_t g_sk = -1; // socket创建套接字
static in_addr_t g_client_ip_addr = 0;

static uint32_t radar_htonl(int16_t n)
{
    return ((uint32_t)((((n) & 0xff) << OFFSET_TWENTY_FOUR) | (((n) & 0xff00) << OFFSET_EIGHT) | \
            (((n) >> OFFSET_EIGHT)  & 0xff00) | (((n) >> OFFSET_TWENTY_FOUR) & 0xff)));
}

static uint16_t radar_htons(int16_t n)
{
    return ((uint16_t)((((n) & 0xff) << OFFSET_EIGHT) | (((n) >> OFFSET_EIGHT) & 0xff)));
}

static void udp_unpack_tlv(uint8_t *buf, int32_t bytes)
{
    if ((buf == NULL) || (bytes < TLV_MIN_LEN)) {
        return;
    }

    for (int32_t i = 0; i < bytes; i++) {
        if (buf[i] == TLV_HEADER) {
            if (i + TLV_TAG_PART_LEN + TLV_LEN_PART_LEN >= bytes) {
                continue;
            }
            switch (buf[i + 1]) {
                case TLV_IP_ADDR: {
                    if (i + TLV_IP_LEN - 1  > bytes) {
                        continue;
                    }

                    g_client_ip_addr = *(in_addr_t *)&buf[i + TLV_TAG_PART_LEN + TLV_LEN_PART_LEN ];

                    struct in_addr ip;
                    ip.s_addr = g_client_ip_addr;
                    char ip_str[INET_ADDRSTRLEN];
                    if (inet_ntop(AF_INET, &ip, ip_str, sizeof(ip_str)) != NULL) {
                        osal_printk("%sReceived IP: %s\n", RADAR_SOCKET_LOG, ip_str);
                    }
                    continue;
                }
                default:
                    continue;
            }
        }
    }
}

static int get_local_ip_from_netif(char *str_ip)
{
    struct netif *netif;
    ip4_addr_t *ip;

    for (netif = netif_list; netif != NULL; netif = netif->next) {
        if (netif_is_up(netif)) {
            ip = ip_2_ip4(&netif->ip_addr);
            if (ip != NULL) {
                inet_ntoa_r(*ip, str_ip, INET_ADDRSTRLEN);
                return 0;
            }
        }
    }

    return -1;
}

static int32_t radar_udp_socket_recv_task_body(void *param)
{
    UNUSED(param);
    
    for (;;) {
        struct sockaddr_in client_addr = {0};
        socklen_t client_addr_len = sizeof(client_addr);
        uint8_t recv_buf[TLV_MAX_LEN];
        int32_t recv_bytes = recvfrom(g_sk, recv_buf, sizeof(recv_buf), 0,
                                      (struct sockaddr *)&client_addr, &client_addr_len);
        if (recv_bytes < 0) {
            (void)osDelay(DELAY_200);
            continue;
        }

        osal_printk("%srecv_bytes:%d\r\n", RADAR_SOCKET_LOG, recv_bytes);
        udp_unpack_tlv(recv_buf, recv_bytes);
    }

    return 0;
}

int ip_str_to_hex(const char *ip_str, uint8_t *ip_hex)
{
    struct in_addr ip;
    if (inet_aton(ip_str, &ip) == 0) {
        return -1;
    }

    memcpy_s(ip_hex, IP_LEN, &ip, IP_LEN);
    return 0;
}

void build_tlv_packet(const uint8_t *ip_hex, uint8_t *tlv_packet)
{
    tlv_packet[NUM_0] = TLV_HEADER;
    tlv_packet[NUM_1] = TLV_LOCAL_IP;
    tlv_packet[NUM_2] = (uint8_t)(IP_LEN & 0xFF);
    tlv_packet[NUM_3] = (uint8_t)((IP_LEN >> RADAR_MSG_TRANS_LEN_OFFSET) & 0xFF);
    memcpy_s(&tlv_packet[NUM_4], IP_LEN, ip_hex, IP_LEN);
}

static int32_t radar_udp_socket_send_task_body(void *param)
{
    UNUSED(param);

    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len = sizeof(client_addr);

    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(SOCK_TARGET_PORT);

    while (true) {
        while (g_client_ip_addr == 0) {
            osal_printk("%swaiting client ip address", RADAR_SOCKET_LOG);
            (void)osDelay(DELAY_200);
        }

        client_addr.sin_addr.s_addr = g_client_ip_addr;

        char local_ip_str[INET_ADDRSTRLEN] = {0};

        if (get_local_ip_from_netif(local_ip_str) == 0) {
            osal_printk("%sLocal IP: %s", local_ip_str, RADAR_SOCKET_LOG);
        }

        uint8_t ip_hex[IP_LEN] = {0};
        ip_str_to_hex(local_ip_str, ip_hex);

        uint8_t tlv_packet[TLV_IP_LEN] = {0};
        build_tlv_packet(ip_hex, tlv_packet);

        for (;;) {
            int32_t res = sendto(g_sk, tlv_packet, sizeof(tlv_packet), 0, (struct sockaddr *)&client_addr, client_addr_len);
            if (res < 0) {
                g_client_ip_addr = 0;
                break;
            }
            (void)osDelay(DELAY_1000);
        }
    }
    return 0;
}

void radar_udp_socket_recv_task(void)
{
    osThreadAttr_t attr;
    attr.name       = "radar_udp_socket_recv";
    attr.attr_bits  = 0U;
    attr.cb_mem     = NULL;
    attr.cb_size    = 0U;
    attr.stack_mem  = NULL;
    attr.stack_size = RADAR_SOCKET_TASK_STACK_SIZE;
    attr.priority   = RADAR_SOCKET_TASK_PRIO;
    (void)osThreadNew((osThreadFunc_t)radar_udp_socket_recv_task_body, NULL, &attr);
}

void radar_udp_socket_send_task(void)
{
    osThreadAttr_t attr;
    attr.name       = "radar_udp_socket_send";
    attr.attr_bits  = 0U;
    attr.cb_mem     = NULL;
    attr.cb_size    = 0U;
    attr.stack_mem  = NULL;
    attr.stack_size = RADAR_SOCKET_TASK_STACK_SIZE;
    attr.priority   = RADAR_SOCKET_TASK_PRIO;
    (void)osThreadNew((osThreadFunc_t)radar_udp_socket_send_task_body, NULL, &attr);
}

int32_t radar_udp_socket_server(void)
{
    int32_t ret;
    uint32_t broadcast = 1;
    struct sockaddr_in server_addr = {0};
    uint32_t opt = 1;
    uint32_t buffer = RADAR_SOCKET_BUFFER_SIZE;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = radar_htons(SOCK_TARGET_PORT);
    server_addr.sin_addr.s_addr = radar_htonl(INADDR_ANY); // 0.0.0.0

    g_sk = socket(AF_INET, SOCK_DGRAM, 0); // 使用 UDP
    if (g_sk < 0) {
        return -1;
    }

    int flags = lwip_fcntl(g_sk, F_GETFL, 0);
    if (flags < 0 || lwip_fcntl(g_sk, F_SETFL, flags | O_NONBLOCK) < 0) {
        lwip_close(g_sk);
        return -1;
    }

    setsockopt(g_sk, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    setsockopt(g_sk, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(g_sk, SOL_SOCKET, SO_SNDBUF, &buffer, sizeof(buffer));
    setsockopt(g_sk, SOL_SOCKET, SO_RCVBUF, &buffer, sizeof(buffer));

    ret = bind(g_sk, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        int err = errno;
        char err_str[256];
        strerror_r(err, err_str, sizeof(err_str));
        osal_printk("%sbind failed, error: %s\r\n", RADAR_SOCKET_LOG, err_str);
        lwip_close(g_sk);
        return -1;
    }

    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    if (getsockname(g_sk, (struct sockaddr *)&local_addr, &len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, sizeof(ip_str))) {
            osal_printk("%sSocket bound to %s:%d\n", RADAR_SOCKET_LOG, ip_str, ntohs(local_addr.sin_port));
        }
    }

    radar_udp_socket_recv_task();
    radar_udp_socket_send_task();

    return 0;
}