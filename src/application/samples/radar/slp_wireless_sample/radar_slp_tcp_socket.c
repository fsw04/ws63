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
#include "radar_service.h"
#include "at_product.h"
#include "at_config.h"

#define RADAR_SOCKET_SEND_RES           (1U << 0)
#define RADAR_SOCKET_SEND_RAW           (1U << 1)
#define RADAR_SOCKET_SEND_DBG           (1U << 2)
#define RADAR_SOCKET_EVENT_MASK (RADAR_SOCKET_SEND_RES | RADAR_SOCKET_SEND_RAW | RADAR_SOCKET_SEND_DBG)

#define RADAR_SOCKET_LOG                "[RADAR_TCP_SOCKET]"
#define RADAR_SOCKET_TASK_PRIO          27
#define RADAR_SOCKET_TASK_STACK_SIZE    0x1000

#define SOCK_TARGET_PORT                9999
#define IP_TCP_SERVER_LISTEN_NUM        4
#define OFFSET_TWENTY_FOUR              24
#define OFFSET_EIGHT                    8

#define TLV_MIN_LEN                     3 // Bytes
#define TLV_RECV_MAX_LEN                400 // Bytes
#define TLV_HEADER                      0xAA

#define TLV_AT_CMD                      0xA1

#define TLV_CMD_GET_RES                 0xB1
#define TLV_CMD_GET_RAW                 0xB2
#define TLV_CMD_GET_DBG                 0xB3

#define BUF_TWO                         2
#define BUF_THREE                       3
#define RECV_DELAY                      200     // 等待2s

#define RADAR_RET_SUCC                  0
#define RADAR_RET_FAIL                  (-1)

#define RADAR_SOCKET_SEND_BUFFER_SIZE   (100 * 1024)   // 100K
#define RADAR_SOCKET_RECV_BUFFER_SIZE   1024           // 1k

#define SOCKET_RES_LEN                  40
#define SOCKET_SUB_FRAME_LEN            16
#define SOCKET_DBG_LEN                  300
#define UART_HEAD_LEN                   4
#define UART_TAIL_LEN                   2
#define RADAR_MSG_TRANS_LEN_IDX2        2
#define RADAR_MSG_TRANS_LEN_IDX3        3
#define RADAR_MSG_TRANS_LEN_OFFSET      8

#define SEND_QUEUE_SIZE                 100

typedef enum {
    SEND_TYPE_RES,
    SEND_TYPE_RAW,
    SEND_TYPE_DBG
} send_data_type_t;

typedef struct {
    send_data_type_t type;
    uint8_t *data;
    uint32_t len;
} send_queue_item_t;

