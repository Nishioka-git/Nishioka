// client.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include "ns3/lr-wpan-fields.h"

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

void ScanConfirm(Ptr<UartLrWpanNetDevice> device, MlmeScanConfirmParams params) {
    if (params.m_resultListSize == 0) {
        std::cout << "Client: No networks found.\n";
        return;
    }

    const auto& desc = params.m_panDescList[0];
    MlmeAssociateRequestParams assoc;
    assoc.m_chNum = desc.m_logCh;
    assoc.m_chPage = desc.m_logChPage;
    assoc.m_coordAddrMode = desc.m_coorAddrMode;
    assoc.m_coordPanId = desc.m_coorPanId;
    assoc.m_coordShortAddr = desc.m_coorShortAddr;
    assoc.m_capabilityInfo = 0x80;

    device->GetMac()->MlmeAssociateRequest(assoc);
}

void AssociateConfirm(Ptr<UartLrWpanNetDevice> device, MlmeAssociateConfirmParams params) {
    std::cout << "Client: Association Confirmed, Address = " << params.m_assocShortAddr << std::endl;

    McpsDataRequestParams data;
    data.m_dstPanId = 0xCAFE;
    data.m_dstAddrMode = SHORT_ADDR;
    data.m_dstAddr = Mac16Address("00:00");
    data.m_msduHandle = 1;
    data.m_txOptions = 0;
    data.m_srcAddrMode = SHORT_ADDR;

    std::string msg = "Hello from client!";
    Ptr<Packet> packet = Create<Packet>((uint8_t*)msg.c_str(), msg.size());

    device->GetMac()->McpsDataRequest(data, packet);
}

int main() {
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    Ptr<Node> clientNode = CreateObject<Node>();
    Ptr<UartLrWpanNetDevice> clientDev = CreateObject<UartLrWpanNetDevice>("/dev/ttyUSB1");
    clientNode->AddDevice(clientDev);

    clientDev->GetMac()->SetMlmeScanConfirmCallback(MakeBoundCallback(&ScanConfirm, clientDev));
    clientDev->GetMac()->SetMlmeAssociateConfirmCallback(MakeBoundCallback(&AssociateConfirm, clientDev));

    Simulator::Schedule(Seconds(2.0), [clientDev]() {
        MlmeScanRequestParams scan;
        scan.m_scanChannels = 1 << 11;
        scan.m_scanDuration = 4;
        scan.m_scanType = MLMESCAN_ACTIVE;
        clientDev->GetMac()->MlmeScanRequest(scan);
    });

    Simulator::Stop(Seconds(30));
    Simulator::Run();
    Simulator::Destroy();
}
