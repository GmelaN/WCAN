#include <ns3/core-module.h>
#include <ns3/simulator.h>
#include <ns3/lr-wpan-module.h>
#include <ns3/network-module.h>

#include <ns3/log.h>

#include <ns3/csma-channel.h>

// channel
#include <ns3/single-model-spectrum-channel.h>

// delay model, loss model
#include <ns3/propagation-delay-model.h>
#include <ns3/propagation-loss-model.h>

// mobility model
#include <ns3/constant-position-mobility-model.h>

#include <iostream>


using namespace ns3;

// Simulator::ScheduleWithContext ID
static int eventId = 0;

/// @brief Node로부터 LrWpanNetDevice를 형변환하여 반환하는 함수
/// @param lrWpanNode 대상 노드
/// @param deviceIndex NetDevice의 인덱스
/// @return LrWpanNetDevice로 형변환된 대상 노드의 NetDevice Ptr
static Ptr<LrWpanNetDevice> getLrWpanDevice(Ptr<Node> lrWpanNode, int deviceIndex)
{
    return DynamicCast<LrWpanNetDevice>(lrWpanNode->GetDevice(deviceIndex));
}


/// @brief MCPS-DATA.request 이벤트를 등록합니다.
/// @param params McpsDataRequestParams
/// @param lrWpanMac Ptr<LrWpanMac>
/// @param packet Ptr<Packet>
/// @param delay Time
static void SetMcpsDataRequest(McpsDataRequestParams params, Ptr<LrWpanMac> lrWpanMac, std::string message, Time delay)
{
    Simulator::ScheduleWithContext(
        eventId++,
        delay,
        &LrWpanMac::McpsDataRequest,
        lrWpanMac,
        params,
        Create<Packet>((uint8_t*) message.c_str(), message.length())
    );
}


/// @brief MCPS-DATA.request params 구조체를 만듭니다.
/// @param dstAddr 목적지 MAC 주소
/// @param dstPanId 목적지 PAN ID
/// @param addrMode LrWpanAddressMode
/// @param txOption TX_OPTION_ACK, TX_OPTION_NONE: TX_OPTION_GTS, TX_OPTION_INDIRECT는 현재 미지원
/// @return McpsDatRequestParams
static McpsDataRequestParams createMcpsDataRequestParams(Mac16Address dstAddr, int dstPanId, LrWpanAddressMode addrMode, int txOption)
{
    static int msduHandle = 0;

    McpsDataRequestParams params;
    params.m_dstAddrMode = params.m_srcAddrMode = addrMode;
    params.m_dstAddr = dstAddr;
    if(dstPanId >= 0)
        params.m_dstPanId = dstPanId;
    params.m_txOptions = txOption;
    params.m_msduHandle = msduHandle++;

    return params;
}


//////////////////// CALLBACKS ////////////////////

static void McpsDataConfirm(McpsDataConfirmParams params)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << ": MCPS-DATA.confirm occured, status: " << params.m_status);
}


static void McpsDataIndication(McpsDataIndicationParams params, Ptr<Packet> p)
{
    // 패킷 데이터 추출하기
    uint8_t* buffer = new u_int8_t[p->GetSize()];       // 버퍼 배열
    p->CopyData(buffer, p->GetSize());                  // 버퍼에 패킷 내용 복사
    std::string message = std::string((char*) buffer, p->GetSize()); // 버퍼를 문자열로 변환
    delete[] buffer;                                    // 버퍼 할당 해제

    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << ": " << params.m_dstAddr << " RECEIVED " << "\"" << message << "\"" << " FROM " << params.m_srcAddr);
}

///////////////////////////////////////////////////


const int COORDINATOR_PAN_ID = 5;


int main(int argc, char* argv[])
{
    // LogComponentEnable("LrWpanMac", LOG_LEVEL_ALL);

    // Container, Helper
    NodeContainer pan;
    LrWpanHelper lrWpanHelper;

    // 노드가 10개인 PAN 네트워크 생성
    pan.Create(10);
    NetDeviceContainer netDevices = lrWpanHelper.Install(pan);
    lrWpanHelper.CreateAssociatedPan(netDevices, COORDINATOR_PAN_ID);   // 첫 번째 노드가 코디네이터, PAN ID는 5

    // 임의 노드
    Ptr<Node> someNode = pan.Get(5);
    Ptr<LrWpanNetDevice> someNodeNetDevice = getLrWpanDevice(someNode, 0);

    // 모든 노드에 callback 설정
    for(NodeContainer::Iterator nodePtr = pan.Begin(); nodePtr != pan.End(); nodePtr++)
    {
        Ptr<Node> node = *nodePtr;
        someNodeNetDevice = DynamicCast<LrWpanNetDevice>(node->GetDevice(0));

        someNodeNetDevice->GetMac()->SetMcpsDataConfirmCallback(
            MakeBoundCallback(&McpsDataConfirm)
        );

        someNodeNetDevice->GetMac()->SetMcpsDataIndicationCallback(
            MakeBoundCallback(&McpsDataIndication)
        );

        Ptr<LrWpanCsmaCa> csmaCa = someNodeNetDevice->GetCsmaCa();

        // csmaCa->SetSlottedCsmaCa();
        // csmaCa->SetMacMinBE(1);
        // csmaCa->SetMacMaxCSMABackoffs(0);
    }

    SetMcpsDataRequest(
        createMcpsDataRequestParams(Mac16Address("00:01"), -1, SHORT_ADDR, TX_OPTION_NONE),
        DynamicCast<LrWpanNetDevice>(pan.Get(4)->GetDevice(0))->GetMac(),
        "message from node 5",
        Seconds(0.1)
    );

    SetMcpsDataRequest(
        createMcpsDataRequestParams(Mac16Address("00:01"), -1, SHORT_ADDR, TX_OPTION_NONE),
        DynamicCast<LrWpanNetDevice>(pan.Get(7)->GetDevice(0))->GetMac(),
        "message from node 8",
        Seconds(0.11)
    );

    SetMcpsDataRequest(
        createMcpsDataRequestParams(Mac16Address("00:01"), -1, SHORT_ADDR, TX_OPTION_NONE),
        DynamicCast<LrWpanNetDevice>(pan.Get(1)->GetDevice(0))->GetMac(),
        "message from node 2",
        Seconds(0.12)
    );

    Simulator::Stop(Seconds(10000));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
