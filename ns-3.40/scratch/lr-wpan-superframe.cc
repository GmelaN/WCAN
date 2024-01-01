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

/// @brief MLME-SCAN.request에 대한 MLME-SCAN.confirm 콜백 함수,
/// 찾은 비콘 신호 중 가장 강한 신호를 보내는 코디네이터에게 MLME-ASSOCIATE.request를 전송합니다.
/// SHORT_ADDR 모드만 지원함
/// @param device Ptr<LrWpanNetDevice>
/// @param params MlmeScanConfirmParams
static void
MlmeScanConfirm(Ptr<LrWpanNetDevice> device, MlmeScanConfirmParams params)
{
    std::cout
        << Simulator::Now().As(Time::S)
        << ": device "
        << device->GetMac()->GetShortAddress()
        << " issued MLME-SCAN.confirm, scan type: "
        << params.m_scanType
    ;

    // 스캔이 정상적으로 실행되지 못한 경우
    if(params.m_status != MLMESCAN_SUCCESS && params.m_status != MLMESCAN_NO_BEACON)
    {
        std::cout
            << ", with ERROR code: "
            << params.m_status
            << std::endl
        ;
        return;
    }

    // 스캔은 정상적으로 실행되었으나 비콘 신호를 찾지 못한 경우
    if(params.m_status == MLMESCAN_NO_BEACON || params.m_panDescList.empty())
    {
        std::cout
            << ", BEACON_NOT_FOUND"
            << std::endl
        ;
        return;
    }

    // 탐지한 비콘 신호 중 가장 품질이 좋은 비콘 신호에 연결

    // 1. 가장 품질이 좋은 비콘 신호를 선택
    int maxLqi = 0, maxLqiPanDescIdx = 0;
    for(uint32_t i = 0; i < params.m_panDescList.size(); i++)
    {
        if(params.m_panDescList[i].m_linkQuality > maxLqi)
        {
            maxLqi = params.m_panDescList[i].m_linkQuality;
            maxLqiPanDescIdx = i;
        }
    }
    PanDescriptor targetCoordinator = params.m_panDescList[maxLqiPanDescIdx];

    // 2. 타깃 PAN 코디네이터에 연결 요청(MLME-ASSOCIATE.request)을 보냄
    MlmeAssociateRequestParams mlmeAssociateRequestParams;
    mlmeAssociateRequestParams.m_chNum = targetCoordinator.m_logCh;
    mlmeAssociateRequestParams.m_chPage = targetCoordinator.m_logChPage;
    mlmeAssociateRequestParams.m_coordPanId = targetCoordinator.m_coorPanId;
    mlmeAssociateRequestParams.m_capabilityInfo.SetShortAddrAllocOn(true);
    mlmeAssociateRequestParams.m_coordShortAddr = targetCoordinator.m_coorShortAddr;
    mlmeAssociateRequestParams.m_coordAddrMode = targetCoordinator.m_coorAddrMode;

    std::cout
        << ", selected PAN ID "
        << targetCoordinator.m_coorPanId
        << ", scheduling MLME-ASSOCIATE.request"
        << std::endl
    ;

    Simulator::ScheduleNow(
        &LrWpanMac::MlmeAssociateRequest,
        device->GetMac(),
        mlmeAssociateRequestParams
    );
}


/// @brief 디바이스의 MLME-ASSOCIATE.request에 대한 MLME-ASSOCIATE.confirm 콜백 함수
/// @param device Ptr<LrWpanNetDevice>
/// @param params MlmeAssociateConfirmParams
static void
MlmeAssociateConfirm(Ptr<LrWpanNetDevice> device, MlmeAssociateConfirmParams params)
{
    std::cout
        << Simulator::Now().As(Time::S)
        << ": device "
        << device->GetMac()->GetShortAddress()
        << " issued MLME-ASSOCIATE.confirm"
    ;

    if(params.m_status == MLMEASSOC_SUCCESS) {
        std::cout
            << ": SUCCESS"
            << std::endl
        ;
        return;
    }

    std::cout
        << ", with ERROR code "
        << params.m_status
        << std::endl
    ;
}