typedef struct {
    send_queue_item_t items[SEND_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    osMutexId_t mutex;
    osSemaphoreId_t not_empty;
} send_queue_t;

typedef struct {
    uint8_t header[UART_HEAD_LEN];
    complex_short_t data[CONFIG_RADAR_ANT_PAIR_NUM][CONFIG_PC_OUT_NUM];
    uint8_t tail[UART_TAIL_LEN];
} radar_slp_subframe_raw_data_t;

typedef struct {
    uint8_t header;
    uint8_t tag;
    uint16_t len;
    radar_slp_subframe_raw_data_t socket_raw_data[SOCKET_SUB_FRAME_LEN];
} radar_slp_tlv_raw_data_t;

typedef struct {
    uint8_t header;
    uint8_t tag;
    uint16_t len;
    uint8_t socket_dbg_data[SOCKET_DBG_LEN];
} radar_slp_tlv_dbg_data_t;

typedef struct {
    uint8_t header;
    uint8_t tag;
    uint16_t len;
    uint8_t socket_res_data[SOCKET_RES_LEN];
} radar_slp_tlv_res_data_t;

typedef struct {
    uint16_t id;
    int16_t rng;
    int16_t agl;
    int16_t vel;
} __attribute__((packed)) radar_slp_res_data_t;

static uint16_t g_res_len = 0;
static uint16_t g_dbg_len = 0;
static int32_t g_client = -1;
static int32_t g_sk = -1; // socket创建套接字

static send_queue_t g_send_queue;
static radar_slp_res_data_t g_radar_slp_res_data[RADAR_MAX_RPT_TARGET_NUM] = {0};
static uint8_t g_socket_save_idx = 0;
static radar_slp_tlv_res_data_t g_radar_slp_tlv_res_data = {0};
static radar_slp_tlv_raw_data_t g_radar_slp_tlv_raw_data = {0};
static radar_slp_tlv_dbg_data_t g_radar_slp_tlv_dbg_data = {0};
static osal_event g_radar_socket_events;

static void radar_init_socket_event(void)
{
    memset_s(&g_radar_socket_events, sizeof(osal_event), 0, sizeof(osal_event));
    osal_event_init(&g_radar_socket_events);
    g_socket_save_idx = 0;
}

static void send_queue_init(void)
{
    g_send_queue.head = 0;
    g_send_queue.tail = 0;
    g_send_queue.mutex = osMutexNew(NULL);
    g_send_queue.not_empty = osSemaphoreNew(0, 0, NULL);
}

static int32_t send_queue_push(send_queue_item_t *item)
{
    if (g_send_queue.mutex == NULL || g_send_queue.not_empty == NULL) {
        return -1;
    }

    osMutexAcquire(g_send_queue.mutex, osWaitForever);

    if ((g_send_queue.tail + 1) % SEND_QUEUE_SIZE == g_send_queue.head) {
        osMutexRelease(g_send_queue.mutex);
        return -1; // 队列满
    }

    g_send_queue.items[g_send_queue.tail] = *item;
    g_send_queue.tail = (g_send_queue.tail + 1) % SEND_QUEUE_SIZE;
    osSemaphoreRelease(g_send_queue.not_empty);

    osMutexRelease(g_send_queue.mutex);
    return 0;
}

static int32_t send_queue_pop(send_queue_item_t *item)
{
    if (g_send_queue.mutex == NULL || g_send_queue.not_empty == NULL) {
        return -1;
    }

    if (g_send_queue.head == g_send_queue.tail) {
        return -1; // 队列空
    }

    osMutexAcquire(g_send_queue.mutex, osWaitForever);

    *item = g_send_queue.items[g_send_queue.head];
    g_send_queue.head = (g_send_queue.head + 1) % SEND_QUEUE_SIZE;

    osMutexRelease(g_send_queue.mutex);
    return 0;
}

int16_t radar_send_socket_event(uint32_t event)
{
    int16_t ret = osal_event_write(&g_radar_socket_events, event);
    if (ret == 0) {
        return RADAR_RET_SUCC;
    }
    return RADAR_RET_FAIL;
}

static int16_t radar_recv_socket_event(uint32_t *event_bits)
{
    int16_t ret = osal_event_read(&g_radar_socket_events, RADAR_SOCKET_EVENT_MASK, OSAL_EVENT_FOREVER,
        OSAL_WAITMODE_OR | OSAL_WAITMODE_CLR);
    if (ret == -1) {
        *event_bits = 0;
        return RADAR_RET_FAIL;
    }
    *event_bits = (uint32_t)ret;
    return RADAR_RET_SUCC;
}

static void slp_radar_print_set_power_sts_cb_res(uint8_t status, errcode_radar_client_t err_code)
{
    if (err_code == ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP_RADAR][SAMPLE] Radar set power status %u succ.\r\n", status);
    } else {
        osal_printk("[SLP_RADAR][SAMPLE] Radar set power status %u fail, errcode:0x%x.\r\n", status, err_code);
    }
}

void slp_radar_send_res(const radar_result_t *result)
{
    size_t trans_len = sizeof(g_radar_slp_res_data);
    for (uint8_t i = 0; i < RADAR_MAX_RPT_TARGET_NUM; i++) {
        g_radar_slp_res_data[i].id = (uint8_t)(result->target_info[i].id);
        g_radar_slp_res_data[i].rng = result->target_info[i].rng;
        g_radar_slp_res_data[i].agl = result->target_info[i].agl;
        g_radar_slp_res_data[i].vel = result->target_info[i].vel;
    }
    uint8_t header[UART_HEAD_LEN] = {'r', 'd', 0, 0};
    uint8_t tail[UART_TAIL_LEN] = {'n', 'd'};
    header[RADAR_MSG_TRANS_LEN_IDX2] = (uint8_t)(trans_len & 0xFF);
    header[RADAR_MSG_TRANS_LEN_IDX3] = (uint8_t)((trans_len >> RADAR_MSG_TRANS_LEN_OFFSET) & 0xFF);

    uint8_t *addr = (uint8_t*)&g_radar_slp_tlv_res_data.socket_res_data;
    g_res_len = trans_len + sizeof(header) + sizeof(tail);
    (void)memcpy_s(addr, UART_HEAD_LEN, header, UART_HEAD_LEN);
    (void)memcpy_s(addr + UART_HEAD_LEN, trans_len, g_radar_slp_res_data, trans_len);
    (void)memcpy_s(addr + UART_HEAD_LEN + trans_len, UART_TAIL_LEN, tail, UART_TAIL_LEN);
    
    radar_send_socket_event(RADAR_SOCKET_SEND_RES);
}

