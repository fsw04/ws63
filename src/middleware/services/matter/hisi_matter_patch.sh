#!/bin/sh
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025-2025. All rights reserved.
# 脚本功能：
# 1. 打入海思对matter源码修改的补丁;
# 2. 将海思适配层代码、sample等软链接到matter对应目录;

# 目录配置
CUR_DIR=$(pwd)
ROOT_DIR=$(cd $CUR_DIR/../../../; pwd)
CHIP_DIR=${CUR_DIR}/connectedhomeip
PATCH_DIR=${CUR_DIR}/patch
PATCH_NAME="matter-v1.4.2-hisilicon-patch-2026-02-03.patch"
PATCH_FILE=${PATCH_DIR}/${PATCH_NAME}

ADAPTER_DIR=${CUR_DIR}/adapter
HISI_PLAT_SRC_DIR=${ADAPTER_DIR}/src/platform/hisilicon
HISI_EXAMPLE_DIR=${ADAPTER_DIR}/examples/lighting-app/hisilicon
HISI_EXAMPLE_PAT_DIR=${ADAPTER_DIR}/examples/platform/hisilicon
HISI_CFG_DIR=${ADAPTER_DIR}/config/hisilicon

# 全局配置：开启错误退出（命令执行失败则脚本立即退出），开启管道错误检测
set -euo pipefail
# 日志打印函数（带时间戳）
log_info() {
    echo -e "\033[32m[INFO] $(date +%Y-%m-%d\ %H:%M:%S) $1\033[0m"
}
log_error() {
    echo -e "\033[31m[ERROR] $(date +%Y-%m-%d\ %H:%M:%S) $1\033[0m"
    exit 1  # 错误则退出脚本
}

# 第一步：打入海思patch
cd ${CHIP_DIR}
if [ ! -f "${PATCH_FILE}" ]; then
    log_error "Patch file ${PATCH_FILE} does not exist!"
fi
log_info "applying patch: ${PATCH_NAME}"
if ! git apply --check ${PATCH_FILE}; then
    log_error "check: patch file is not correct!"
fi
git apply "${PATCH_FILE}"
log_info "git apply patch file:${PATCH_NAME} success!"

# 第二步：拷贝海思平台适配代码到connectedhomeip相应目录
if [ -d "${HISI_PLAT_SRC_DIR}" ]; then
cp -r "${HISI_PLAT_SRC_DIR}" ${CHIP_DIR}/src/platform
else
log_error "${HISI_PLAT_SRC_DIR} is incorrect!!!"
fi

if [ -d "${HISI_EXAMPLE_DIR}" ]; then
cp -r "${HISI_EXAMPLE_DIR}" ${CHIP_DIR}/examples/lighting-app
else
log_error "${HISI_PLAT_SRC_DIR} is incorrect!!!"
fi

if [ -d "${HISI_EXAMPLE_PAT_DIR}" ]; then
cp -r "${HISI_EXAMPLE_PAT_DIR}" ${CHIP_DIR}/examples/platform
else
log_error "${HISI_PLAT_SRC_DIR} is incorrect!!!"
fi

if [ -d "${HISI_CFG_DIR}" ]; then
cp -r "${HISI_CFG_DIR}" ${CHIP_DIR}/config
else
log_error "${HISI_CFG_DIR} is incorrect!!!"
fi

# 第三步  创建软链接
create_symlink() {
    local target="$1"
    local link="$2"

    if [ ! -e "$target" ]; then
        log_error "Target: $target does not exist"
    fi

    if [ ! -L "$link" ]; then
        ln -s "$target" "$link"
    fi
}

# 配置目录下的软链接
CFG_DIR=${CHIP_DIR}/config/hisilicon
EXAMPLE_DIR=${CHIP_DIR}/examples/lighting-app/hisilicon
log_info "Creating hisi config symbolic links..."
# 第三方目录下的软链接
cd ${CFG_DIR}
if [ ! -d "third_party" ]; then
mkdir "third_party"
fi
create_symlink "../../.." "${CFG_DIR}/third_party/connectedhomeip"

declare -A config_links=(
    ["build"]="third_party/connectedhomeip/build"
    ["build_overrides"]="../../examples/build_overrides"
)

for link_name in "${!config_links[@]}"; do
    target_path="${config_links[$link_name]}"
    link_path="${CFG_DIR}/$link_name"
    create_symlink "$target_path" "$link_path"
done

# 示例目录下的软链接
cd ${EXAMPLE_DIR}
if [ ! -d "third_party" ]; then
mkdir "third_party"
fi
log_info "Creating hisi examples symbolic links..."
create_symlink "../../build_overrides" "${EXAMPLE_DIR}/build_overrides"
create_symlink "../../../.." "${EXAMPLE_DIR}/third_party/connectedhomeip"

log_info "The Matter software package based on the HiSilicon platform is ready."
