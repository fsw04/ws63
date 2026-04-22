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

#pragma once

#include <cstring>
#include <lib/core/OTAImageHeader.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/OTAImageProcessor.h>
#include <crypto/CHIPCryptoPAL.h>

namespace chip {
#define OTA_HASH_LEN 32
class OTAImageProcessorImpl : public OTAImageProcessorInterface {
public:
    OTAImageProcessorImpl();
    ~OTAImageProcessorImpl() override;

    //////////// OTAImageProcessorInterface Implementation ///////////////
    CHIP_ERROR PrepareDownload() override;
    CHIP_ERROR Finalize() override;
    CHIP_ERROR Apply() override;
    CHIP_ERROR Abort() override;
    CHIP_ERROR ProcessBlock(ByteSpan &block) override;
    bool IsFirstImageRun() override;
    CHIP_ERROR ConfirmCurrentImage() override;
    void SetOTADownloader(OTADownloader *downloader)
    {
        mDownloader = downloader;
    }

private:
    //////////// Actual handlers for the OTAImageProcessorInterface ///////////////
    static void HandlePrepareDownload(intptr_t context);
    static void HandleFinalize(intptr_t context);
    static void HandleAbort(intptr_t context);
    static void HandleProcessBlock(intptr_t context);
    static void HandleApply(intptr_t context);

    CHIP_ERROR SetBlock(ByteSpan &block);
    CHIP_ERROR ReleaseBlock();

    CHIP_ERROR ProcessData(ByteSpan &block);

    // 使用标准解析器处理头部
    CHIP_ERROR ProcessHeaderWithParser(ByteSpan &block);

    // 处理payload
    CHIP_ERROR ProcessPayloadData(ByteSpan &block);

    // 初始化哈希计算
    CHIP_ERROR InitializeHashCalculation();

    // 辅助函数
    void ResetState();

    // 数据累积相关函数
    CHIP_ERROR AccumulateBuffer(uint8_t *buffer, uint32_t bufferSize, uint32_t &bytesReceived, ByteSpan &block,
        uint32_t totalBytesNeeded, const char *bufferName);

    // 核心数据结构
    MutableByteSpan mBlock;
    OTADownloader *mDownloader;
    OTAImageHeaderParser mHeaderParser;

    // 流式哈希计算
    Crypto::Hash_SHA256_stream *mSHA256Context;
    bool mIsHashContextInitialized;

    // 处理阶段
    enum ProcessingStage {
        STAGE_INITIALIZED,
        STAGE_PROCESSING_HEADER,
        STAGE_PROCESSING_PAYLOAD,
        STAGE_COMPLETE,
        STAGE_ERROR
    };
    ProcessingStage mCurrentStage;

    // 数据统计
    uint8_t mImageDigest[OTA_HASH_LEN];  // SHA256哈希长度固定为32字节
    uint64_t mTotalFileBytes;         // 总文件大小
    uint64_t mDownloadedBytes;        // 已下载字节数
    uint64_t mPayloadSize;            // payload大小
    uint64_t mPayloadProcessedBytes;  // 已处理的payload字节数

    // 用于跟踪头部解析状态
    bool mHeaderParsed;
    ByteSpan mRemainingData;  // 当前块中剩余未处理的数据

    OTAImageHeader mHeader;  // 解析后的头部信息

    // 静态成员变量
    static uint32_t mFlashDataOffset;
    static uint32_t mUpgStorageSize;
};

}  // namespace chip