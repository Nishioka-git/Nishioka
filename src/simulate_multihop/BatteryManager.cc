#include "BatteryManager.h"
#include "ns3/simulator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>

// External device pointers
extern Ptr<UartLrWpanNetDevice> g_coordinatorDevice;
extern Ptr<UartLrWpanNetDevice> g_uartNetDevice1;
extern Ptr<UartLrWpanNetDevice> g_uartNetDevice2;

// Battery levels
extern uint8_t g_batteryLevelCoordinator;
extern uint8_t g_batteryLevelDev01;
extern uint8_t g_batteryLevelDev02;

uint8_t ReadBatteryLevel(const std::string& filename, uint8_t defaultValue) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "[BATTERY] Warning: Could not open " << filename
                  << ", using default value: " << (int)defaultValue << "%" << std::endl;
        return defaultValue;
    }

    std::string line;
    if (std::getline(file, line)) {
        std::stringstream ss(line);
        uint32_t hexValue;

        if (line.find("0x") == 0 || line.find("0X") == 0) {
            ss >> std::hex >> hexValue;
        } else {
            ss >> std::hex >> hexValue;
        }

        if (ss.fail()) {
            std::cout << "[BATTERY] Warning: Failed to parse battery value from "
                      << filename << ", using default: " << (int)defaultValue << "%" << std::endl;
            return defaultValue;
        }

        uint8_t percentage = static_cast<uint8_t>(hexValue);
        if (percentage > 100) {
            std::cout << "[BATTERY] Warning: Battery value " << (int)percentage
                      << "% is out of range, capping to 100%" << std::endl;
            percentage = 100;
        }

        std::cout << "[BATTERY] Read from " << filename
                  << ": 0x" << std::hex << hexValue << std::dec
                  << " = " << (int)percentage << "%" << std::endl;

        file.close();
        return percentage;
    }

    file.close();
    std::cout << "[BATTERY] Warning: Empty file " << filename
              << ", using default: " << (int)defaultValue << "%" << std::endl;
    return defaultValue;
}

uint8_t GetBatteryLevel(Ptr<UartLrWpanNetDevice> device) {
    if (device == g_coordinatorDevice) {
        return g_batteryLevelCoordinator;
    } else if (device == g_uartNetDevice1) {
        return g_batteryLevelDev01;
    } else if (device == g_uartNetDevice2) {
        return g_batteryLevelDev02;
    }
    return 100; // Default value
}

void UpdateBatteryLevels() {
    std::cout << "\n========== Updating Battery Levels ==========\n";
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << "Current working directory: " << cwd << std::endl;
    }

    const std::string batteryFilePath = "/home/yuugo/ns-3-dev/scratch/battery.txt";
    g_batteryLevelCoordinator = ReadBatteryLevel(batteryFilePath, g_batteryLevelCoordinator);
    g_batteryLevelDev01 = ReadBatteryLevel(batteryFilePath, g_batteryLevelDev01);
    g_batteryLevelDev02 = ReadBatteryLevel(batteryFilePath, g_batteryLevelDev02);

    std::cout << "Coordinator Battery: " << (int)g_batteryLevelCoordinator << "%\n";
    std::cout << "Dev01 Battery: " << (int)g_batteryLevelDev01 << "%\n";
    std::cout << "Dev02 Battery: " << (int)g_batteryLevelDev02 << "%\n";
    std::cout << "=============================================\n\n";

    Simulator::Schedule(Seconds(50.0), &UpdateBatteryLevels);
}
