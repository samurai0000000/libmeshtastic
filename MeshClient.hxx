/*
 * MeshClient.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHCLIENT_HXX
#define MESHCLIENT_HXX

#include <string>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <libmeshtastic.h>

using namespace std;

class MeshClient {

public:

    MeshClient();
    ~MeshClient();

    bool attachSerial(string device);
    void detach(void);
    void join(void);

    bool verbose(void) const;
    void setVerbose(bool verbose);

    bool logStderr(void) const;
    void enableLogStderr(bool enable);

    unsigned int heartbeatSeconds(void) const;
    void setHeartbeatSeconds(unsigned int seconds);

    bool sendDisconnect(void);
    bool sendWantConfig(void);
    bool sendHeartbeat(void);

    bool textMessage(uint32_t dest, uint8_t channel, const string &message,
                     unsigned int hop_start = 3, bool want_ack = false);

    bool adminMessageReboot(unsigned int seconds = 0);

    uint32_t whoami(void) const;
    string lookupLongName(uint32_t id) const;
    string lookupShortName(uint32_t id) const;
    string getDisplayName(uint32_t id) const;
    string getChannelName(uint8_t channel) const;

protected:

    static void mtEvent(struct mt_client *, const void *, size_t,
                        const meshtastic_FromRadio *);
    static void logEvent(struct mt_client *, const char *, size_t);

    virtual void gotPacket(const meshtastic_MeshPacket &packet);
    virtual void gotMyNodeInfo(const meshtastic_MyNodeInfo &myNodeInfo);
    virtual void gotNodeInfo(const meshtastic_NodeInfo &nodeInfo);
    virtual void gotConfig(const meshtastic_Config &config);
    virtual void gotDeviceConfig(const meshtastic_Config_DeviceConfig &c);
    virtual void gotPositionConfig(const meshtastic_Config_PositionConfig &c);
    virtual void gotPowerConfig(const meshtastic_Config_PowerConfig &c);
    virtual void gotNetworkConfig(const meshtastic_Config_NetworkConfig &c);
    virtual void gotDisplayConfig(const meshtastic_Config_DisplayConfig &c);
    virtual void gotLoraConfig(const meshtastic_Config_LoRaConfig &c);
    virtual void gotBluetoothConfig(const meshtastic_Config_BluetoothConfig &c);
    virtual void gotSecurityConfig(const meshtastic_Config_SecurityConfig &c);
    virtual void gotSessionkeyConfig(const meshtastic_Config_SessionkeyConfig &c);
    //virtual void gotDeviceUIConfig(const meshtastic_DeviceUIConfig &c);
    virtual void gotModuleConfig(const meshtastic_ModuleConfig &moduleConfig);
    virtual void gotModuleConfigMQTT(const meshtastic_ModuleConfig_MQTTConfig &c);
    virtual void gotModuleConfigSerial(const meshtastic_ModuleConfig_SerialConfig &c);
    virtual void gotModuleConfigExternalNotification(const meshtastic_ModuleConfig_ExternalNotificationConfig &c);
    virtual void gotModuleConfigStoreForward(const meshtastic_ModuleConfig_StoreForwardConfig &c);
    virtual void gotModuleConfigRangeTest(const meshtastic_ModuleConfig_RangeTestConfig &c);
    virtual void gotModuleConfigTelemetry(const meshtastic_ModuleConfig_TelemetryConfig &c);
    virtual void gotModuleConfigCannedMessage(const meshtastic_ModuleConfig_CannedMessageConfig &c);
    virtual void gotModuleConfigAudio(const meshtastic_ModuleConfig_AudioConfig &c);
    virtual void gotModuleConfigRemoteHardware(const meshtastic_ModuleConfig_RemoteHardwareConfig &c);
    virtual void gotModuleConfigNeighborInfo(const meshtastic_ModuleConfig_NeighborInfoConfig &c);
    virtual void gotModuleConfigAmbientLighting(const meshtastic_ModuleConfig_AmbientLightingConfig &c);
    virtual void gotModuleConfigDetectionSensor(const meshtastic_ModuleConfig_DetectionSensorConfig &c);
    virtual void gotModuleConfigPaxcounter(const meshtastic_ModuleConfig_PaxcounterConfig &c);
    virtual void gotChannel(const meshtastic_Channel &channel);
    virtual void gotConfigCompleteId(uint32_t id);
    virtual void gotQueueStatus(const meshtastic_QueueStatus &queueStatus);
    virtual void gotDeviceMetadata(const meshtastic_DeviceMetadata &deviceMetadata);
    virtual void gotFileInfo(const meshtastic_FileInfo &fileInfo);
    virtual void gotDeviceUIConfig(const meshtastic_DeviceUIConfig &deviceUIConfig);
    virtual void gotMqttClientProxyMessage(const meshtastic_MqttClientProxyMessage &m);

    virtual void gotTextMessage(const meshtastic_MeshPacket &packet,
                                const string &message);
    virtual void gotPosition(const meshtastic_MeshPacket &packet,
                             const meshtastic_Position &position);
    virtual void gotUser(const meshtastic_MeshPacket &packet,
                         const meshtastic_User &user);
    virtual void gotRouting(const meshtastic_MeshPacket &packet,
                            const meshtastic_Routing &routing);
    virtual void gotTelemetry(const meshtastic_MeshPacket &packet,
                              const meshtastic_Telemetry &telemetry);
    virtual void gotDeviceMetrics(const meshtastic_MeshPacket &packet,
                                  const meshtastic_DeviceMetrics &metrics);
    virtual void gotEnvironmentMetrics(const meshtastic_MeshPacket &packet,
                                       const meshtastic_EnvironmentMetrics &metrics);
    virtual void gotAirQualityMetrics(const meshtastic_MeshPacket &packet,
                                      const meshtastic_AirQualityMetrics &metrics);
    virtual void gotPowerMetrics(const meshtastic_MeshPacket &packet,
                                       const meshtastic_PowerMetrics &metrics);
    virtual void gotLocalStats(const meshtastic_MeshPacket &packet,
                               const meshtastic_LocalStats &stats);
    virtual void gotHealthMetrics(const meshtastic_MeshPacket &packet,
                                  const meshtastic_HealthMetrics &metrics);
    virtual void gotHostMetrics(const meshtastic_MeshPacket &packet,
                                const meshtastic_HostMetrics &metrics);
    virtual void gotTraceRoute(const meshtastic_MeshPacket &packet,
                               const meshtastic_RouteDiscovery &routeDiscovery);

    meshtastic_MyNodeInfo _myNodeInfo;
    map<uint32_t, meshtastic_NodeInfo> _nodeInfos;
    meshtastic_Config_DeviceConfig _deviceConfig;
    meshtastic_Config_PositionConfig _positionConfig;
    meshtastic_Config_PowerConfig _powerConfig;
    meshtastic_Config_NetworkConfig _networkConfig;
    meshtastic_Config_DisplayConfig _displayConfig;
    meshtastic_Config_LoRaConfig _loraConfig;
    meshtastic_Config_BluetoothConfig _bluetoothConfig;
    meshtastic_Config_SecurityConfig _securityConfig;
    meshtastic_Config_SessionkeyConfig _sessionkeyConfig;
    //meshtastic_DeviceUIConfig _deviceUIConfig;
    map<uint8_t, meshtastic_Channel> _channels;
    meshtastic_QueueStatus _queueStatus;
    meshtastic_DeviceMetadata _deviceMetadata;
    meshtastic_DeviceUIConfig _deviceUIConfig;
    map<string, meshtastic_FileInfo> _fileInfos;
    meshtastic_ModuleConfig_MQTTConfig _modMQTT;
    meshtastic_ModuleConfig_SerialConfig _modSerial;
    meshtastic_ModuleConfig_ExternalNotificationConfig _modExternalNotification;
    meshtastic_ModuleConfig_StoreForwardConfig _modStoreForward;
    meshtastic_ModuleConfig_RangeTestConfig _modRangeTest;
    meshtastic_ModuleConfig_TelemetryConfig _modTelemetry;
    meshtastic_ModuleConfig_CannedMessageConfig _modCannedMessage;
    meshtastic_ModuleConfig_AudioConfig _modAudio;
    meshtastic_ModuleConfig_RemoteHardwareConfig _modRemoteHardware;
    meshtastic_ModuleConfig_NeighborInfoConfig _modNeighborInfo;
    meshtastic_ModuleConfig_AmbientLightingConfig _modAmbientLighting;
    meshtastic_ModuleConfig_DetectionSensorConfig _modDetectionSensor;
    meshtastic_ModuleConfig_PaxcounterConfig _modPaxcounter;

    map<uint32_t, meshtastic_Position> _positions;
    map<uint32_t, meshtastic_DeviceMetrics> _deviceMetrics;
    map<uint32_t, meshtastic_EnvironmentMetrics> _environmentMetrics;
    map<uint32_t, meshtastic_AirQualityMetrics> _airQualityMetrics;
    map<uint32_t, meshtastic_PowerMetrics> _powerMetrics;
    map<uint32_t, meshtastic_LocalStats> _localStats;
    map<uint32_t, meshtastic_HealthMetrics> _healthMetrics;
    map<uint32_t, meshtastic_HostMetrics> _hostMetrics;

private:

    void stop(void);
    static void thread_function(MeshClient *mtc);
    void run(void);

private:

    bool _verbose;
    bool _logStderr;
    unsigned int _heartbeatSeconds;

    struct mt_client _mtc;
    shared_ptr<thread> _thread;
    mutex _mutex;
    bool _isRunning;

};

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
