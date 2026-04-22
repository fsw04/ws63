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

#include "include/DeviceCallbacks.h"
#include "app/server/Server.h"

#include <common/HisiAppServer.h>
#include <common/CHIPDeviceManager.h>

#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <platform/CHIPDeviceLayer.h>
#include <lib/support/CHIPMem.h>

#include <app/clusters/identify-server/identify-server.h>
#include <app/clusters/network-commissioning/network-commissioning.h>
#include <lib/core/ErrorStr.h>
#include <platform/hisilicon/HisiConfig.h>
#include <platform/hisilicon/NetworkCommissioningDriver.h>
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>

#include <ota/OTAHelper.h>

using chip::ByteSpan;
using chip::EndpointId;
using chip::FabricIndex;
using chip::NodeId;
using chip::OnDeviceConnected;
using chip::OnDeviceConnectionFailure;
using chip::PeerId;
using chip::Server;
using chip::VendorId;
using chip::Callback::Callback;
using chip::System::Layer;
using chip::Transport::PeerAddress;
using namespace chip::Messaging;

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceManager;
using namespace ::chip::DeviceLayer;

static AppDeviceCallbacks EchoCallbacks;

void OnIdentifyStart(Identify *)
{
    ChipLogProgress(Zcl, "OnIdentifyStart");
}

void OnIdentifyStop(Identify *)
{
    ChipLogProgress(Zcl, "OnIdentifyStop");
}

void OnTriggerEffect(Identify * identify)
{
    switch (identify->mCurrentEffectIdentifier) {
        case Clusters::Identify::EffectIdentifierEnum::kBlink:
            ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kBlink");
            break;
        case Clusters::Identify::EffectIdentifierEnum::kBreathe:
            ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kBreathe");
            break;
        case Clusters::Identify::EffectIdentifierEnum::kOkay:
            ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kOkay");
            break;
        case Clusters::Identify::EffectIdentifierEnum::kChannelChange:
            ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kChannelChange");
            break;
        default:
            ChipLogProgress(Zcl, "No identifier effect");
            return;
    }
}

static Identify gIdentify1 = {
    chip::EndpointId{ 1 }, OnIdentifyStart, OnIdentifyStop, Clusters::Identify::IdentifyTypeEnum::kVisibleIndicator,
    OnTriggerEffect,
};

static void InitServer(intptr_t context)
{
    ChipLogProgress(DeviceLayer, "Matter App InitServer start.");
    HisiAppServer::Init();
    OTAHelpers::Instance().InitOTARequestor();

    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
}

extern "C" void MatterAppEntry(void)
{
    ChipLogProgress(DeviceLayer, "Lighting App Entry.");
    CHIP_ERROR err = CHIP_NO_ERROR;
    static const uint32_t WAIT_TIME_MS = 100;

    CHIPDeviceManager & deviceMgr = CHIPDeviceManager::GetInstance();
    err                           = deviceMgr.Init(&EchoCallbacks); // start the CHIP task
    if (err != CHIP_NO_ERROR) {
        ChipLogError(DeviceLayer, "DeviceManagerInit() - ERROR!\r\n");
    } else {
        ChipLogProgress(DeviceLayer, "DeviceManagerInit() - OK\r\n");
    }
    chip::DeviceLayer::PlatformMgr().ScheduleWork(InitServer, 0);

    ConfigurationMgr().LogDeviceConfig();

    while (true) {
        osDelay(WAIT_TIME_MS);
    }

    ChipLogProgress(SoftwareUpdate, "Exited");
    return;
}
