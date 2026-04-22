/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2024-2025. All rights reserved.
 *
 * Description: Radar SLP samples function \n
 *
 */

#include "stdlib.h"
#include "uart.h"
#include "td_base.h"
#include "td_type.h"
#include "cmsis_os2.h"
#include "soc_osal.h"
#include "app_init.h"
#include "osal_task.h"
#include "securec.h"
#include "radar_service.h"

#define RADAR_STATUS_QUERY_DELAY 1000 // 10s
#define RADAR_DEFAULT_RPT_MODE  0
#define RADAR_DEFAULT_SENSITIVITY   0
#define RADAR_DEFAULT_RNG_BOUNDARY  700
#define RADAR_DEFAULT_DLY_TIME  20
#define RADAR_POWER_ON_DELAY    150 // 1.5s
#define RADAR_DEFAULT_STA_CREATE_RNG_THRES 0 // 0m

// 雷达结果回调函数实现样例
static void radar_print_res(const radar_result_t *result)
{
    for (uint32_t i = 0; i < RADAR_MAX_RPT_TARGET_NUM; i++) {
        osal_printk("[SLP_RADAR][SAMPLE] %u, %d, %d, %d\r\n", result->target_info[i].id, result->target_info[i].rng,
            result->target_info[i].agl, result->target_info[i].vel);
    }
}

// 雷达设置状态回调函数实现样例
static void radar_print_set_power_sts_cb_res(uint8_t status, errcode_radar_client_t err_code)
{
    if (err_code == ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP_RADAR][SAMPLE] slp set power status %u succ.\r\n", status);
    } else {
        osal_printk("[SLP_RADAR][SAMPLE] slp set power status %u fail, errcode:0x%x.\r\n", status, err_code);
    }
}

// 雷达错误码回调函数实现样例
static void radar_print_rpt_errcode_cb_res(errcode_radar_client_t err_code)
{
    if (err_code != ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP_RADAR][SAMPLE] radar report errcode: 0x%x.\r\n", err_code);
    }
}

// 注册雷达相关回调函数
static void register_radar_cb(void)
{
    errcode_radar_client_t ret;
    // 注册雷达结果回调函数
    ret = uapi_radar_register_result_cb(radar_print_res);
    if (ret != ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP RADAR][SAMPLE]uapi_radar_register_result_cb failed, ret: 0x%x.\r\n", ret);
    }
    // 注册设置芯片上下电状态回调函数
    ret = uapi_radar_register_set_power_status_cb(radar_print_set_power_sts_cb_res);
    if (ret != ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP RADAR][SAMPLE]uapi_radar_register_result_cb failed, ret: 0x%x.\r\n", ret);
    }
    // 注册雷达算法错误码上报回调函数
    ret = uapi_radar_register_report_errcode_cb(radar_print_rpt_errcode_cb_res);
    if (ret != ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP RADAR][SAMPLE]uapi_radar_register_report_errcode_cb failed, ret: 0x%x.\r\n", ret);
    }
}

// 获取并打印雷达宽窄带软件版本号
static void print_radar_version(void)
{
    radar_version_info_t radar_ver = {0};
    if (uapi_radar_get_version(&radar_ver) != ERRCODE_RC_SUCCESS) {
        return;
    }
    osal_printk("[SLP_RADAR][SAMPLE] Radar wide band version:[%u.%u.%u]. narrow band version:[%u.%u.%u].\r\n",
        radar_ver.wide_band.major, radar_ver.wide_band.minor, radar_ver.wide_band.patch,
        radar_ver.narrow_band.major, radar_ver.narrow_band.minor, radar_ver.narrow_band.patch);
}

