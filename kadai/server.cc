// server.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include "ns3/lr-wpan-fields.h"

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

void StartConfirm(Ptr<UartLrWpanNetDevice> device, MlmeStartConfirmParams params) {
    std::cout << Simulator::Now().As(Time::S) << " Server: StartConfirm: Status "
              << static_cast<uint32_t>(params.m_status) << std::endl;
}

void AssociateIndication(Ptr<UartLrWpanNetDevice> device, MlmeAssociateIndicationParams params) {
    std::cout << "Server: Association requested from " << params.m_extDevAddr << std::endl;

    MlmeAssociateResponseParams resp;
    resp.m_extDevAddr = params.m_extDevAddr;
    resp.m_status = MacStatus::SUCCESS;
    resp.m_assocShortAddr = Mac16Address("AB:CD");

    device->GetMac()->MlmeAssociateResponse(resp);
}

void DataIndication(Ptr<UartLrWpanNetDevice> device, McpsDataIndicationParams params, Ptr<Packet> p) {
    std::vector<uint8_t> buffer(p->GetSize());
    p->CopyData(buffer.data(), buffer.size());
    std::string data(buffer.begin(), buffer.end());

    std::cout << "Server received data: " << data << std::endl;
}

int main() {
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    Ptr<Node> serverNode = CreateObject<Node>();
    Ptr<UartLrWpanNetDevice> serverDev = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB0");
    serverNode->AddDevice(serverDev);

    serverDev->GetMac()->SetMlmeStartConfirmCallback(MakeBoundCallback(&StartConfirm, serverDev));
    serverDev->GetMac()->SetMlmeAssociateIndicationCallback(MakeBoundCallback(&AssociateIndication, serverDev));
    serverDev->GetMac()->SetMcpsDataIndicationCallback(MakeBoundCallback(&DataIndication, serverDev));

    // セットアップ
    Ptr<MacPibAttributes> chAttr = Create<MacPibAttributes>();
    chAttr->pCurrentChannel = 11;
    serverDev->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::pCurrentChannel, chAttr);

    Ptr<MacPibAttributes> addrAttr = Create<MacPibAttributes>();
    addrAttr->macShortAddress = Mac16Address("00:00");
    serverDev->GetMac()->MlmeSetRequest(MacPibAttributeIdentifier::macShortAddress, addrAttr);

    MlmeStartRequestParams startParams;
    startParams.m_PanId = 0xCAFE;
    startParams.m_logCh = 11;
    startParams.m_bcnOrd = 15;
    startParams.m_sfrmOrd = 15;
    startParams.m_panCoor = true;
    serverDev->GetMac()->MlmeStartRequest(startParams);

    Simulator::Stop(Seconds(30));
    Simulator::Run();
    Simulator::Destroy();
}
