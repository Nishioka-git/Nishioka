#ifndef ASSOCIATION_H
#define ASSOCIATION_H

#include "ns3/lr-wpan-fields.h"
#include <string>

namespace ns3 {
namespace lrwpan {
class UartLrWpanNetDevice;
}
}

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

// Association-related structures and functions
struct AssociationInfo {
    Mac16Address deviceAddr;
    uint16_t shortAddr;
    uint8_t batteryLevel;
};

// Association callback declarations
void AssociateIndication(Ptr<UartLrWpanNetDevice> device, MlmeAssociateIndicationParams params);
void AssociateConfirm(Ptr<UartLrWpanNetDevice> device, MlmeAssociateConfirmParams params);
void AssociateIndicationDev01(Ptr<UartLrWpanNetDevice> device, MlmeAssociateIndicationParams params);

// Association management
Mac16Address AssignShortAddress(uint16_t& counter);
void UpdateAssociatedDeviceCount();

#endif // ASSOCIATION_H
