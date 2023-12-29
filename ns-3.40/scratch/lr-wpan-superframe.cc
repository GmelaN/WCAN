#include <ns3/core-module.h>
#include <ns3/lr-wpan-module.h>
#include <ns3/simulator.h>

#include <ns3/network-module.h>

#include <ns3/net-device-container.h>

// channel
#include <ns3/propagation-module.h>
#include <ns3/spectrum-module.h>

// mobility model
#include <ns3/mobility-helper.h>
#include <ns3/mobility-module.h>

#include <ns3/net-device.h>

#include <ns3/callback.h>

#include <iostream>

using namespace ns3;


/////////////////////////// CALLBACK ///////////////////////////
static void
ScanConfirm(Ptr<LrWpanNetDevice> device, MlmeScanConfirmParams params)
{
    // The algorithm to select which coordinator to associate is not
    // covered by the standard. In this case, we use the coordinator
    // with the highest LQI value obtained from a passive scan and make
    // sure this coordinator allows association.

    if (params.m_status == MLMESCAN_SUCCESS)
    {
        // Select the coordinator with the highest LQI from the PAN Descriptor List
        int maxLqi = 0;
        int panDescIndex = 0;
        if (!params.m_panDescList.empty())
        {
            for (uint32_t i = 0; i < params.m_panDescList.size(); i++)
            {
                if (params.m_panDescList[i].m_linkQuality > maxLqi)
                {
                    maxLqi = params.m_panDescList[i].m_linkQuality;
                    panDescIndex = i;
                }
            }

            // Only request association if the coordinator is permitting association at this moment.
            if (params.m_panDescList[panDescIndex].m_superframeSpec.IsAssocPermit())
            {
                std::string addressing;
                if (params.m_panDescList[panDescIndex].m_coorAddrMode == SHORT_ADDR)
                {
                    addressing = "Short";
                }
                else if (params.m_panDescList[panDescIndex].m_coorAddrMode == EXT_ADDR)
                {
                    addressing = "Ext";
                }

                std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                          << " [" << device->GetMac()->GetShortAddress() << " | "
                          << device->GetMac()->GetExtendedAddress() << "]"
                          << " MLME-scan.confirm:  Selected PAN ID "
                          << params.m_panDescList[panDescIndex].m_coorPanId
                          << "| Coord addressing mode: " << addressing << " | LQI "
                          << static_cast<int>(params.m_panDescList[panDescIndex].m_linkQuality)
                          << "\n";

                if (params.m_panDescList[panDescIndex].m_linkQuality >= 127)
                {
                    MlmeAssociateRequestParams assocParams;
                    assocParams.m_chNum = params.m_panDescList[panDescIndex].m_logCh;
                    assocParams.m_chPage = params.m_panDescList[panDescIndex].m_logChPage;
                    assocParams.m_coordPanId = params.m_panDescList[panDescIndex].m_coorPanId;
                    assocParams.m_coordAddrMode = params.m_panDescList[panDescIndex].m_coorAddrMode;

                    if (params.m_panDescList[panDescIndex].m_coorAddrMode ==
                        LrWpanAddressMode::SHORT_ADDR)
                    {
                        assocParams.m_coordAddrMode = LrWpanAddressMode::SHORT_ADDR;
                        assocParams.m_coordShortAddr =
                            params.m_panDescList[panDescIndex].m_coorShortAddr;
                        assocParams.m_capabilityInfo.SetShortAddrAllocOn(true);
                    }
                    else if (assocParams.m_coordAddrMode == LrWpanAddressMode::EXT_ADDR)
                    {
                        assocParams.m_coordAddrMode = LrWpanAddressMode::EXT_ADDR;
                        assocParams.m_coordExtAddr =
                            params.m_panDescList[panDescIndex].m_coorExtAddr;
                        assocParams.m_coordShortAddr = Mac16Address("ff:fe");
                        assocParams.m_capabilityInfo.SetShortAddrAllocOn(false);
                    }

                    Simulator::ScheduleNow(&LrWpanMac::MlmeAssociateRequest,
                                           device->GetMac(),
                                           assocParams);
                }
                else
                {
                    std::cout << Simulator::Now().As(Time::S) << " Node "
                              << device->GetNode()->GetId() << " ["
                              << device->GetMac()->GetShortAddress() << " | "
                              << device->GetMac()->GetExtendedAddress() << "]"
                              << " MLME-scan.confirm: Beacon found but link quality too low for "
                                 "association.\n";
                }
            }
        }
        else
        {
            std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId()
                      << " [" << device->GetMac()->GetShortAddress() << " | "
                      << device->GetMac()->GetExtendedAddress()
                      << "] MLME-scan.confirm: Beacon not found.\n";
        }
    }
    else
    {
        std::cout << Simulator::Now().As(Time::S) << " [" << device->GetMac()->GetShortAddress()
                  << " | " << device->GetMac()->GetExtendedAddress()
                  << "]  error occurred, scan failed.\n";
    }
}

