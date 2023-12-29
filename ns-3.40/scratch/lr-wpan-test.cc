#include <ns3/simulator.h>
#include <ns3/lr-wpan-module.h>

// channel
#include <ns3/single-model-spectrum-channel.h>

// delay model, loss model
#include <ns3/propagation-delay-model.h>
#include <ns3/propagation-loss-model.h>

// mobility model
#include <ns3/constant-position-mobility-model.h>

#include <iostream>

using namespace ns3;

/**
 * @brief 데이터 수신 완료 시 호출되는 콜백 함수
 * 
 * @param params McpsDataIndicationParams
 * @param p Ptr<Packet>
 */
static void DataIndication(McpsDataIndicationParams params, Ptr<Packet> p)
{
    // 패킷 데이터 추출하기
    uint8_t* buffer = new u_int8_t[p->GetSize()];       // 버퍼 배열
    p->CopyData(buffer, p->GetSize());                  // 버퍼에 패킷 내용 복사
    std::string string = std::string((char*) buffer, p->GetSize()); // 버퍼를 문자열로 변환
    delete[] buffer;                                    // 버퍼 할당 해제

    NS_LOG_UNCOND("Received packet of size "
        << p->GetSize()
        << "\tTime: "
        << Simulator::Now().As(Time::S)
        << "\tPacket content: \""
        << string << "\""
    );
}

/**
 * @brief 데이터 송신 성공시 호출되는 콜백 함수
 * 
 * @param params McpsDataConfirmParams
 */
static void mcpsDataConfirm(McpsDataConfirmParams params)
{
    NS_LOG_UNCOND("Data Transfer Completed." << std::endl);
}


