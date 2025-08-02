#ifndef CONTROLLERCONFIG_H
#define CONTROLLERCONFIG_H
#include <string>
#include <Arduino.h>

struct BasicConfig
{
    String deviceName;
    String fwVersion;
    String model;
    String sn;
    String uuid;

    // Default constructor
    BasicConfig() : deviceName("MTech Switch Actuator 6Ch Rev 1.x"), 
                    fwVersion("1.0.0"), 
                    model("MT_SA_6CH_REV_1X"), 
                    sn("123456"), 
                    uuid("00000000-0000-0000-0000-000000000001") {}
    
    // Constructor with arguments
    BasicConfig(String deviceName, String fwVersion, String model, String sn, String uuid)
        : deviceName(deviceName), fwVersion(fwVersion), model(model), sn(sn), uuid(uuid)
    {
    }
};

struct NetworkConfig
{
    String wifiSSID;
    String wifiPassword;
    bool staticIP;
    String ipAddress;
    String subnet;
    String gateway;
    String dns;

    // Default constructor
    NetworkConfig() : wifiSSID("MTech_SA_6CH"), 
                      wifiPassword("password"), 
                      staticIP(false), 
                      ipAddress("192.168.1.100"), 
                      subnet("255.255.255.0"), 
                      gateway("192.168.1.1"), 
                      dns(".8.8.8") {}
    
    // Constructor with arguments
    NetworkConfig(String wifiSSID, String wifiPassword, bool staticIP, String ipAddress, String subnet,
                  String gateway, String dns)
        : wifiSSID(wifiSSID), wifiPassword(wifiPassword), staticIP(staticIP), ipAddress(ipAddress), subnet(subnet), gateway(gateway),
          dns(dns)
    {
    }
};

// Enum to represent Modbus types
enum class ModbusType
{
    RTU,
    TCP
};

// Structure for Modbus RTU configuration
struct ModbusRTUConfig
{
    String serialPort;
    uint32_t baudRate;
    char parity; // 'N', 'E', 'O' for None, Even, Odd
    uint8_t dataBits;
    uint8_t stopBits;
    uint32_t timeout; // Timeout in milliseconds
    uint32_t responseTimeout; // Response timeout in milliseconds
    uint32_t retryCount; // Number of retries for communication

    // Default constructor
    ModbusRTUConfig() : serialPort(""), baudRate(115200), parity('N'), dataBits(8), stopBits(1), timeout(5000), responseTimeout(2000), retryCount(3) {}

    // Constructor with arguments
    ModbusRTUConfig(String serialPort, uint32_t baudRate, char parity, uint8_t dataBits, uint8_t stopBits,
                    uint32_t timeout, uint32_t responseTimeout, uint32_t retryCount)
        : serialPort(serialPort), baudRate(baudRate), parity(parity), dataBits(dataBits), stopBits(stopBits),
          timeout(timeout), responseTimeout(responseTimeout), retryCount(retryCount)
    {
    }
};

// Structure for Modbus TCP configuration
struct ModbusTCPConfig
{
    String ipAddress;
    uint16_t port;
    uint32_t timeout; // Timeout in milliseconds
    uint32_t responseTimeout; // Response timeout in milliseconds
    uint32_t retryCount; // Number of retries for communication

    // Default constructor
    ModbusTCPConfig() : ipAddress("127.0.0.1"), port(502), timeout(5000), responseTimeout(2000), retryCount(3) {}

    // Constructor with arguments
    ModbusTCPConfig(String ipAddress, uint16_t port, uint32_t timeout, uint32_t responseTimeout, uint32_t retryCount)
        : ipAddress(ipAddress), port(port), timeout(timeout), responseTimeout(responseTimeout), retryCount(retryCount)
    {
    }
};

// Main Modbus configuration structure
struct ModbusConfig
{
    ModbusType type;
    uint8_t slaveId;

    ModbusRTUConfig rtu;
    ModbusTCPConfig tcp;

    // Default constructor
    ModbusConfig() : type(ModbusType::TCP), slaveId(1) {};

    // Constructor with arguments
    ModbusConfig(ModbusType type, uint8_t slaveId, ModbusRTUConfig rtu, ModbusTCPConfig tcp)
        : type(type), slaveId(slaveId), rtu(rtu), tcp(tcp)
    {
    }
};

struct ControllerConfig
{
    BasicConfig basicConf;
    NetworkConfig networkConf;

    // Parameterized constructor
    ControllerConfig(const BasicConfig &basic, const NetworkConfig &network) : basicConf(basic), networkConf(network)
    {
    }

    // Default constructor (added)
    ControllerConfig() = default;
};

// Controller configuration can be added if needed

// Factory configurations for different models
const ControllerConfig MT_SA_6CH_REV_1X = 
{
    BasicConfig {"MTech Switch Actuator 6Ch Rev 1.x",
                 "1.0.0",
                 "MT_SA_6CH_REV_1X",
                 "123456",
                 "00000000-0000-0000-0000-000000000001"},
    NetworkConfig {"MTech_SA_6CH",
                   "password",
                   false,
                   "192.168.1.100",
                   "255.255.255.0",
                   "192.168.1.1",
                   "8.8.8.8"}
};

const ControllerConfig MT_SA_8CH_REV_1X = 
{
    BasicConfig {"MTech Switch Actuator 8Ch Rev 1.x",
                 "1.0.0",
                 "MT_SA_8CH_REV_1X",
                 "654321",
                 "00000000-0000-0000-0000-000000000002"},
    NetworkConfig {"MTech_SA_8CH",
                   "password",
                   false,
                   "192.168.1.100",
                   "255.255.255.0",
                   "192.168.1.1",
                   "8.8.8.8"}
};

#endif // CONTROLLERCONFIG_H