static void
AssociateConfirm(Ptr<LrWpanNetDevice> device, MlmeAssociateConfirmParams params)
{
    // Used by device higher layer to inform the results of a
    // association procedure from its mac layer.This is implemented by other protocol stacks
    // and is only here for demonstration purposes.
    if (params.m_status == LrWpanMlmeAssociateConfirmStatus::MLMEASSOC_SUCCESS)
    {
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId() << " ["
                  << device->GetMac()->GetShortAddress() << " | "
                  << device->GetMac()->GetExtendedAddress() << "]"
                  << " MLME-associate.confirm: Association with coordinator successful."
                  << " (PAN: " << device->GetMac()->GetPanId()
                  << " | CoordShort: " << device->GetMac()->GetCoordShortAddress()
                  << " | CoordExt: " << device->GetMac()->GetCoordExtAddress() << ")\n";
    }
    else if (params.m_status == LrWpanMlmeAssociateConfirmStatus::MLMEASSOC_NO_ACK)
    {
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId() << " ["
                  << device->GetMac()->GetShortAddress() << " | "
                  << device->GetMac()->GetExtendedAddress() << "]"
                  << " MLME-associate.confirm: Association with coordinator FAILED (NO ACK).\n";
    }
    else
    {
        std::cout << Simulator::Now().As(Time::S) << " Node " << device->GetNode()->GetId() << " ["
                  << device->GetMac()->GetShortAddress() << " | "
                  << device->GetMac()->GetExtendedAddress() << "]"
                  << " MLME-associate.confirm: Association with coordinator FAILED.\n";
    }
}

static void
AssociateIndication(Ptr<LrWpanNetDevice> device, MlmeAssociateIndicationParams params)
{
    // This is typically implemented by the coordinator next layer (3rd layer or higher).
    // The steps described below are out of the scope of the standard.

    // Here the 3rd layer should check:
    //    a) Whether or not the device was previously associated with this PAN
    //       (the coordinator keeps a list).
    //    b) The coordinator have sufficient resources available to allow the
    //       association.
    // If the association fails, status = 1 or 2 and assocShortAddr = FFFF.

    // In this example, the coordinator accepts every association request and have no association
    // limits. Furthermore, previous associated devices are not checked.

    // When short address allocation is on (set initially in the association request), the
    // coordinator is supposed to assign a short address. In here, we just do a dummy address
    // assign. The assigned short address is just a truncated version of the device existing
    // extended address (i.e the default short address).

    MlmeAssociateResponseParams assocRespParams;

    assocRespParams.m_extDevAddr = params.m_extDevAddr;
    assocRespParams.m_status = LrWpanAssociationStatus::ASSOCIATED;
    if (params.capabilityInfo.IsShortAddrAllocOn())
    {
        // Truncate the extended address and make an assigned
        // short address based on this. This mechanism is not described by the standard.
        // It is just implemented here as a quick and dirty way to assign short addresses.
        uint8_t buffer64MacAddr[8];
        uint8_t buffer16MacAddr[2];

        params.m_extDevAddr.CopyTo(buffer64MacAddr);
        buffer16MacAddr[1] = buffer64MacAddr[7];
        buffer16MacAddr[0] = buffer64MacAddr[6];

        Mac16Address shortAddr;
        shortAddr.CopyFrom(buffer16MacAddr);
        assocRespParams.m_assocShortAddr = shortAddr;
    }
    else
    {
        // If Short Address allocation flag is false, the device will
        // use its extended address to send data packets and short address will be
        // equal to ff:fe. See 802.15.4-2011 (Section 5.3.2.2)
        assocRespParams.m_assocShortAddr = Mac16Address("ff:fe");
    }

    Simulator::ScheduleNow(&LrWpanMac::MlmeAssociateResponse, device->GetMac(), assocRespParams);
}

