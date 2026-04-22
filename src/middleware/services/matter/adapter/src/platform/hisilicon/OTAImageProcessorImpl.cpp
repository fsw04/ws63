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

#include <app/clusters/ota-requestor/OTADownloader.h>
#include <app/clusters/ota-requestor/OTARequestorInterface.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/hisilicon/ConfigurationManagerImpl.h>
#include <platform/hisilicon/HisiConfig.h>

#include <platform/hisilicon/OTAImageProcessorImpl.h>
#include <cstring>
#include "upg.h"
#include "partition.h"
#include "hal_reboot.h"

#include <lib/core/CHIPError.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/TLV.h>
#include <lib/support/CHIPMem.h>

using namespace chip;
using namespace chip::System;
using namespace ::chip::DeviceLayer::Internal;

namespace chip {
// 定义静态成员变量
uint32_t OTAImageProcessorImpl::mFlashDataOffset = 0;
uint32_t OTAImageProcessorImpl::mUpgStorageSize = 0;

namespace {
void HandleRestart(Layer *systemLayer, void *appState)
{
    ChipLogProgress(SoftwareUpdate, "System restarting for OTA update...");
}
}  // namespace

// 构造函数
OTAImageProcessorImpl::OTAImageProcessorImpl()
    : mDownloader(nullptr), mSHA256Context(nullptr), mIsHashContextInitialized(false), mCurrentStage(STAGE_INITIALIZED),
      mTotalFileBytes(0), mDownloadedBytes(0), mPayloadSize(0), mPayloadProcessedBytes(0), mHeaderParsed(false)
{
    memset_s(mImageDigest, sizeof(mImageDigest), 0, sizeof(mImageDigest));
    mRemainingData = ByteSpan();
}

// 析构函数
OTAImageProcessorImpl::~OTAImageProcessorImpl()
{
    if (mSHA256Context != nullptr) {
        delete mSHA256Context;
        mSHA256Context = nullptr;
    }
}

void OTAImageProcessorImpl::ResetState()
{
    ChipLogProgress(SoftwareUpdate, "Resetting OTA processor state");

    mFlashDataOffset = 0;
    mIsHashContextInitialized = false;
    mCurrentStage = STAGE_INITIALIZED;
    mTotalFileBytes = 0;
    mDownloadedBytes = 0;
    mPayloadProcessedBytes = 0;
    mPayloadSize = 0;
    mHeaderParsed = false;

    memset_s(mImageDigest, sizeof(mImageDigest), 0, sizeof(mImageDigest));
    mRemainingData = ByteSpan();
    if (mSHA256Context != nullptr) {
        delete mSHA256Context;
        mSHA256Context = nullptr;
    }

    mHeaderParser.Clear();
}

CHIP_ERROR OTAImageProcessorImpl::PrepareDownload()
{
    ChipLogProgress(SoftwareUpdate, "Prepare download");

    ResetState();

    // 初始化头部解析器
    mHeaderParser.Init();
    ChipLogProgress(SoftwareUpdate, "Header parser initialized");

    DeviceLayer::PlatformMgr().ScheduleWork(HandlePrepareDownload, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::Finalize()
{
    ChipLogProgress(SoftwareUpdate, "Finalize");
    DeviceLayer::PlatformMgr().ScheduleWork(HandleFinalize, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::Apply()
{
    ChipLogProgress(SoftwareUpdate, "Apply");
    DeviceLayer::PlatformMgr().ScheduleWork(HandleApply, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::Abort()
{
    ChipLogProgress(SoftwareUpdate, "Abort");
    DeviceLayer::PlatformMgr().ScheduleWork(HandleAbort, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::ProcessBlock(ByteSpan &block)
{
    ChipLogProgress(SoftwareUpdate, "Process Block, size: %zu", block.size());

    if ((block.data() == nullptr) || block.empty()) {
        ChipLogError(SoftwareUpdate, "Invalid block data");
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    CHIP_ERROR err = SetBlock(block);
    if (err != CHIP_NO_ERROR) {
        ChipLogError(SoftwareUpdate, "Cannot set block data: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    DeviceLayer::PlatformMgr().ScheduleWork(HandleProcessBlock, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}

bool OTAImageProcessorImpl::IsFirstImageRun()
{
    OTARequestorInterface *requestor = chip::GetRequestorInstance();
    if (requestor == nullptr) {
        ChipLogProgress(SoftwareUpdate, "Requestor instance is null, not first image run");
        return false;
    }

    bool isFirstRun = (requestor->GetCurrentUpdateState() == OTARequestorInterface::OTAUpdateStateEnum::kApplying);
    ChipLogProgress(SoftwareUpdate, "IsFirstImageRun: %s", isFirstRun ? "true" : "false");
    return isFirstRun;
}

CHIP_ERROR OTAImageProcessorImpl::ConfirmCurrentImage()
{
    OTARequestorInterface *requestor = chip::GetRequestorInstance();
    if (requestor == nullptr) {
        ChipLogError(SoftwareUpdate, "Requestor instance is null");
        return CHIP_ERROR_INTERNAL;
    }

    uint32_t currentVersion;
    uint32_t targetVersion = requestor->GetTargetVersion();
    ReturnErrorOnFailure(DeviceLayer::ConfigurationMgr().GetSoftwareVersion(currentVersion));

    ChipLogProgress(
        SoftwareUpdate, "Current version: %" PRIu32 ", Target version: %" PRIu32, currentVersion, targetVersion);

    if (currentVersion != targetVersion) {
        ChipLogError(SoftwareUpdate,
            "Current software version = %" PRIu32 ", expected software version = %" PRIu32,
            currentVersion, targetVersion);
        return CHIP_ERROR_INCORRECT_STATE;
    }

    ChipLogProgress(SoftwareUpdate, "Current image confirmed successfully");
    return CHIP_NO_ERROR;
}

void OTAImageProcessorImpl::HandlePrepareDownload(intptr_t context)
{
    auto *imageProcessor = reinterpret_cast<OTAImageProcessorImpl *>(context);
    if (imageProcessor == nullptr) {
        ChipLogError(SoftwareUpdate, "ImageProcessor context is null");
        return;
    } else if (imageProcessor->mDownloader == nullptr) {
        ChipLogError(SoftwareUpdate, "mDownloader is null");
        return;
    }

    ChipLogProgress(SoftwareUpdate, "Initializing partition system...");

    // 初始化分区系统
    errcode_t ret = uapi_partition_init();
    if (ret != 0) {
        ChipLogError(SoftwareUpdate, "uapi_partition_init failed with error: %d", ret);
        imageProcessor->mDownloader->OnPreparedForDownload(CHIP_ERROR_INTERNAL);
        return;
    }

    // 获取升级存储区大小
    mUpgStorageSize = uapi_upg_get_storage_size();
    ChipLogProgress(SoftwareUpdate, "OTA download prepared successfully");
    imageProcessor->mDownloader->OnPreparedForDownload(CHIP_NO_ERROR);
}

void OTAImageProcessorImpl::HandleFinalize(intptr_t context)
{
    auto *imageProcessor = reinterpret_cast<OTAImageProcessorImpl *>(context);
    if (imageProcessor == nullptr) {
        ChipLogError(SoftwareUpdate, "ImageProcessor context is null");
        return;
    }

    ChipLogProgress(SoftwareUpdate, "Finalizing OTA download...");

    // 验证头部是否已成功解析
    if (!imageProcessor->mHeaderParsed) {
        ChipLogError(SoftwareUpdate, "OTA header not parsed successfully");
        imageProcessor->mDownloader->EndDownload(CHIP_ERROR_INVALID_ARGUMENT);
        return;
    }

    // 验证payload是否处理完整
    if (imageProcessor->mPayloadProcessedBytes != imageProcessor->mPayloadSize) {
        ChipLogError(SoftwareUpdate,
            "Payload size mismatch: processed %" PRIu64 " bytes, expected %" PRIu64 " bytes",
            imageProcessor->mPayloadProcessedBytes,
            imageProcessor->mPayloadSize);
        imageProcessor->mDownloader->EndDownload(CHIP_ERROR_INTEGRITY_CHECK_FAILED);
        return;
    }

    // 完成流式哈希计算并验证
    if (imageProcessor->mIsHashContextInitialized && imageProcessor->mSHA256Context != nullptr) {
        uint8_t computedDigest[OTA_HASH_LEN];
        MutableByteSpan digestSpan(computedDigest, sizeof(computedDigest));

        CHIP_ERROR err = imageProcessor->mSHA256Context->Finish(digestSpan);
        if (err != CHIP_NO_ERROR) {
            ChipLogError(SoftwareUpdate, "Failed to compute hash: %" CHIP_ERROR_FORMAT, err.Format());
            imageProcessor->mDownloader->EndDownload(err);
            return;
        }

        ChipLogProgress(SoftwareUpdate, "Hash calculation completed");

        // 验证哈希值
        if (memcmp(computedDigest, imageProcessor->mImageDigest, OTA_HASH_LEN) != 0) {
            ChipLogError(SoftwareUpdate, "OTA image hash verification failed!");
            imageProcessor->ReleaseBlock();
            imageProcessor->mDownloader->EndDownload(CHIP_ERROR_INTEGRITY_CHECK_FAILED);
            return;
        }

        ChipLogProgress(SoftwareUpdate, "OTA image hash verification passed");
        imageProcessor->mIsHashContextInitialized = false;

        // 清理SHA256上下文
        delete imageProcessor->mSHA256Context;
        imageProcessor->mSHA256Context = nullptr;
    } else {
        ChipLogError(SoftwareUpdate, "Hash context not initialized");
        imageProcessor->mDownloader->EndDownload(CHIP_ERROR_INTERNAL);
        return;
    }

    imageProcessor->ReleaseBlock();
    ChipLogProgress(SoftwareUpdate, "OTA image downloaded and verified successfully");
    imageProcessor->mDownloader->EndDownload(CHIP_NO_ERROR);
}

void OTAImageProcessorImpl::HandleAbort(intptr_t context)
{
    auto *imageProcessor = reinterpret_cast<OTAImageProcessorImpl *>(context);
    if (imageProcessor == nullptr) {
        ChipLogError(SoftwareUpdate, "ImageProcessor context is null");
        return;
    }

    ChipLogProgress(SoftwareUpdate, "Aborting OTA process...");

    // 清理哈希计算上下文
    if (imageProcessor->mIsHashContextInitialized) {
        imageProcessor->mIsHashContextInitialized = false;
        ChipLogProgress(SoftwareUpdate, "Hash context cleared");
    }

    // 清理SHA256上下文
    if (imageProcessor->mSHA256Context != nullptr) {
        delete imageProcessor->mSHA256Context;
        imageProcessor->mSHA256Context = nullptr;
        ChipLogProgress(SoftwareUpdate, "SHA256 context deleted");
    }

    // 释放块数据
    imageProcessor->ReleaseBlock();

    ChipLogProgress(SoftwareUpdate, "OTA process aborted");
}

void OTAImageProcessorImpl::HandleProcessBlock(intptr_t context)
{
    auto *imageProcessor = reinterpret_cast<OTAImageProcessorImpl *>(context);

    if (imageProcessor == nullptr || imageProcessor->mDownloader == nullptr) {
        ChipLogError(SoftwareUpdate, "Invalid context or downloader");
        return;
    }

    ByteSpan block = ByteSpan(imageProcessor->mBlock.data(), imageProcessor->mBlock.size());
    ChipLogProgress(SoftwareUpdate,
        "HandleProcessBlock: stage=%d, processing %zu bytes",
        imageProcessor->mCurrentStage,
        block.size());

    CHIP_ERROR error = imageProcessor->ProcessData(block);
    if (error == CHIP_ERROR_BUFFER_TOO_SMALL) {
        // 需要更多数据才能继续处理
        ChipLogProgress(SoftwareUpdate, "Need more data, requesting next block");
        imageProcessor->mDownloader->FetchNextData();
        return;
    } else if (error != CHIP_NO_ERROR) {
        // 处理错误
        ChipLogError(SoftwareUpdate, "Failed to process data: %" CHIP_ERROR_FORMAT, error.Format());
        imageProcessor->mDownloader->EndDownload(error);
        return;
    }

    // 成功处理，请求下一个数据块
    imageProcessor->mDownloader->FetchNextData();
}

CHIP_ERROR OTAImageProcessorImpl::ProcessData(ByteSpan &block)
{
    CHIP_ERROR error = CHIP_NO_ERROR;

    // 处理之前剩余的未处理数据（如果有）
    if (mRemainingData.size() > 0) {
        ChipLogProgress(SoftwareUpdate, "Processing %zu bytes of previously remaining data", mRemainingData.size());
        ByteSpan tempSpan = mRemainingData;

        if (mCurrentStage == STAGE_PROCESSING_HEADER || !mHeaderParsed) {
            error = ProcessHeaderWithParser(tempSpan);
        } else if (mCurrentStage == STAGE_PROCESSING_PAYLOAD) {
            error = ProcessPayloadData(tempSpan);
        }

        // 更新剩余数据
        mRemainingData = tempSpan;

        if (error == CHIP_ERROR_BUFFER_TOO_SMALL) {
            // 头部还需要更多数据
            return CHIP_NO_ERROR;
        } else if (error != CHIP_NO_ERROR) {
            return error;
        }
    }

    // 处理新的数据块
    while (block.size() > 0 && mCurrentStage != STAGE_COMPLETE && mCurrentStage != STAGE_ERROR) {
        ChipLogProgress(SoftwareUpdate, "Processing new block data: %zu bytes remaining in block", block.size());
        if (mCurrentStage == STAGE_PROCESSING_HEADER || !mHeaderParsed) {
            // 处理头部
            error = ProcessHeaderWithParser(block);
            if (error == CHIP_ERROR_BUFFER_TOO_SMALL) {
                // 头部需要更多数据，保存剩余数据用于下次处理
                if (block.size() > 0) {
                    mRemainingData = block;
                    ChipLogProgress(SoftwareUpdate, "Saved %zu bytes as remaining data for next block", block.size());
                }
                return CHIP_ERROR_BUFFER_TOO_SMALL;
            } else if (error != CHIP_NO_ERROR) {
                ChipLogError(SoftwareUpdate, "Header processing failed: %" CHIP_ERROR_FORMAT, error.Format());
                mCurrentStage = STAGE_ERROR;
                return error;
            }

            // 头部处理成功，继续处理当前块中的payload数据
            if (block.size() > 0 && mCurrentStage == STAGE_PROCESSING_PAYLOAD) {
                ChipLogProgress(SoftwareUpdate,
                    "Header parsed, processing %zu bytes of payload data from current block",
                    block.size());
                error = ProcessPayloadData(block);
                if (error != CHIP_NO_ERROR) {
                    ChipLogError(SoftwareUpdate, "Payload processing failed: %" CHIP_ERROR_FORMAT, error.Format());
                    mCurrentStage = STAGE_ERROR;
                    return error;
                }
            }
        } else if (mCurrentStage == STAGE_PROCESSING_PAYLOAD) {
            // 处理payload
            error = ProcessPayloadData(block);
            if (error != CHIP_NO_ERROR) {
                ChipLogError(SoftwareUpdate, "Payload processing failed: %" CHIP_ERROR_FORMAT, error.Format());
                mCurrentStage = STAGE_ERROR;
                return error;
            }
        } else {
            ChipLogError(SoftwareUpdate, "Invalid processing stage: %d", mCurrentStage);
            return CHIP_ERROR_INCORRECT_STATE;
        }
    }

    // 检查是否完成
    if (mCurrentStage == STAGE_PROCESSING_PAYLOAD && mPayloadProcessedBytes >= mPayloadSize) {
        mCurrentStage = STAGE_COMPLETE;
        ChipLogProgress(SoftwareUpdate, "All data processed, download complete!");
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::ProcessHeaderWithParser(ByteSpan &block)
{
    if (mHeaderParsed) {
        ChipLogProgress(SoftwareUpdate, "Header already parsed");
        mCurrentStage = STAGE_PROCESSING_PAYLOAD;
        return CHIP_NO_ERROR;
    }

    ChipLogProgress(SoftwareUpdate, "Processing header with standard parser, input size: %zu", block.size());

    ByteSpan originalBlock = block;
    CHIP_ERROR error = mHeaderParser.AccumulateAndDecode(block, mHeader);
    if (error == CHIP_ERROR_BUFFER_TOO_SMALL) {
        ChipLogProgress(
            SoftwareUpdate, "Header parser needs more data (consumed %zu bytes)", originalBlock.size() - block.size());
        mCurrentStage = STAGE_PROCESSING_HEADER;
        return CHIP_ERROR_INTERNAL;
    } else if (error != CHIP_NO_ERROR) {
        ChipLogError(SoftwareUpdate, "Failed to decode OTA header: %" CHIP_ERROR_FORMAT, error.Format());
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // 头部解析成功
    ChipLogProgress(SoftwareUpdate, "OTA Image Header parsed successfully");

    // 验证摘要类型并保存预期哈希值
    if (mHeader.mImageDigestType != OTAImageDigestType::kSha256) {
        ChipLogError(SoftwareUpdate, "Unsupported digest type: %d", static_cast<int>(mHeader.mImageDigestType));
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    if (mHeader.mImageDigest.size() != OTA_HASH_LEN) {
        ChipLogError(SoftwareUpdate, "Invalid digest length: %zu", mHeader.mImageDigest.size());
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // 保存预期的哈希值
    errcode_t ret = memcpy_s(mImageDigest, sizeof(mImageDigest), mHeader.mImageDigest.data(), OTA_HASH_LEN);
    if (ret != 0) {
        ChipLogError(SoftwareUpdate, "memcpy_s failed. ret:0x%x", ret);
        return CHIP_ERROR_INTERNAL;
    }
    mPayloadSize = mHeader.mPayloadSize;

    // 验证存储容量
    if (mPayloadSize > mUpgStorageSize) {
        ChipLogError(SoftwareUpdate,
            "OTA payload size (%" PRIu64 ") exceeds storage capacity (%u)",
            mPayloadSize, mUpgStorageSize);
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }

    // 准备升级
    ChipLogProgress(SoftwareUpdate, "Preparing OTA upgrade for payload size: %" PRIu64, mPayloadSize);
    upg_prepare_info_t prepare_info;
    prepare_info.package_len = mPayloadSize;
    ret = uapi_upg_prepare(&prepare_info);
    if (ret != 0) {
        ChipLogError(SoftwareUpdate, "uapi_upg_prepare failed. ret:0x%x", ret);
        return CHIP_ERROR_INTERNAL;
    }

    // 初始化哈希计算
    error = InitializeHashCalculation();
    if (error != CHIP_NO_ERROR) {
        ChipLogError(SoftwareUpdate, "Failed to initialize hash calculation");
        return error;
    }

    mHeaderParsed = true;
    mCurrentStage = STAGE_PROCESSING_PAYLOAD;

    ChipLogProgress(SoftwareUpdate, "Header processing completed successfully");

    return CHIP_NO_ERROR;
}

void OTAImageProcessorImpl::HandleApply(intptr_t context)
{
    ChipLogProgress(SoftwareUpdate, "OTA image downloaded and verified successfully, preparing to reboot system...");
    constexpr int OTA_AUTO_REBOOT_DELAY_MS = 1000;
    auto *imageProcessor = reinterpret_cast<OTAImageProcessorImpl *>(context);
    if (imageProcessor == nullptr) {
        ChipLogError(SoftwareUpdate, "ImageProcessor context is null");
        return;
    }

    // 保存新版本号到配置
    ChipLogProgress(SoftwareUpdate, "Saving new software version: %" PRIu32, imageProcessor->mHeader.mSoftwareVersion);

    CHIP_ERROR err =
        HisiConfig::WriteConfigValue(HisiConfig::kConfigKey_SoftwareVersion, imageProcessor->mHeader.mSoftwareVersion);
    if (err != CHIP_NO_ERROR) {
        ChipLogError(SoftwareUpdate, "Failed to write software version: %" CHIP_ERROR_FORMAT, err.Format());
    } else {
        ChipLogProgress(
            SoftwareUpdate, "Software version updated to: %" PRIu32, imageProcessor->mHeader.mSoftwareVersion);
    }

    // 请求升级
    ChipLogProgress(SoftwareUpdate, "Requesting upgrade...");
    errcode_t ret = uapi_upg_request_upgrade(true);
    if (ret != 0) {
        ChipLogError(SoftwareUpdate, "uapi_upg_request_upgrade failed with error: %d", ret);
        return;
    }

    ChipLogProgress(SoftwareUpdate, "System will reboot in %d ms for OTA update", OTA_AUTO_REBOOT_DELAY_MS);

    // 延迟重启
    DeviceLayer::SystemLayer().StartTimer(
        System::Clock::Milliseconds32(OTA_AUTO_REBOOT_DELAY_MS), HandleRestart, nullptr);
}

CHIP_ERROR OTAImageProcessorImpl::SetBlock(ByteSpan &block)
{
    if (block.empty()) {
        ChipLogProgress(SoftwareUpdate, "Setting empty block, releasing current block");
        ReleaseBlock();
        return CHIP_NO_ERROR;
    }

    if (mBlock.size() < block.size()) {
        ChipLogProgress(
            SoftwareUpdate, "Allocating new block of size %zu (current size: %zu)", block.size(), mBlock.size());

        if (!mBlock.empty()) {
            ReleaseBlock();
        }

        uint8_t *mBlock_ptr = static_cast<uint8_t *>(chip::Platform::MemoryAlloc(block.size()));
        if (mBlock_ptr == nullptr) {
            ChipLogError(SoftwareUpdate, "Memory allocation failed for block of size %zu", block.size());
            return CHIP_ERROR_NO_MEMORY;
        }

        mBlock = MutableByteSpan(mBlock_ptr, block.size());
        ChipLogProgress(SoftwareUpdate, "Block allocated at %p, size: %zu", mBlock_ptr, block.size());
    }

    CHIP_ERROR err = CopySpanToMutableSpan(block, mBlock);
    if (err != CHIP_NO_ERROR) {
        ChipLogError(SoftwareUpdate, "Cannot copy block data: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    ChipLogProgress(SoftwareUpdate, "Block set successfully, size: %zu", mBlock.size());
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::ReleaseBlock()
{
    if (mBlock.data() != nullptr) {
        ChipLogProgress(SoftwareUpdate, "Releasing block at %p, size: %zu", mBlock.data(), mBlock.size());
        chip::Platform::MemoryFree(mBlock.data());
    } else {
        ChipLogProgress(SoftwareUpdate, "Block already released");
    }

    mBlock = MutableByteSpan();
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::AccumulateBuffer(uint8_t *buffer, uint32_t bufferSize, uint32_t &bytesReceived,
    ByteSpan &block, uint32_t totalBytesNeeded, const char *bufferName)
{
    if (bytesReceived >= totalBytesNeeded) {
        return CHIP_NO_ERROR;  // 已经累积完成
    }

    // 检查缓冲区大小是否足够
    if (totalBytesNeeded > bufferSize) {
        ChipLogError(SoftwareUpdate, "%s buffer too small: need %u, have %u", bufferName, totalBytesNeeded, bufferSize);
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }

    // 计算还需要多少字节
    uint32_t bytesNeeded = totalBytesNeeded - bytesReceived;
    uint32_t bytesToCopy = (block.size() < bytesNeeded) ? static_cast<uint32_t>(block.size()) : bytesNeeded;
    if (bytesToCopy > 0) {
        if (memcpy_s(buffer + bytesReceived, bytesToCopy, block.data(), bytesToCopy) != 0) {
            ChipLogError(SoftwareUpdate, "memcpy_s failed!");
            return CHIP_ERROR_INTERNAL;
        }
        bytesReceived += bytesToCopy;
        block = block.SubSpan(bytesToCopy);

        ChipLogProgress(SoftwareUpdate, "Accumulated %u bytes to %s buffer, total: %u/%u", bytesToCopy,
            bufferName, bytesReceived, totalBytesNeeded);
    }

    // 如果还没有累积足够的数据，需要更多数据
    if (bytesReceived < totalBytesNeeded) {
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::InitializeHashCalculation()
{
    // 初始化流式哈希计算上下文（计算整个payload部分）
    mSHA256Context = new Crypto::Hash_SHA256_stream();
    CHIP_ERROR sha256Err = mSHA256Context->Begin();
    if (sha256Err != CHIP_NO_ERROR) {
        ChipLogError(SoftwareUpdate, "Failed to begin hash calculation: %" CHIP_ERROR_FORMAT, sha256Err.Format());
        delete mSHA256Context;
        mSHA256Context = nullptr;
        return sha256Err;
    }
    mIsHashContextInitialized = true;

    ChipLogProgress(SoftwareUpdate, "Hash calculation initialized for payload verification");
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::ProcessPayloadData(ByteSpan &block)
{
    if (block.empty()) {
        return CHIP_NO_ERROR;
    }

    // 计算本次处理的字节数（不能超过剩余payload大小）
    uint64_t remainingPayload = mPayloadSize - mPayloadProcessedBytes;
    size_t bytesToProcess = block.size();
    if (bytesToProcess > remainingPayload) {
        bytesToProcess = static_cast<size_t>(remainingPayload);
        ChipLogProgress(
            SoftwareUpdate, "Trimming block from %zu to %zu bytes to fit payload size", block.size(), bytesToProcess);
    }

    ByteSpan payloadChunk = block.SubSpan(0, bytesToProcess);

    // 计算哈希（只计算payload部分）
    if (mIsHashContextInitialized && mSHA256Context != nullptr) {
        CHIP_ERROR err = mSHA256Context->AddData(payloadChunk);
        if (err != CHIP_NO_ERROR) {
            ChipLogError(SoftwareUpdate,
                "Failed to update hash with %zu bytes: %" CHIP_ERROR_FORMAT,
                payloadChunk.size(), err.Format());
            return err;
        }

        ChipLogProgress(SoftwareUpdate, "Hash updated with %zu bytes of payload", payloadChunk.size());
    } else {
        ChipLogError(SoftwareUpdate, "Hash context not initialized");
        return CHIP_ERROR_INTERNAL;
    }

    // 写入Flash
    ChipLogProgress(SoftwareUpdate, "Writing %zu bytes to flash at offset %u", payloadChunk.size(), mFlashDataOffset);

    errcode_t ret =
        uapi_upg_write_package_sync(mFlashDataOffset, const_cast<uint8_t *>(payloadChunk.data()), payloadChunk.size());
    if (ret != 0) {
        ChipLogError(SoftwareUpdate, "uapi_upg_write_package_sync failed with error: %d", ret);
        return CHIP_ERROR_WRITE_FAILED;
    }

    // 更新统计信息
    mFlashDataOffset += payloadChunk.size();
    mPayloadProcessedBytes += payloadChunk.size();

    // 计算总文件大小（基于头部大小+payload大小）
    if (mTotalFileBytes == 0) {
        // 估算总大小：固定头部16字节 + TLV头部 + payload大小
        mTotalFileBytes = 16 + mPayloadSize;
    }

    mDownloadedBytes = mFlashDataOffset;  // 近似估算

    // 更新输入块，移除已处理的部分
    block = block.SubSpan(payloadChunk.size());

    ChipLogProgress(
        SoftwareUpdate, "Payload processed: %" PRIu64 "/%" PRIu64 " bytes", mPayloadProcessedBytes, mPayloadSize);
    // 检查是否完成
    if (mPayloadProcessedBytes >= mPayloadSize) {
        ChipLogProgress(SoftwareUpdate, "Payload processing completed");
        mCurrentStage = STAGE_COMPLETE;
    }

    return CHIP_NO_ERROR;
}
}  // namespace chip