/// @brief 디바이스의 MLME-ASSOCIATE.request에 대한 코디네이터의 MLME-ASSOCIATE.indication 콜백 함수
/// @param device Ptr<LrWpanNetDevice>
/// @param params MlmeAssociateIndicationParams
static void
MlmeAssociateIndication(Ptr<LrWpanNetDevice> device, MlmeAssociateIndicationParams params)
{
    static uint rawAddr = 2;

    std::cout
        << Simulator::Now().GetSeconds()
        << ": device "
        << device->GetMac()->GetShortAddress()
        << " issued MLME-ASSOCIATE.confirm"
    ;

    MlmeAssociateResponseParams assocRespParams;
    assocRespParams.m_extDevAddr = params.m_extDevAddr;
    assocRespParams.m_status = LrWpanAssociationStatus::ASSOCIATED;

    assocRespParams.m_assocShortAddr = Mac16Address(rawAddr++);

    // 코디네이터는 설정을 마치고 디바이스에게 응답을 보냄
    std::cout
        << ", scheduling MLME-ASSOCIATE.response"
        << std::endl
    ;

    Simulator::ScheduleNow(&LrWpanMac::MlmeAssociateResponse, device->GetMac(), assocRespParams);
}


/// @brief 디바이스의 MCPS-DATA.request에 대한 디바이스의 MCPS-DATA.confirm 콜백 함수
/// @param device Ptr<LrWpanNetDevice>
/// @param params McpsDataConfirmParams
static void 
McpsDataConfirm(Ptr<LrWpanNetDevice> device, McpsDataConfirmParams params)
{
    std::cout
        << Simulator::Now().As(Time::S)
        << ": device "
        << device->GetMac()->GetShortAddress()
        << " issued MCPS-DATA.confirm"
    ;

    if(params.m_status == IEEE_802_15_4_SUCCESS)
    {
        std::cout
            << ": SUCCESS"
            << std::endl
        ;
        return;
    }

    std::cout
        << " : ERROR code "
        << params.m_status
        << std::endl
    ;
}


/// @brief 전송자의 MCPS-DATA.request로 전송된 데이터가 수신자에게 도착했을 떄 발생하는 MCPS-DATA.Indication에 대한 콜백 함수
/// @param params McpsDataIndicationParams
/// @param p Ptr<Packet>
static void
McpsDataIndication(Ptr<LrWpanNetDevice> device, McpsDataIndicationParams params, Ptr<Packet> p)
{
    // 패킷 데이터를 까서 문자열로 변환
    uint8_t* buffer = new uint8_t[p->GetSize() + 1];
    p->CopyData(buffer, p->GetSize());
    std::string packetBody = std::string((char*) buffer, p->GetSize());
    delete[] buffer;

    // 받은 문자열을 출력
    std::cout
        << Simulator::Now().As(Time::S)
        << ": device "
        << device->GetMac()->GetShortAddress()
        << "received message \""
        << packetBody
        << "\""
        << std::endl
    ;
}


