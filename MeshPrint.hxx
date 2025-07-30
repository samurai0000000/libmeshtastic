/*
 * MeshPrint.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHPRINT_HXX
#define MESHPRINT_HXX

#include <ostream>
#include <libmeshtastic.h>

using namespace std;

extern ostream &operator<<(ostream &os, const meshtastic_MeshPacket &p);
extern ostream &operator<<(ostream &os, const meshtastic_Data &d);
extern ostream &operator<<(ostream &os, const meshtastic_PortNum &p);
extern ostream &operator<<(ostream &os, const meshtastic_Telemetry &t);
extern ostream &operator<<(ostream &os, const meshtastic_DeviceMetrics &m);
extern ostream &operator<<(ostream &os, const meshtastic_EnvironmentMetrics &m);
extern ostream &operator<<(ostream &os, const meshtastic_AirQualityMetrics &m);
extern ostream &operator<<(ostream &os, const meshtastic_PowerMetrics &m);
extern ostream &operator<<(ostream &os, const meshtastic_LocalStats &m);
extern ostream &operator<<(ostream &os, const meshtastic_HealthMetrics &m);
extern ostream &operator<<(ostream &os, const meshtastic_HostMetrics &m);
extern ostream &operator<<(ostream &os, const meshtastic_MyNodeInfo &m);
extern ostream &operator<<(ostream &os, const meshtastic_NodeInfo &i);
extern ostream &operator<<(ostream &os, const meshtastic_User &u);
extern ostream &operator<<(ostream &os, const meshtastic_Position &p);
extern ostream &operator<<(ostream &os, const meshtastic_Routing &r);
extern ostream &operator<<(ostream &os, const meshtastic_AdminMessage &m);
extern ostream &operator<<(ostream &os, const meshtastic_AdminMessage_ConfigType &t);
extern ostream &operator<<(ostream &os, const meshtastic_AdminMessage_ModuleConfigType &t);
extern ostream &operator<<(ostream &os, const meshtastic_DeviceConnectionStatus &s);
extern ostream &operator<<(ostream &os, const meshtastic_WifiConnectionStatus &s);
extern ostream &operator<<(ostream &os, const meshtastic_EthernetConnectionStatus &s);
extern ostream &operator<<(ostream &os, const meshtastic_BluetoothConnectionStatus &s);
extern ostream &operator<<(ostream &os, const meshtastic_SerialConnectionStatus &s);
extern ostream &operator<<(ostream &os, const meshtastic_NetworkConnectionStatus &s);
extern ostream &operator<<(ostream &os, const meshtastic_HamParameters &p);
extern ostream &operator<<(ostream &os, const meshtastic_NodeRemoteHardwarePinsResponse &p);
extern ostream &operator<<(ostream &os, const meshtastic_NodeRemoteHardwarePin &p);
extern ostream &operator<<(ostream &os, const meshtastic_RemoteHardwarePin &p);
extern ostream &operator<<(ostream &os, const meshtastic_RemoteHardwarePinType &t);
extern ostream &operator<<(ostream &os, const meshtastic_RouteDiscovery &r);
extern ostream &operator<<(ostream &os, const meshtastic_Config &c);
extern ostream &operator<<(ostream &os, const meshtastic_Config_DeviceConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_Config_PositionConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_Config_PowerConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_Config_NetworkConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_Config_DisplayConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_Config_LoRaConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_Config_BluetoothConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_Config_SecurityConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_Config_SessionkeyConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_MQTTConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_SerialConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_ExternalNotificationConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_StoreForwardConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_RangeTestConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_TelemetryConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_CannedMessageConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_AudioConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_RemoteHardwareConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_NeighborInfoConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_AmbientLightingConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_DetectionSensorConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_PaxcounterConfig &c);
extern ostream &operator<<(ostream &os, const meshtastic_Channel &c);
extern ostream &operator<<(ostream &os, const meshtastic_ChannelSettings &s);
extern ostream &operator<<(ostream &os, const meshtastic_ModuleSettings &s);
extern ostream &operator<<(ostream &os, const meshtastic_QueueStatus &q);
extern ostream &operator<<(ostream &os, const meshtastic_DeviceMetadata &m);
extern ostream &operator<<(ostream &os, const meshtastic_FileInfo &i);
extern ostream &operator<<(ostream &os, const meshtastic_DeviceUIConfig &u);
extern ostream &operator<<(ostream &os, const meshtastic_MqttClientProxyMessage &m);

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