int main(int argc, char* argv[]) {
    /* 
     * object.h의 CreateObject<T>로 인스턴스 생성하는듯
     * 생성자에 매개변수가 필요한 경우 Create<T>(args)를 사용
     * 얘는 Ptr로 받는데, Ptr은 <T>타입 스마트 포인터를 지원
    */

    // 1. Node 생성
        Ptr<Node> n0 = CreateObject<Node>();
        Ptr<Node> n1 = CreateObject<Node>();
        Ptr<Node> n2 = CreateObject<Node>();

    // 2. NetDevice 생성
        Ptr<LrWpanNetDevice> dev0 = CreateObject<LrWpanNetDevice>();
        Ptr<LrWpanNetDevice> dev1 = CreateObject<LrWpanNetDevice>();
        Ptr<LrWpanNetDevice> dev2 = CreateObject<LrWpanNetDevice>();

        // 2-1. NetDevice에 mac address 설정
            dev0->SetAddress(Mac16Address("00:01"));
            dev1->SetAddress(Mac16Address("00:02"));
            dev2->SetAddress(Mac16Address("00:03"));

        // 2-2. NetDevice에 channel 설정: single-model-spectrum-channel.h???
            // 2-2-1. SingleModelSpectrumChannel 인스턴스를 만들고
                Ptr<SingleModelSpectrumChannel> singleModelSpectrumChannel =
                    CreateObject<SingleModelSpectrumChannel>();

            // 2-2-2. LogDistancePropagationLossModel 인스턴스도 만들고
                Ptr<LogDistancePropagationLossModel> propModel = 
                    CreateObject<LogDistancePropagationLossModel>();

            // 2-2-3. ConstantSpeedPropagationDelayModel 인스턴스도 만들고
                Ptr<ConstantSpeedPropagationDelayModel> delayModel =
                    CreateObject<ConstantSpeedPropagationDelayModel>();
            
            // 2-2-4. 최종적으로 SingleModelSpectrumChannel에 LossModel, DelayModel을 설정
                singleModelSpectrumChannel->AddPropagationLossModel(propModel);
                singleModelSpectrumChannel->SetPropagationDelayModel(delayModel);

        // 완성한 Channel을 NetDevice에 설정
            dev0->SetChannel(singleModelSpectrumChannel);
            dev1->SetChannel(singleModelSpectrumChannel);
            dev2->SetChannel(singleModelSpectrumChannel);

    // 3. MobilityModel 생성 -> 2번 과정 이전에 해도 됨?????
        Ptr<ConstantPositionMobilityModel> sender0Mobility = 
            CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> sender1Mobility = 
            CreateObject<ConstantPositionMobilityModel>();
        Ptr<ConstantPositionMobilityModel> sender2Mobility = 
            CreateObject<ConstantPositionMobilityModel>();

        // 3-1. MobilityModel에 Position 설정
            sender0Mobility->SetPosition(Vector3D(0, 0, 0));
            sender1Mobility->SetPosition(Vector3D(10, 10, 0));
            sender2Mobility->SetPosition(Vector3D(0, 0, 10));

        // 3-2. NetDevice의 Phy에 Mobility 설정
            dev0->GetPhy()->SetMobility(sender0Mobility);
            dev1->GetPhy()->SetMobility(sender1Mobility);
            dev2->GetPhy()->SetMobility(sender2Mobility);
    
    // 완성한 NetDevice를 Node에 설정
        n0->AddDevice(dev0);
        n1->AddDevice(dev1);
        n2->AddDevice(dev2);

    // 4. Packet, McpsDataRequestParams 설정하기
        // - MCPS: MAC Common Part Sublayer = MAC-SAP(MAC-Service Access Point)
        //   - MAC의, APP<->MAC<->PHY 간 데이터 이동 인터페이스임
        // 4-1. Packet 설정
            std::string string = "Hello world!";
            Ptr<Packet> p_str = Create<Packet>((uint8_t*) string.c_str(), string.length());
            // Ptr<Packet> p_str2 = Create<Packet>((uint8_t*) string.c_str(), string.length());
            // CreateObject는 생성자의 인자를 받지 않음
            // Ptr<Packet> p_str = CreateObject<Packet>((uint8_t*) string.c_str(), string.length());

        // 4-2. MCPS Request Params 정의 -> 왜 Ptr 안 씀?????
            McpsDataRequestParams params; // TX 상황에서 쓰는 놈인가????
            params.m_dstPanId = 0;
            params.m_srcAddrMode = SHORT_ADDR;
            params.m_dstAddrMode = SHORT_ADDR;
            params.m_dstAddr = Mac16Address("00:03");
            params.m_msduHandle = 0;            // msduHandle은 무엇인가???
            params.m_txOptions = TX_OPTION_NONE; // 다른 모드도 있는가?????

    // 5-1. Simulator에 싣기: n0 -> n2
        Simulator::ScheduleWithContext(
            1,                                  // 노드의 ID
            Seconds(0.0),                       // 이벤트의 실행 시점(t+value)
            &LrWpanMac::McpsDataRequest,        // 실행할 이벤트
            dev0->GetMac(),                     // 나머지: McpsDataRequest의 인자로 들어감
            params,                             // 나머지: McpsDataRequest의 인자로 들어감
            p_str->Copy()                       // 나머지: McpsDataRequest의 인자로 들어감
        );


    // 5-2. Simulator에 싣기: n0 -> n1
        params.m_dstAddr = Mac16Address("00:02");
        Simulator::ScheduleWithContext(
            1,                                  // 노드의 ID
            Seconds(5),                         // 이벤트의 실행 시점(t+value)
            &LrWpanMac::McpsDataRequest,        // 실행할 이벤트
            dev0->GetMac(),                     // 나머지: McpsDataRequest의 인자로 들어감
            params,                             // 나머지: McpsDataRequest의 인자로 들어감
            p_str->Copy()                       // 나머지: McpsDataRequest의 인자로 들어감
        );

    // 기타: 채널 바꾸기
        // Ptr<LrWpanPhyPibAttributes> p = Create<LrWpanPhyPibAttributes>();
        // p->phyCurrentChannel = 25;        
        // LrWpanPibAttributeIdentifier id = phyCurrentChannel;
        // dev0->GetPhy()->PlmeSetAttributeRequest(id, p);

    // 6. Callback 붙이기
        // 6-1. MCPS-DATA.Indication: 데이터 수신 완료 시 호출
        McpsDataIndicationCallback cb = MakeCallback(&DataIndication);
        dev1->GetMac()->SetMcpsDataIndicationCallback(cb);

        McpsDataIndicationCallback cb2 = MakeCallback(&DataIndication);
        dev2->GetMac()->SetMcpsDataIndicationCallback(cb2);

        McpsDataConfirmCallback cb3 = MakeCallback(&mcpsDataConfirm);
        dev0->GetMac()->SetMcpsDataConfirmCallback(cb3);

    // 시뮬레이션은 1000초 후 종료
    Simulator::Stop(Seconds(1000));
    std::cout << Simulator::Now().As(Time::S) << ": simulation start" << std::endl;
    Simulator::Run();
    std::cout << "simulation end" << std::endl;
    Simulator::Destroy();
    return 0;
}