/// @brief MLME-COMM-STATUS.indication 콜백 함수
/// @param device Ptr<LrWpanNetDevice>
/// @param params MlmeCommStatusIndicationParams
static void
CommStatusIndication(Ptr<LrWpanNetDevice> device, MlmeCommStatusIndicationParams params)
{
    // Used by coordinator higher layer to inform results of a
    // association procedure from its mac layer.This is implemented by other protocol stacks
    // and is only here for demonstration purposes.
    switch (params.m_status)
    {
        case LrWpanMlmeCommStatus::MLMECOMMSTATUS_SUCCESS:
            std::cout << Simulator::Now().As(Time::S) << " Coordinator " << device->GetNode()->GetId() << " "
                << device->GetMac()->GetShortAddress()
                << " MLME-comm-status.indication: SUCCESS\n";
            break;

        case LrWpanMlmeCommStatus::MLMECOMMSTATUS_TRANSACTION_EXPIRED:
            std::cout << Simulator::Now().As(Time::S) << " Coordinator " << device->GetNode()->GetId() << " "
                    << device->GetMac()->GetShortAddress()
                    << " MLME-comm-status.indication: Transaction for device " << params.m_dstExtAddr
                    << " EXPIRED in pending transaction list\n";
            break;
        case LrWpanMlmeCommStatus::MLMECOMMSTATUS_NO_ACK:
            std::cout << Simulator::Now().As(Time::S) << " Coordinator " << device->GetNode()->GetId() << " "
                    << device->GetMac()->GetShortAddress()
                    << " MLME-comm-status.indication: NO ACK from " << params.m_dstExtAddr
                    << " device registered in the pending transaction list\n";
            break;

        case LrWpanMlmeCommStatus::MLMECOMMSTATUS_CHANNEL_ACCESS_FAILURE:
            std::cout << Simulator::Now().As(Time::S) << " Coordinator " << device->GetNode()->GetId() << " "
                    << device->GetMac()->GetShortAddress()
                    << " MLME-comm-status.indication: CHANNEL ACCESS problem in transaction for "
                    << params.m_dstExtAddr << " registered in the pending transaction list\n";
            break;

        default:
            break;
    }
}

//////////////////////////////////////////////////////////////////


/// @brief LR-WPAN 노드 디바이스를 생성합니다. 디바이스의 MAC 주소는 00:02부터 오름차순으로 할당됩니다.
/// @param nodeCount 코디네이터 1개를 제외한 디바이스의 수
/// @return 생성된 디바이스를 담은 NodeContainer
NodeContainer createNodes(uint32_t nodeCount)
{
    NodeContainer nodes;
    MobilityHelper mobilityHelper;
    LrWpanHelper lrWpanHelper;              // channel은 디폴트로

    nodes.Create(nodeCount);

    mobilityHelper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityHelper.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(-30.0),
                                  "MinY",
                                  DoubleValue(-30.0),
                                  "DeltaX",
                                  DoubleValue(30.0),
                                  "DeltaY",
                                  DoubleValue(30.0),
                                  "GridWidth",
                                  UintegerValue(20),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobilityHelper.Install(nodes);

    NetDeviceContainer netDevices = lrWpanHelper.Install(nodes);

    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    Ptr<LogDistancePropagationLossModel> propModel =
        CreateObject<LogDistancePropagationLossModel>();
    Ptr<ConstantSpeedPropagationDelayModel> delayModel =
        CreateObject<ConstantSpeedPropagationDelayModel>();
    channel->AddPropagationLossModel(propModel);
    channel->SetPropagationDelayModel(delayModel);

    lrWpanHelper.SetChannel(channel);



    uint16_t rawAddr = 2;
    for(NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); i++)
    {
        Ptr<Node> node = *i;
        Ptr<LrWpanNetDevice> device = DynamicCast<LrWpanNetDevice>(node->GetDevice(0));
        Mac16Address addr = Mac16Address(rawAddr++);
        device->SetAddress(addr);
    }
    return nodes;
}


