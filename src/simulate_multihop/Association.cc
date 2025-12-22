#include "Association.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uart-lr-wpan-net-device.h"
#include <iostream>

// Global variables for association management
static uint16_t g_nextShortAddr = 0x0002;
static uint16_t g_nextShortAddrDev01 = 0x0003;
static Mac16Address g_dev01ShortAddr = Mac16Address("00:02");
static uint16_t g_totalDevicesInPAN = 3;
static uint16_t g_associatedDeviceCount = 0;

// External device pointers (will be set in main)
extern Ptr<UartLrWpanNetDevice> g_coordinatorDevice;
extern Ptr<UartLrWpanNetDevice> g_uartNetDevice1;
extern Ptr<UartLrWpanNetDevice> g_uartNetDevice2;
extern Mac16Address g_dev01Addr;
extern Mac16Address g_dev02Addr;

Mac16Address AssignShortAddress(uint16_t& counter) {
    return Mac16Address::ConvertFrom(Mac16Address(counter++));
}

void AssociateIndication(Ptr<UartLrWpanNetDevice> device, MlmeAssociateIndicationParams params) {
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC IND] Node " << device->GetNode()->GetId()
              << " received association request from device with capability "
              << std::hex << static_cast<uint32_t>(params.capabilityInfo) << std::dec
              << " | Dev Addr: " << params.m_extDevAddr << std::endl;
    std::cout << "Sending Association Response..." << std::endl;

    Mac16Address assignedAddr = AssignShortAddress(g_nextShortAddr);

    MlmeAssociateResponseParams respParams;
    respParams.m_assocShortAddr = assignedAddr;
    respParams.m_extDevAddr = params.m_extDevAddr;
    respParams.m_status = MacStatus::SUCCESS;
    device->GetMac()->MlmeAssociateResponse(respParams);

    g_associatedDeviceCount++;
    std::cout << "Assigned short address: " << assignedAddr
              << " | Total associated devices: " << g_associatedDeviceCount
              << "/" << g_totalDevicesInPAN << std::endl;
}

void AssociateConfirm(Ptr<UartLrWpanNetDevice> device, MlmeAssociateConfirmParams params) {
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC CONFIRM] Node " << device->GetNode()->GetId()
              << ", Associate Confirm: Status " << static_cast<uint32_t>(params.m_status)
              << " | Address: " << params.m_assocShortAddr << std::endl;

    if (device == g_uartNetDevice1 && params.m_status == MacStatus::SUCCESS) {
        g_dev01ShortAddr = params.m_assocShortAddr;
        g_dev01Addr = params.m_assocShortAddr;

        std::cout << "\n========== dev01 Association Complete ==========\n";
        std::cout << "dev01 will use RREQ for route discovery\n";
        std::cout << "===============================================\n\n";

        // Schedule dev02 association
        Simulator::Schedule(Seconds(0.5), [=]() {
            MlmeAssociateRequestParams associateParams;
            associateParams.m_chNum = 0xD;
            associateParams.m_chPage = 0;
            associateParams.m_coordAddrMode = SHORT_ADDR;
            associateParams.m_coordPanId = 0xCAFE;
            associateParams.m_capabilityInfo = 0x80;
            associateParams.m_coordShortAddr = g_dev01ShortAddr;
            g_uartNetDevice2->GetMac()->MlmeAssociateRequest(associateParams);
        });
    }

    if (params.m_status == MacStatus::SUCCESS && device == g_uartNetDevice2) {
        g_dev02Addr = params.m_assocShortAddr;

        std::cout << "\n========== dev02 Association Complete ==========\n";
        std::cout << "All devices associated. Starting route discovery...\n";
        std::cout << "===============================================\n\n";

        // Call route discovery (defined in main)
        Simulator::Schedule(Seconds(0.2), [=]() {
            // Route discovery will be initiated from main
        });
    }
}

void AssociateIndicationDev01(Ptr<UartLrWpanNetDevice> device, MlmeAssociateIndicationParams params) {
    std::cout << Simulator::Now().As(Time::S) << " [ASSOC IND] Node " << device->GetNode()->GetId()
              << " received association request from device with capability "
              << std::hex << static_cast<uint32_t>(params.capabilityInfo) << std::dec
              << " | Dev Addr: " << params.m_extDevAddr << std::endl;
    std::cout << "Sending Association Response (dev01)..." << std::endl;

    Mac16Address assignedAddr = AssignShortAddress(g_nextShortAddrDev01);

    MlmeAssociateResponseParams respParams;
    respParams.m_assocShortAddr = assignedAddr;
    respParams.m_extDevAddr = params.m_extDevAddr;
    respParams.m_status = MacStatus::SUCCESS;
    device->GetMac()->MlmeAssociateResponse(respParams);

    g_associatedDeviceCount++;
    std::cout << "Assigned short address (dev01): " << assignedAddr
              << " | Total associated devices: " << g_associatedDeviceCount
              << "/" << g_totalDevicesInPAN << std::endl;
}
