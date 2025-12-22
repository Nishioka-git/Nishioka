#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include "ns3/uart-lr-wpan-net-device.h"
#include <string>

namespace ns3 {
namespace lrwpan {
class UartLrWpanNetDevice;
}
}

using namespace ns3;
using namespace ns3::lrwpan;
using namespace ns3::uart;

// Battery management functions
uint8_t ReadBatteryLevel(const std::string& filename, uint8_t defaultValue = 100);
uint8_t GetBatteryLevel(Ptr<UartLrWpanNetDevice> device);
void UpdateBatteryLevels();

#endif // BATTERY_MANAGER_H