// 开启雷达功能
static void radar_function_start(void)
{
    errcode_radar_client_t ret;
    // 1. 芯片上电
    ret = uapi_radar_set_power_status(CHIP_STATUS_POWERON);
    (void)osDelay(RADAR_POWER_ON_DELAY);    // 芯片上电及版本加载需要1.5s时间
    if (ret != ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP RADAR][SAMPLE]uapi_radar_set_power_status %u failed, ret: 0x%x.\r\n",
            CHIP_STATUS_POWERON, ret);
        return;
    }
    // 2. 参数配置
    radar_alg_para_t alg_para = {0};
    // 算法基础参数
    alg_para.basic_para.rpt_mode = RADAR_DEFAULT_RPT_MODE;
    alg_para.basic_para.sensitivity = RADAR_DEFAULT_SENSITIVITY;
    alg_para.basic_para.rng_boundary = RADAR_DEFAULT_RNG_BOUNDARY;
    alg_para.basic_para.delay_time = RADAR_DEFAULT_DLY_TIME;
    alg_para.basic_para.is_wire_down = 0;
    alg_para.basic_para.static_create_range = RADAR_DEFAULT_STA_CREATE_RNG_THRES;

    // AI补偿参数
    alg_para.ai_para.ai_status = 1;
    for (uint8_t i = 0; i < RADAR_AI_OFFSET_PARA_NUM; i++) {
        alg_para.ai_para.dynamic_offset_direction[i] = 0;
        alg_para.ai_para.static_offset_direction[i] = 0;
    }
    ret = uapi_radar_set_alg_para(&alg_para);
    if (ret != ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP RADAR][SAMPLE]uapi_radar_set_alg_para failed, ret: 0x%x.\r\n", ret);
    }
    // 3. 使能雷达
    ret = uapi_radar_set_status(RADAR_STATUS_START);
    if (ret != ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP_RADAR][SAMPLE]uapi_radar_set_status %u failed, ret: 0x%x\r\n", RADAR_STATUS_START, ret);
    }
}

// 关闭雷达功能
void radar_function_stop(void)
{
    errcode_radar_client_t ret;
    // 1. 先去使能雷达
    ret = uapi_radar_set_status(RADAR_STATUS_STOP);
    if (ret != ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP_RADAR][SAMPLE]slp radar set status %u failed, ret: 0x%x\r\n", RADAR_STATUS_STOP, ret);
    }
    // 2. 再芯片下电
    ret = uapi_radar_set_power_status(CHIP_STATUS_POWEROFF);
    if (ret != ERRCODE_RC_SUCCESS) {
        osal_printk("[SLP_RADAR][SAMPLE]uapi_radar_set_power_status %u failed, ret: 0x%x\r\n",
            CHIP_STATUS_POWERON, ret);
    }
}

int radar_demo_init(void)
{
    osal_printk("[SLP_RADAR][SAMPLE] radar_demo_init start\r\n");
    // 1. 注册雷达结果回调函数
    register_radar_cb();
    // 2. 芯片上电、参数配置、开启雷达
    radar_function_start();
    // 3. 获取版本号
    print_radar_version();
    // 雷达查询接口示例
    while (1) {
        (void)osDelay(RADAR_STATUS_QUERY_DELAY);
        radar_result_t result = {0};
        uapi_radar_get_result(&result);
    }
}

#define SLP_RADAR_TASK_PRIO                  (osPriority_t)(13)
#define SLP_RADAR_TASK_STACK_SIZE            0x1000

void slp_radar_sample_entry(void)
{
    osThreadAttr_t attr;
    attr.name       = "slp_radar_sample";
    attr.attr_bits  = 0U;
    attr.cb_mem     = NULL;
    attr.cb_size    = 0U;
    attr.stack_mem  = NULL;
    attr.stack_size = SLP_RADAR_TASK_STACK_SIZE;
    attr.priority   = SLP_RADAR_TASK_PRIO;
    (void)osThreadNew((osThreadFunc_t)radar_demo_init, NULL, &attr);
}

/* Run the slp_radar_sample_task. */
app_run(slp_radar_sample_entry);