int main(int argc, char *argv[]) {
    const int COORDINATOR_PAN_ID = 5;
    const int COORDINATOR_CHANNEL = 12;
    const char* COORDINATOR_MAC_ADDR = "CA:FE";


    // 디바이스 생성
    NodeContainer devices = createNodes(5);
    Ptr<Node> coordinator = CreateObject<Node>();
    Ptr<LrWpanNetDevice> coordinatorNetDevice = CreateObject<LrWpanNetDevice>();
    coordinator->AddDevice(coordinatorNetDevice);

    // 코디네이터 설정
    Ptr<ConstantPositionMobilityModel> mobilityModel = CreateObject<ConstantPositionMobilityModel>();
    mobilityModel->SetPosition(Vector3D(0, 0, 0));
    coordinatorNetDevice->GetPhy()->SetMobility(mobilityModel);
    coordinatorNetDevice->SetChannel(DynamicCast<SpectrumChannel>(devices.Get(0)->GetDevice(0)->GetChannel()));
    coordinatorNetDevice->GetMac()->SetShortAddress(Mac16Address(COORDINATOR_MAC_ADDR));

    coordinatorNetDevice->GetMac()->SetMlmeAssociateIndicationCallback(
        MakeBoundCallback(&MlmeAssociateIndication, coordinatorNetDevice));
    coordinatorNetDevice->GetMac()->SetMlmeCommStatusIndicationCallback(
        MakeBoundCallback(&CommStatusIndication, coordinatorNetDevice));
    coordinatorNetDevice->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&McpsDataIndication, coordinatorNetDevice));

    MlmeStartRequestParams params;
    params.m_panCoor = true;
    params.m_PanId = COORDINATOR_PAN_ID;
    params.m_bcnOrd = 14;
    params.m_sfrmOrd = 6;
    params.m_logCh = COORDINATOR_CHANNEL;
    params.m_coorRealgn = false;

    // 다른 디바이스에 PAN의 존재를 알리기 위해, MLME-START.request로 비콘 신호 전송 시작
    Simulator::ScheduleWithContext(coordinator->GetId(),
                                   Seconds(0.0),
                                   &LrWpanMac::MlmeStartRequest,
                                   coordinatorNetDevice->GetMac(),
                                   params);

    // 다른 디바이스 설정
    for(NodeContainer::Iterator i = devices.Begin(); i != devices.End(); i++) {
        Ptr<Node> node = *i;
        Ptr<LrWpanNetDevice> netDevice = DynamicCast<LrWpanNetDevice>(node->GetDevice(0));

        // 스캔 확인 콜백, 코디네이터와의 연결 확인 콜백, 데이터 수신 콜백 설정
        netDevice->GetMac()->SetMlmeScanConfirmCallback(
            MakeBoundCallback(&MlmeScanConfirm, netDevice));
        netDevice->GetMac()->SetMlmeAssociateConfirmCallback(
            MakeBoundCallback(&MlmeAssociateConfirm, netDevice));
        netDevice->GetMac()->SetMcpsDataIndicationCallback(
        MakeBoundCallback(&McpsDataIndication, netDevice));


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
        scanParams.m_scanType = MLMESCAN_ACTIVE;

        // We start the scanning process 100 milliseconds apart for each device
        // to avoid a storm of association requests with the coordinators
        // 디바이스 개별 스캔 시작
        Time jitter = Seconds(2) + MilliSeconds(std::distance(devices.Begin() + 1, i) * 100);
        Simulator::ScheduleWithContext(node->GetId(),
                                       jitter,
                                       &LrWpanMac::MlmeScanRequest,
                                       netDevice->GetMac(),
                                       scanParams);

        // 데이터 송신 결과를 출력하는 콜백 함수
        netDevice->GetMac()->SetMcpsDataConfirmCallback(
            MakeBoundCallback(&McpsDataConfirm, netDevice)
        );
    }


    // 한 디바이스에서 코디네이터로 데이터 전송
    std::string string = "Hi there!";
    Ptr<Packet> packet = Create<Packet>((uint8_t*) string.c_str(), string.length());

    // 한 NetDeivce
    Ptr<LrWpanNetDevice> someNetDevice = DynamicCast<LrWpanNetDevice>(devices.Get(0)->GetDevice(0));

    McpsDataRequestParams params2;
    params2.m_dstPanId = COORDINATOR_PAN_ID;
    params2.m_srcAddrMode = SHORT_ADDR;
    params2.m_dstAddrMode = SHORT_ADDR;
    params2.m_dstAddr = Mac16Address(COORDINATOR_MAC_ADDR);
    params2.m_msduHandle = 1;
    params2.m_txOptions = TX_OPTION_ACK;  // Enable direct transmission with Ack
    Simulator::Schedule(
        Seconds(1500),
        &LrWpanMac::McpsDataRequest,
        someNetDevice->GetMac(),
        params2,
        packet
    );

    Simulator::Stop(Seconds(100000));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
