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
#include <SimpleClient.hxx>

using namespace std;

class HomeChat;

/*
 * Suitable for use on a full system with OS (x86, aarch64, etc.)
 */
class MeshClient : public SimpleClient {

public:

    MeshClient();
    ~MeshClient();

    virtual void clear(void);

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

    unsigned int hopsAway(uint32_t node_num) const;
    unsigned int hopsAway(const meshtastic_MeshPacket &packet) const;

    inline uint32_t whoami(void) const {
        return SimpleClient::whoami();
    }

    inline string lookupLongName(uint32_t id) const {
        return SimpleClient::lookupLongName(id);
    }

    inline string lookupShortName(uint32_t id) const {
        return SimpleClient::lookupShortName(id);
    }

    inline string getDisplayName(uint32_t id) const {
        return SimpleClient::getDisplayName(id);
    }

    inline string getChannelName(uint8_t channel) const {
        return SimpleClient::getChannelName(channel);
    }

    inline const meshtastic_Config_DeviceConfig &deviceConfig(void) const {
        return _deviceConfig;
    }

    inline const meshtastic_Config_PositionConfig &positionConfig(void) const {
        return _positionConfig;
    }

    inline const meshtastic_Config_PowerConfig &powerConfig(void) const {
        return _powerConfig;
    }

    inline const meshtastic_Config_NetworkConfig &networkConfig(void) const {
        return _networkConfig;
    }

    inline const meshtastic_Config_DisplayConfig &displayConfig(void) const {
        return _displayConfig;
    }

    inline const meshtastic_Config_BluetoothConfig &bluetoothConfig(void) const {
        return _bluetoothConfig;
    }

    inline const meshtastic_Config_SecurityConfig &securityConfig(void) const  {
        return _securityConfig;
    }

    inline const meshtastic_Config_SessionkeyConfig &sessionkeyConfig(void) const {
        return _sessionkeyConfig;
    }

    inline const meshtastic_QueueStatus &queueStatus(void) const {
        return _queueStatus;
    }

    inline const meshtastic_DeviceMetadata &deviceMetadata(void) const {
        return _deviceMetadata;
    }

    inline const meshtastic_DeviceUIConfig &deviceUIConfig(void) const {
        return _deviceUIConfig;
    }

    inline const map<string, meshtastic_FileInfo> &fileInfos(void) const {
        return _fileInfos;
    }

    inline const meshtastic_ModuleConfig_MQTTConfig &modMQTT(void) const {
        return _modMQTT;
    }

    inline const meshtastic_ModuleConfig_SerialConfig &modSerial(void) const {
        return _modSerial;
    }

    inline const meshtastic_ModuleConfig_ExternalNotificationConfig &modExternalNotification(void) const {
        return _modExternalNotification;
    }

    inline const meshtastic_ModuleConfig_StoreForwardConfig &modStoreForward(void) const {
        return _modStoreForward;
    }

    inline const meshtastic_ModuleConfig_RangeTestConfig &modRangeTest(void) const {
        return _modRangeTest;
    }

    inline const meshtastic_ModuleConfig_TelemetryConfig &modTelemetry(void) const {
        return _modTelemetry;
    }

    inline const meshtastic_ModuleConfig_CannedMessageConfig &modCannedMessage(void) const {
        return _modCannedMessage;
    }

    inline const meshtastic_ModuleConfig_AudioConfig &modAudio(void) const {
        return _modAudio;
    }

    inline const meshtastic_ModuleConfig_RemoteHardwareConfig &modRemoteHardware(void) const {
        return _modRemoteHardware;
    }

    inline const meshtastic_ModuleConfig_NeighborInfoConfig &modNeighborInfo(void) const {
        return _modNeighborInfo;
    }

    inline const meshtastic_ModuleConfig_AmbientLightingConfig &modAmbientLighting(void) const {
        return _modAmbientLighting;
    }

    inline const meshtastic_ModuleConfig_DetectionSensorConfig &modDetectionSensor(void) const {
        return _modDetectionSensor;
    }

    inline const meshtastic_ModuleConfig_PaxcounterConfig &modPaxcounter(void) const {
        return _modPaxcounter;
    }

    inline virtual HomeChat *getHomeChat(void) {
        return NULL;
    }

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
    virtual void gotRebooted(bool rebooted);
    virtual void gotQueueStatus(const meshtastic_QueueStatus &queueStatus);
    virtual void gotDeviceMetadata(const meshtastic_DeviceMetadata &deviceMetadata);
    virtual void gotFileInfo(const meshtastic_FileInfo &fileInfo);
    virtual void gotDeviceUIConfig(const meshtastic_DeviceUIConfig &deviceUIConfig);
    virtual void gotMqttClientProxyMessage(const meshtastic_MqttClientProxyMessage &m);