static void
CommStatusIndication(Ptr<LrWpanNetDevice> device, MlmeCommStatusIndicationParams params)
{
    // Used by coordinator higher layer to inform results of a
    // association procedure from its mac layer.This is implemented by other protocol stacks
    // and is only here for demonstration purposes.
    switch (params.m_status)
    {
    case LrWpanMlmeCommStatus::MLMECOMMSTATUS_TRANSACTION_EXPIRED:
        std::cout << Simulator::Now().As(Time::S) << " Coordinator " << device->GetNode()->GetId()
                  << " [" << device->GetMac()->GetShortAddress() << " | "
                  << device->GetMac()->GetExtendedAddress() << "]"
                  << " MLME-comm-status.indication: Transaction for device " << params.m_dstExtAddr
                  << " EXPIRED in pending transaction list\n";
        break;
    case LrWpanMlmeCommStatus::MLMECOMMSTATUS_NO_ACK:
        std::cout << Simulator::Now().As(Time::S) << " Coordinator " << device->GetNode()->GetId()
                  << " [" << device->GetMac()->GetShortAddress() << " | "
                  << device->GetMac()->GetExtendedAddress() << "]"
                  << " MLME-comm-status.indication: NO ACK from " << params.m_dstExtAddr
                  << " device registered in the pending transaction list\n";
        break;

    case LrWpanMlmeCommStatus::MLMECOMMSTATUS_CHANNEL_ACCESS_FAILURE:
        std::cout << Simulator::Now().As(Time::S) << " Coordinator " << device->GetNode()->GetId()
                  << " [" << device->GetMac()->GetShortAddress() << " | "
                  << device->GetMac()->GetExtendedAddress() << "]"
                  << " MLME-comm-status.indication: CHANNEL ACCESS problem in transaction for "
                  << params.m_dstExtAddr << " registered in the pending transaction list\n";
        break;

    default:
        break;
    }
}

static void 
CoordinatorMcpsDataIndication(McpsDataIndicationParams params, Ptr<Packet> p)
{
    uint8_t* buffer = new uint8_t[p->GetSize() + 1];
    p->CopyData(buffer, p->GetSize());

    std::string packetBody = std::string((char*) buffer, p->GetSize());
    delete[] buffer;

    NS_LOG_UNCOND("Coordinator received message \"" << packetBody << "\"" << std::endl);
}

static void 
RFDMcpsDataConfirm(McpsDataConfirmParams params)
{
    NS_LOG_UNCOND("+" << Simulator::Now().GetSeconds() << " RFD MCPS-DATA.confirm status: " << params.m_status << std::endl);
}
//////////////////////////////////////////////////////////////////

/// @brief LR-WPAN 노드 디바이스를 생성합니다. 코디네이터는 NodeContainer의 첫 번째 노드이고, MAC 주소는 FF:FE입니다.
/// @param nodeCount 코디네이터 1개를 제외한 디바이스의 수
/// @return 생성된 디바이스를 담은 NodeContainer
NodeContainer createNodes(uint32_t nodeCount)
{
    NodeContainer nodes;
    MobilityHelper mobilityHelper;
    LrWpanHelper lrWpanHelper;              // channel은 디폴트로

    nodes.Create(nodeCount);

    mobilityHelper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityHelper.Install(nodes);

    NetDeviceContainer netDevices = lrWpanHelper.Install(nodes);
    lrWpanHelper.SetExtendedAddresses(netDevices);

    return nodes;
}