void slp_radar_send_res_nb(const radar_slp_res_data_t *result)
{
    size_t trans_len = sizeof(g_radar_slp_res_data);

    uint8_t header[UART_HEAD_LEN] = {'r', 'd', 0, 0};
    uint8_t tail[UART_TAIL_LEN] = {'n', 'd'};
    header[RADAR_MSG_TRANS_LEN_IDX2] = (uint8_t)(trans_len & 0xFF);
    header[RADAR_MSG_TRANS_LEN_IDX3] = (uint8_t)((trans_len >> RADAR_MSG_TRANS_LEN_OFFSET) & 0xFF);

    uint8_t *addr = (uint8_t*)&g_radar_slp_tlv_res_data.socket_res_data;
    g_res_len = trans_len + sizeof(header) + sizeof(tail);
    (void)memcpy_s(addr, UART_HEAD_LEN, header, UART_HEAD_LEN);
    (void)memcpy_s(addr + UART_HEAD_LEN, trans_len, result, trans_len);
    (void)memcpy_s(addr + UART_HEAD_LEN + trans_len, UART_TAIL_LEN, tail, UART_TAIL_LEN);
    
    radar_send_socket_event(RADAR_SOCKET_SEND_RES);
}

void slp_radar_send_raw(radar_raw_data_msg_t *dataMsg)
{
    uint16_t trans_len = dataMsg->data_len * sizeof(complex_short_t);
    uint8_t header[UART_HEAD_LEN] = {'p', 'd', 0, 0};
    uint8_t tail[UART_TAIL_LEN] = {'n', 'd'};
    header[RADAR_MSG_TRANS_LEN_IDX2] = (uint8_t)(trans_len & 0xFF);
    header[RADAR_MSG_TRANS_LEN_IDX3] = (uint8_t)((trans_len >> RADAR_MSG_TRANS_LEN_OFFSET) & 0xFF);

    uint8_t *addr = (uint8_t*)&g_radar_slp_tlv_raw_data.socket_raw_data[g_socket_save_idx];
    (void)memcpy_s(addr, UART_HEAD_LEN, header, UART_HEAD_LEN);
    (void)memcpy_s(addr + UART_HEAD_LEN, trans_len, dataMsg->data, trans_len);
    (void)memcpy_s(addr + UART_HEAD_LEN + trans_len, UART_TAIL_LEN, tail, UART_TAIL_LEN);

    g_socket_save_idx++;
    if (g_socket_save_idx == SOCKET_SUB_FRAME_LEN) {
        g_socket_save_idx = 0;
        radar_send_socket_event(RADAR_SOCKET_SEND_RAW);
    }
}

void slp_radar_send_dbg(uint8_t *data, uint32_t dbg_data_len)
{
    uint16_t trans_len = dbg_data_len;
    uint8_t header[UART_HEAD_LEN] = {'d', 'd', 0, 0};
    uint8_t tail[UART_TAIL_LEN] = {'n', 'd'};
    header[RADAR_MSG_TRANS_LEN_IDX2] = (uint8_t)(trans_len & 0xFF);
    header[RADAR_MSG_TRANS_LEN_IDX3] = (uint8_t)((trans_len >> RADAR_MSG_TRANS_LEN_OFFSET) & 0xFF);

    uint8_t *addr = (uint8_t*)&g_radar_slp_tlv_dbg_data.socket_dbg_data;
    g_dbg_len = trans_len + sizeof(header) + sizeof(tail);
    (void)memcpy_s(addr, UART_HEAD_LEN, header, UART_HEAD_LEN);
    (void)memcpy_s(addr + UART_HEAD_LEN, trans_len, data, trans_len);
    (void)memcpy_s(addr + UART_HEAD_LEN + trans_len, UART_TAIL_LEN, tail, UART_TAIL_LEN);

    radar_send_socket_event(RADAR_SOCKET_SEND_DBG);
}

static uint32_t radar_htonl(int16_t n)
{
    return ((uint32_t)((((n) & 0xff) << OFFSET_TWENTY_FOUR) | (((n) & 0xff00) << OFFSET_EIGHT) | \
            (((n) >> OFFSET_EIGHT)  & 0xff00) | (((n) >> OFFSET_TWENTY_FOUR) & 0xff)));
}

static uint16_t radar_htons(int16_t n)
{
    return ((uint16_t)((((n) & 0xff) << OFFSET_EIGHT) | (((n) >> OFFSET_EIGHT) & 0xff)));
}

