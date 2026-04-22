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

#include "OTAHelper.h"

#include <app/clusters/ota-requestor/BDXDownloader.h>
#include <app/clusters/ota-requestor/DefaultOTARequestor.h>
#include <app/clusters/ota-requestor/DefaultOTARequestorStorage.h>
#include <app/clusters/ota-requestor/ExtendedOTARequestorDriver.h>
#include <platform/hisilicon/OTAImageProcessorImpl.h>
#include <system/SystemEvent.h>

#include <app/clusters/ota-requestor/DefaultOTARequestorUserConsent.h>
#include <lib/shell/Commands.h>
#include <lib/shell/Engine.h>
#include <lib/shell/commands/Help.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip::DeviceLayer;
using namespace chip;

class CustomOTARequestorDriver : public DeviceLayer::ExtendedOTARequestorDriver {
public:
    bool CanConsent() override;
};

namespace {
DefaultOTARequestor g_requestorCore;
DefaultOTARequestorStorage g_requestorStorage;
CustomOTARequestorDriver g_requestorUser;
BDXDownloader g_downloader;
OTAImageProcessorImpl g_imageProcessor;
chip::Optional<bool> g_requestorCanConsent;
static chip::ota::UserConsentState g_userConsentState = chip::ota::UserConsentState::kUnknown;
chip::ota::DefaultOTARequestorUserConsent g_userConsentProvider;

} // namespace

bool CustomOTARequestorDriver::CanConsent()
{
    return g_requestorCanConsent.ValueOr(DeviceLayer::ExtendedOTARequestorDriver::CanConsent());
}

extern "C" void QueryImageCmdHandler()
{
    ChipLogProgress(DeviceLayer, "Calling QueryImageCmdHandler");
    PlatformMgr().ScheduleWork([](intptr_t) { GetRequestorInstance()->TriggerImmediateQuery(); });
}

extern "C" void ApplyUpdateCmdHandler()
{
    ChipLogProgress(DeviceLayer, "Calling ApplyUpdateCmdHandler");
    PlatformMgr().ScheduleWork([](intptr_t) { GetRequestorInstance()->ApplyUpdate(); });
}

extern "C" void NotifyUpdateAppliedHandler(uint32_t version)
{
    ChipLogProgress(DeviceLayer, "NotifyUpdateApplied");
    PlatformMgr().ScheduleWork([](intptr_t) { GetRequestorInstance()->NotifyUpdateApplied(); });
}

extern "C" void HiQueryImageCmdHandler(char * pcWriteBuffer, int xWriteBufferLen, int argc, char ** argv)
{
    QueryImageCmdHandler();
    ChipLogProgress(DeviceLayer, "QueryImageCmdHandler begin");
}

extern "C" void HiApplyUpdateCmdHandler(char * pcWriteBuffer, int xWriteBufferLen, int argc, char ** argv)
{
    ApplyUpdateCmdHandler();
    ChipLogProgress(DeviceLayer, "ApplyUpdateCmdHandler send request");
}

extern "C" void HiNotifyUpdateApplied(char * pcWriteBuffer, int xWriteBufferLen, int argc, char ** argv)
{
    ChipLogProgress(DeviceLayer, "NotifyUpdateApplied send request");
}

static void InitOTARequestorHandler(System::Layer * systemLayer, void * appState)
{
    // Initialize and interconnect the Requestor and Image Processor objects -- START
    SetRequestorInstance(&g_requestorCore);

    g_requestorStorage.Init(Server::GetInstance().GetPersistentStorage());

    // Set server instance used for session establishment
    g_requestorCore.Init(Server::GetInstance(), g_requestorStorage, g_requestorUser, g_downloader);

    g_imageProcessor.SetOTADownloader(&g_downloader);

    // Connect the Downloader and Image Processor objects
    g_downloader.SetImageProcessorDelegate(&g_imageProcessor);
    g_requestorUser.Init(&g_requestorCore, &g_imageProcessor);

    if (g_userConsentState != chip::ota::UserConsentState::kUnknown) {
        g_userConsentProvider.SetUserConsentState(g_userConsentState);
        g_requestorUser.SetUserConsentDelegate(&g_userConsentProvider);
    }

    // Initialize and interconnect the Requestor and Image Processor objects -- END
}

void OTAHelpers::InitOTARequestor()
{
    chip::DeviceLayer::SystemLayer().StartTimer(
        chip::System::Clock::Seconds32(kInitOTARequestorDelaySec), InitOTARequestorHandler, nullptr);
}