int main(int argc, char *argv[]) {
    NodeContainer reducedFunctionalDevices = createNodes(10);
    Ptr<Node> coordinator = CreateObject<Node>();
    Ptr<LrWpanNetDevice> coordinatorNetDevice = CreateObject<LrWpanNetDevice>();
    coordinator->AddDevice(coordinatorNetDevice);

    // 코디네이터 설정
    Ptr<ConstantPositionMobilityModel> mobilityModel = CreateObject<ConstantPositionMobilityModel>();
    mobilityModel->SetPosition(Vector3D(0, 0, 0));

    coordinatorNetDevice->GetPhy()->SetMobility(mobilityModel);
    
    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    channel->AddPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

    coordinatorNetDevice->SetChannel(channel);

    coordinatorNetDevice->GetMac()->SetMlmeAssociateIndicationCallback(
        MakeBoundCallback(&AssociateIndication, coordinatorNetDevice));
    coordinatorNetDevice->GetMac()->SetMlmeCommStatusIndicationCallback(
        MakeBoundCallback(&CommStatusIndication, coordinatorNetDevice));
    coordinatorNetDevice->GetMac()->SetMcpsDataIndicationCallback(
        MakeCallback(&CoordinatorMcpsDataIndication));

    coordinatorNetDevice->GetMac()->SetShortAddress(Mac16Address("FF:FE"));

    MlmeStartRequestParams params;
    params.m_panCoor = true;
    params.m_PanId = 5;
    params.m_bcnOrd = 3;
    params.m_sfrmOrd = 3;
    params.m_logCh = 12;
    params.m_coorRealgn = false;

    // 다른 디바이스에 PAN의 존재를 알리기 위해, MLME-START.request로 비콘 신호 전송 시작
    Simulator::ScheduleWithContext(coordinator->GetId(),
                                   Seconds(0.0),
                                   &LrWpanMac::MlmeStartRequest,
                                   coordinatorNetDevice->GetMac(),
                                   params);

    // 다른 디바이스 설정
    for(NodeContainer::Iterator i = reducedFunctionalDevices.Begin(); i != reducedFunctionalDevices.End(); i++) {
        Ptr<Node> node = *i;
        Ptr<LrWpanNetDevice> lrwpanDevice = DynamicCast<LrWpanNetDevice>(node->GetDevice(0));

        // 스캔 확인 콜백, 코디네이터와의 연결 확인 콜백 설정
        lrwpanDevice->GetMac()->SetMlmeScanConfirmCallback(
            MakeBoundCallback(&ScanConfirm, lrwpanDevice));
        lrwpanDevice->GetMac()->SetMlmeAssociateConfirmCallback(
            MakeBoundCallback(&AssociateConfirm, lrwpanDevice));


        // Devices initiate channels scan on channels 11, 12, 13, and 14 looking for beacons
        // Scan Channels represented by bits 0-26  (27 LSB)
        //                       ch 14  ch 11
        //                           |  |
        // 0x7800  = 0000000000000000111100000000000

        // 코디네이터 스캔
        MlmeScanRequestParams scanParams;
        scanParams.m_chPage = 0;
        scanParams.m_scanChannels = 0x7800;
        scanParams.m_scanDuration = 14;
        scanParams.m_scanType = MLMESCAN_PASSIVE;

        // We start the scanning process 100 milliseconds apart for each device
        // to avoid a storm of association requests with the coordinators
        // RFD 개별 스캔 시작
        Time jitter = Seconds(2) + MilliSeconds(std::distance(reducedFunctionalDevices.Begin() + 1, i) * 1000);
        Simulator::ScheduleWithContext(node->GetId(),
                                       jitter,
                                       &LrWpanMac::MlmeScanRequest,
                                       lrwpanDevice->GetMac(),
                                       scanParams);

        // 데이터 송신 결과를 출력하는 콜백 함수
        lrwpanDevice->GetMac()->SetMcpsDataConfirmCallback(
            MakeCallback(&RFDMcpsDataConfirm)
        );
    }

    // 한 RFD에서 코디네이터로 데이터 전송
    std::string string = "Hi there!";
    Ptr<Packet> packet = Create<Packet>((uint8_t*) string.c_str(), string.length());

    // 한 RFD의 NetDeivce
    Ptr<LrWpanNetDevice> someRfdNetDevice = DynamicCast<LrWpanNetDevice>(reducedFunctionalDevices.Get(5)->GetDevice(0));

    McpsDataRequestParams params2;
    params2.m_dstPanId = 5;
    params2.m_srcAddrMode = SHORT_ADDR;
    params2.m_dstAddrMode = SHORT_ADDR;
    params2.m_dstAddr = Mac16Address("FF:FE");
    params2.m_msduHandle = 0;
    params2.m_txOptions = TX_OPTION_NONE;  // Enable direct transmission with Ack
    Simulator::ScheduleWithContext(1,
                                Seconds(2000),
                                &LrWpanMac::McpsDataRequest,
                                someRfdNetDevice->GetMac(),
                                params2,
                                packet);

    Simulator::Stop(Seconds(5000));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