static void unpack_tlv(uint8_t *buf, int32_t bytes)
{
    if ((buf == NULL) || (bytes < TLV_MIN_LEN)) {
        return;
    }

    int32_t i = 0;
    while (i < bytes) {
        if (buf[i] == TLV_HEADER) {
            switch (buf[i + 1]) {
                case TLV_AT_CMD: {
                    uint8_t cmd_len = buf[i + BUF_TWO];
                    uapi_at_channel_data_recv(AT_UART_PORT, &buf[i + BUF_THREE], (uint32_t)cmd_len);
                    i += cmd_len + BUF_THREE;   // 跳过已经读取的AT命令数据
                    continue;
                }
                default:
                    i++;
                    continue;
            }
        }
        i++;
    }
}

static int32_t recv_tlv(int32_t sk)
{
    uint8_t recv_buf[TLV_RECV_MAX_LEN];

    int32_t recv_bytes = recv(sk, &recv_buf, sizeof(uint8_t) * TLV_RECV_MAX_LEN, 0);
    if (recv_bytes <= 0) {
        if (errno == EAGAIN) {  // 非阻塞式socket，socket没有数据可读时，返回-1，错误码为EAGAIN，此时应该继续等待，否则关闭socket
            return 0;
        } else {
            return -1;
        }
    }
    osal_printk("%srecv_bytes:%d\r\n", RADAR_SOCKET_LOG, recv_bytes);
    unpack_tlv(recv_buf, recv_bytes);

    return 0;
}

static int32_t enqueue_data(uint8_t *src_data, uint16_t send_len, send_data_type_t type)
{
    uint8_t *data = (uint8_t *)malloc(send_len);
    if (data == NULL) {
        osal_printk("%s::malloc failed for data, type: %d\r\n", RADAR_SOCKET_LOG, type);
        return -1;
    }

    (void)memcpy_s(data, send_len, src_data, send_len);

    send_queue_item_t item = {0};
    item.type = type;
    item.data = data;
    item.len = send_len;
    send_queue_push(&item);

    return 0;
}