    inline virtual void gotTextMessage(const meshtastic_MeshPacket &packet,
                                       const string &message) {
        SimpleClient::gotTextMessage(packet, message);
    }

    inline virtual void gotPosition(const meshtastic_MeshPacket &packet,
                                    const meshtastic_Position &position) {
        SimpleClient::gotPosition(packet, position);
    }

    inline virtual void gotUser(const meshtastic_MeshPacket &packet,
                                const meshtastic_User &user) {
        SimpleClient::gotUser(packet, user);
    }

    inline void gotRouting(const meshtastic_MeshPacket &packet,
                           const meshtastic_Routing &routing) {
        SimpleClient::gotRouting(packet, routing);
    }

    inline void gotAdminMessage(const meshtastic_MeshPacket &packet,
                         const meshtastic_AdminMessage &adminMessage) {
        SimpleClient::gotAdminMessage(packet, adminMessage);
    }

    inline virtual void gotTelemetry(const meshtastic_MeshPacket &packet,
                                     const meshtastic_Telemetry &telemetry) {
        SimpleClient::gotTelemetry(packet, telemetry);
    }

    inline virtual void gotDeviceMetrics(
        const meshtastic_MeshPacket &packet,
        const meshtastic_DeviceMetrics &metrics) {
        SimpleClient::gotDeviceMetrics(packet, metrics);
}

    inline virtual void gotEnvironmentMetrics(
        const meshtastic_MeshPacket &packet,
        const meshtastic_EnvironmentMetrics &metrics) {
        SimpleClient::gotEnvironmentMetrics(packet, metrics);
    }

    inline virtual void gotAirQualityMetrics(
        const meshtastic_MeshPacket &packet,
        const meshtastic_AirQualityMetrics &metrics) {
        SimpleClient::gotAirQualityMetrics(packet, metrics);
    }

    inline virtual void gotPowerMetrics(
        const meshtastic_MeshPacket &packet,
        const meshtastic_PowerMetrics &metrics) {
        SimpleClient::gotPowerMetrics(packet, metrics);
    }

    inline virtual void gotLocalStats(const meshtastic_MeshPacket &packet,
                                      const meshtastic_LocalStats &stats) {
        SimpleClient::gotLocalStats(packet, stats);
    }

    inline virtual void gotHealthMetrics(
        const meshtastic_MeshPacket &packet,
        const meshtastic_HealthMetrics &metrics) {
        SimpleClient::gotHealthMetrics(packet, metrics);
    }

    inline virtual void gotHostMetrics(const meshtastic_MeshPacket &packet,
                                       const meshtastic_HostMetrics &metrics) {
        SimpleClient::gotHostMetrics(packet, metrics);
    }

    inline virtual void gotTraceRoute(
        const meshtastic_MeshPacket &packet,
        const meshtastic_RouteDiscovery &routeDiscovery) {
        SimpleClient::gotTraceRoute(packet, routeDiscovery);
    }

    //meshtastic_MyNodeInfo _myNodeInfo;
    //map<uint32_t, meshtastic_NodeInfo> _nodeInfos;
    meshtastic_Config_DeviceConfig _deviceConfig;
    meshtastic_Config_PositionConfig _positionConfig;
    meshtastic_Config_PowerConfig _powerConfig;
    meshtastic_Config_NetworkConfig _networkConfig;
    meshtastic_Config_DisplayConfig _displayConfig;
    //meshtastic_Config_LoRaConfig _loraConfig;
    meshtastic_Config_BluetoothConfig _bluetoothConfig;
    meshtastic_Config_SecurityConfig _securityConfig;
    meshtastic_Config_SessionkeyConfig _sessionkeyConfig;
    //meshtastic_DeviceUIConfig _deviceUIConfig;
    //map<uint8_t, meshtastic_Channel> _channels;
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

    //map<uint32_t, meshtastic_Position> _positions;
    //map<uint32_t, meshtastic_DeviceMetrics> _deviceMetrics;
    //map<uint32_t, meshtastic_EnvironmentMetrics> _environmentMetrics;
    //map<uint32_t, meshtastic_AirQualityMetrics> _airQualityMetrics;
    //map<uint32_t, meshtastic_PowerMetrics> _powerMetrics;
    //map<uint32_t, meshtastic_LocalStats> _localStats;
    //map<uint32_t, meshtastic_HealthMetrics> _healthMetrics;
    //map<uint32_t, meshtastic_HostMetrics> _hostMetrics;

private:

    void stop(void);
    static void thread_function(MeshClient *mtc);
    void run(void);

private:

    bool _verbose;
    bool _logStderr;
    unsigned int _heartbeatSeconds;

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