static int32_t radar_tcp_socket_recv_task_body(void *param)
{
    UNUSED(param);
    int32_t recv_data = 0;
    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len;

    for (;;) {
        g_client = accept(g_sk, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
        if (g_client < 0) {
            osal_printk("%s::radar socket accept error.\r\n", RADAR_SOCKET_LOG);
            (void)osDelay(RECV_DELAY);
            continue;
        }

        for (;;) {
            recv_data = recv_tlv(g_client);
            if (recv_data < 0) {
                lwip_close(g_client);
                break;
            }
        }
    }

    return 0;
}

static int32_t radar_tcp_socket_send_task_body(void *param)
{
    UNUSED(param);
    send_queue_item_t item;
    for (;;) {
        if (osSemaphoreAcquire(g_send_queue.not_empty, osWaitForever) == osOK) {
            if (send_queue_pop(&item) == 0) {
                if (g_client >= 0) {
                    int32_t res = send(g_client, item.data, item.len, 0);
                    if (res < 0) {
                        osal_printk("%s::send failed, type: %d, len: %d\r\n", RADAR_SOCKET_LOG, item.type, item.len);
                    }
                }

                if (item.data != NULL) {
                    free(item.data);
                    item.data = NULL;
                }
            }
        }
    }

    return 0;
}

static int32_t radar_tcp_socket_enqueue_task_body(void *param)
{
    UNUSED(param);
    uint32_t event_bits = 0;

    for (;;) {
        int16_t ret = radar_recv_socket_event(&event_bits);
        if ((ret == -1) || (event_bits == 0)) {
            continue;
        }

        if (event_bits & RADAR_SOCKET_SEND_RES) {
            g_radar_slp_tlv_res_data.header = TLV_HEADER;
            g_radar_slp_tlv_res_data.tag = TLV_CMD_GET_RES;
            g_radar_slp_tlv_res_data.len = g_res_len;
            uint16_t send_len = g_res_len + sizeof(g_radar_slp_tlv_res_data.header) + \
                                sizeof(g_radar_slp_tlv_res_data.tag) + sizeof(g_radar_slp_tlv_res_data.len);
            enqueue_data((uint8_t *)&g_radar_slp_tlv_res_data, send_len, SEND_TYPE_RES);
        }

        if (event_bits & RADAR_SOCKET_SEND_RAW) {
            g_radar_slp_tlv_raw_data.header = TLV_HEADER;
            g_radar_slp_tlv_raw_data.tag = TLV_CMD_GET_RAW;
            g_radar_slp_tlv_raw_data.len = sizeof(radar_slp_subframe_raw_data_t) * SOCKET_SUB_FRAME_LEN;
            uint16_t send_len = sizeof(radar_slp_tlv_raw_data_t);
            enqueue_data((uint8_t *)&g_radar_slp_tlv_raw_data, send_len, SEND_TYPE_RAW);
        }

        if (event_bits & RADAR_SOCKET_SEND_DBG) {
            g_radar_slp_tlv_dbg_data.header = TLV_HEADER;
            g_radar_slp_tlv_dbg_data.tag = TLV_CMD_GET_DBG;
            g_radar_slp_tlv_dbg_data.len = g_dbg_len;
            uint16_t send_len = g_dbg_len + sizeof(g_radar_slp_tlv_dbg_data.header) + \
                                sizeof(g_radar_slp_tlv_dbg_data.tag) + sizeof(g_radar_slp_tlv_dbg_data.len);
            enqueue_data((uint8_t *)&g_radar_slp_tlv_dbg_data, send_len, SEND_TYPE_DBG);
        }

        event_bits = 0;
    }

    return 0;
}

void radar_tcp_socket_recv_task(void)
{
    osThreadAttr_t attr;
    attr.name       = "radar_tcp_socket_recv";
    attr.attr_bits  = 0U;
    attr.cb_mem     = NULL;
    attr.cb_size    = 0U;
    attr.stack_mem  = NULL;
    attr.stack_size = RADAR_SOCKET_TASK_STACK_SIZE;
    attr.priority   = RADAR_SOCKET_TASK_PRIO;
    (void)osThreadNew((osThreadFunc_t)radar_tcp_socket_recv_task_body, NULL, &attr);
}

void radar_tcp_socket_enqueue_task(void)
{
    osThreadAttr_t attr;
    attr.name       = "radar_tcp_socket_enqueue";
    attr.attr_bits  = 0U;
    attr.cb_mem     = NULL;
    attr.cb_size    = 0U;
    attr.stack_mem  = NULL;
    attr.stack_size = RADAR_SOCKET_TASK_STACK_SIZE;
    attr.priority   = RADAR_SOCKET_TASK_PRIO;
    (void)osThreadNew((osThreadFunc_t)radar_tcp_socket_enqueue_task_body, NULL, &attr);
}

void radar_tcp_socket_send_task(void)
{
    osThreadAttr_t attr;
    attr.name       = "radar_tcp_socket_send";
    attr.attr_bits  = 0U;
    attr.cb_mem     = NULL;
    attr.cb_size    = 0U;
    attr.stack_mem  = NULL;
    attr.stack_size = RADAR_SOCKET_TASK_STACK_SIZE;
    attr.priority   = RADAR_SOCKET_TASK_PRIO;
    (void)osThreadNew((osThreadFunc_t)radar_tcp_socket_send_task_body, NULL, &attr);
}

int32_t radar_socket_server(void)
{
    int32_t ret;
    struct sockaddr_in server_addr = {0};
    uint32_t opt = 1;
    uint32_t send_buffer = RADAR_SOCKET_SEND_BUFFER_SIZE;
    uint32_t recv_buffer = RADAR_SOCKET_RECV_BUFFER_SIZE;

    uapi_radar_register_set_power_status_cb(slp_radar_print_set_power_sts_cb_res);
    uapi_radar_register_result_cb(slp_radar_send_res);
    uapi_radar_register_raw_data_cb(slp_radar_send_raw);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = radar_htons(SOCK_TARGET_PORT);
    server_addr.sin_addr.s_addr = radar_htonl(INADDR_ANY); // 0.0.0.0

    g_sk = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sk < 0) {
        return -1;
    }

    int flags = lwip_fcntl(g_sk, F_GETFL, 0);
    if (flags < 0 || lwip_fcntl(g_sk, F_SETFL, flags | O_NONBLOCK) < 0) {
        lwip_close(g_sk);
        return -1;
    }

    setsockopt(g_sk, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(g_sk, SOL_SOCKET, SO_SNDBUF, &send_buffer, sizeof(send_buffer));
    setsockopt(g_sk, SOL_SOCKET, SO_RCVBUF, &recv_buffer, sizeof(recv_buffer));
    setsockopt(g_sk, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    ret = bind(g_sk, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        lwip_close(g_sk);
        return -1;
    }

    ret = listen(g_sk, IP_TCP_SERVER_LISTEN_NUM);
    if (ret < 0) {
        lwip_close(g_sk);
        return -1;
    }

    send_queue_init();
    radar_init_socket_event();
    radar_tcp_socket_recv_task();
    radar_tcp_socket_enqueue_task();
    radar_tcp_socket_send_task();

    return 0;
}