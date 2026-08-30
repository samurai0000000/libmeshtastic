/*
 * SimpleClient.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef SIMPLECLIENT_HXX
#define SIMPLECLIENT_HXX

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <libmeshtastic.h>

class BaseNvm;

#if !defined(_GLIBCXX_HAS_GTHREADS) && (!defined(_LIBCPP_VERSION) || defined(_LIBCPP_HAS_NO_THREADS))

#if defined(INC_FREERTOS_H) || (defined(__has_include) && __has_include(<FreeRTOS.h>))
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

class recursive_mutex {
public:
    recursive_mutex() : _handle(NULL) {
        _handle = xSemaphoreCreateRecursiveMutex();
    }

    ~recursive_mutex() {
        if (_handle != NULL) {
            vSemaphoreDelete(_handle);
            _handle = NULL;
        }
    }

    recursive_mutex(const recursive_mutex &) = delete;
    recursive_mutex &operator=(const recursive_mutex &) = delete;

    inline void lock(void) const {
        if (_handle == NULL) {
            const_cast<recursive_mutex *>(this)->_handle = xSemaphoreCreateRecursiveMutex();
        }
        if (_handle != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
            xSemaphoreTakeRecursive(_handle, portMAX_DELAY);
        }
    }

    inline void unlock(void) const {
        if (_handle != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
            xSemaphoreGiveRecursive(_handle);
        }
    }

private:
    SemaphoreHandle_t _handle;
};

#else

class recursive_mutex {
public:
    recursive_mutex() {}
    ~recursive_mutex() {}
    recursive_mutex(const recursive_mutex &) = delete;
    recursive_mutex &operator=(const recursive_mutex &) = delete;
    inline void lock(void) const {}
    inline void unlock(void) const {}
};

#endif

namespace std {
    using ::recursive_mutex;
}

#endif

using namespace std;

/*
 * Suitable for use on resource-constraint MCU platforms.
 */
class SimpleClient {

public:

    SimpleClient();
    ~SimpleClient();

    inline void setBanner(const string &banner) {
        _banner = banner;
    }
    inline void setVersion(const string &version) {
        _version = version;
    }
    inline void setBuilt(const string &built) {
        _built = built;
    }
    inline void setCopyright(const string &copyright) {
        _copyright = copyright;
    }

    inline const string &banner(void) const {
        return _banner;
    }
    inline const string &version(void) const {
        return _version;
    }
    inline const string &built(void) const {
        return _built;
    }
    inline const string &copyright(void) const {
        return _copyright;
    }
    inline const string &firmwareVersion(void) const {
        return _firmwareVersion;
    }

    virtual void clear(void);

    uint32_t whoami(void) const;
    string whoamiString(void) const;
    string idString(uint32_t id) const;
    string lookupLongName(uint32_t id, bool noUnprintable = false) const;
    string lookupShortName(uint32_t id, bool noUnprintable = false) const;
    string getDisplayName(uint32_t id, bool noUnprintable = false) const;
    uint32_t getId(const string &name) const;
    string getChannelName(uint8_t channel) const;
    uint8_t getChannel(const string &name) const;
    bool isChannelValid(uint8_t channel) const;

    unsigned int hopsAway(uint32_t node_num) const;
    unsigned int hopsAway(const meshtastic_MeshPacket &packet) const;

    bool sendDisconnect(void);
    bool sendWantConfig(void);
    bool sendHeartbeat(void);

    bool textMessage(uint32_t dest, uint8_t channel, const string &message,
                     unsigned int hop_start = 3, bool want_ack = false);
    bool adminMessageReboot(unsigned int seconds = 0);

    struct NodeFilterRange {
        class Iterator {
        public:
            Iterator(map<uint32_t, meshtastic_NodeInfo>::const_iterator it,
                     map<uint32_t, meshtastic_NodeInfo>::const_iterator end,
                     uint32_t seconds, time_t now);

            const meshtastic_NodeInfo &operator*(void) const;
            const meshtastic_NodeInfo *operator->(void) const;
            Iterator &operator++(void);
            Iterator operator++(int);
            bool operator==(const Iterator &other) const;
            bool operator!=(const Iterator &other) const;

        private:
            void advanceToNextValid(void);

            map<uint32_t, meshtastic_NodeInfo>::const_iterator _it;
            map<uint32_t, meshtastic_NodeInfo>::const_iterator _end;
            uint32_t _seconds;
            time_t _now;
        };

        NodeFilterRange(const map<uint32_t, meshtastic_NodeInfo> &nodes,
                        uint32_t seconds, time_t now);

        Iterator begin(void) const;
        Iterator end(void) const;

    private:
        const map<uint32_t, meshtastic_NodeInfo> &_nodes;
        uint32_t _seconds;
        time_t _now;
    };

    NodeFilterRange getLastHeardNodes(uint32_t seconds = 0) const;

    bool commitEditSettings(void);

    bool purgeNode(uint32_t nodeId);
    bool purgeNode(const string &shortName);
    virtual bool purgeOldNodes(void);
    virtual void houseKeeping(void);
    virtual void hourlyTask(void);
    virtual void setupAgent(void);

    virtual void setNvm(shared_ptr<BaseNvm> nvm);
    inline shared_ptr<BaseNvm> nvm(void) const {
        return _nvm;
    }

    inline int getRobotChannel(void) const {
        if (_robotChannel < 0) {
            const_cast<SimpleClient *>(this)->setupAgent();
        }
        return _robotChannel;
    }

    bool setTime(uint32_t seconds = 0, uint32_t dest = 0);
    bool setTimezone(const string &tzdef, uint32_t dest = 0);
    virtual void syncHostClock(uint32_t epoch_seconds);

public:

    inline void lock(void) const
    {
        _mutex.lock();
    }

    inline void unlock(void) const
    {
        _mutex.unlock();
    }

    inline bool isConnected(void) const
    {
        return _isConnected;
    }

    inline bool isClockSynced(void) const
    {
        return _isClockSynced;
    }

    inline time_t since(void) const
    {
        return _since;
    }

    inline uint32_t getUptime(void) const
    {
        time_t now = time(NULL);
        return (now >= _since) ? (uint32_t) (now - _since) : 0;
    }

    inline const meshtastic_MyNodeInfo &myNodeInfo(void) const
    {
        return _myNodeInfo;
    }

    inline const meshtastic_Config_DeviceConfig &deviceConfig(void) const
    {
        return _deviceConfig;
    }

    inline const map<uint32_t, meshtastic_NodeInfo> &nodeInfos(void) const
    {
        return _nodeInfos;
    }

    inline const meshtastic_Config_LoRaConfig &loraConfig(void) const
    {
        return _loraConfig;
    }

    inline const meshtastic_Config_PositionConfig &positionConfig(void) const
    {
        return _positionConfig;
    }

    inline const meshtastic_Config_PowerConfig &powerConfig(void) const
    {
        return _powerConfig;
    }

    inline const meshtastic_Config_NetworkConfig &networkConfig(void) const
    {
        return _networkConfig;
    }

    inline const meshtastic_Config_DisplayConfig &displayConfig(void) const
    {
        return _displayConfig;
    }

    inline const meshtastic_Config_BluetoothConfig &bluetoothConfig(void) const
    {
        return _bluetoothConfig;
    }

    inline const meshtastic_Config_SecurityConfig &securityConfig(void) const
    {
        return _securityConfig;
    }

    inline const meshtastic_Config_SessionkeyConfig &sessionkeyConfig(void) const
    {
        return _sessionkeyConfig;
    }

    inline const meshtastic_QueueStatus &queueStatus(void) const
    {
        return _queueStatus;
    }

    inline const meshtastic_DeviceMetadata &deviceMetadata(void) const
    {
        return _deviceMetadata;
    }

    inline const meshtastic_DeviceUIConfig &deviceUIConfig(void) const
    {
        return _deviceUIConfig;
    }

    inline const map<string, meshtastic_FileInfo> &fileInfos(void) const
    {
        return _fileInfos;
    }

    inline const meshtastic_ModuleConfig_MQTTConfig &modMQTT(void) const
    {
        return _modMQTT;
    }

    inline const meshtastic_ModuleConfig_SerialConfig &modSerial(void) const
    {
        return _modSerial;
    }

    inline const meshtastic_ModuleConfig_ExternalNotificationConfig &modExternalNotification(void) const
    {
        return _modExternalNotification;
    }

    inline const meshtastic_ModuleConfig_StoreForwardConfig &modStoreForward(void) const
    {
        return _modStoreForward;
    }

    inline const meshtastic_ModuleConfig_RangeTestConfig &modRangeTest(void) const
    {
        return _modRangeTest;
    }

    inline const meshtastic_ModuleConfig_TelemetryConfig &modTelemetry(void) const
    {
        return _modTelemetry;
    }

    inline const meshtastic_ModuleConfig_CannedMessageConfig &modCannedMessage(void) const
    {
        return _modCannedMessage;
    }

    inline const meshtastic_ModuleConfig_AudioConfig &modAudio(void) const
    {
        return _modAudio;
    }

    inline const meshtastic_ModuleConfig_RemoteHardwareConfig &modRemoteHardware(void) const
    {
        return _modRemoteHardware;
    }

    inline const meshtastic_ModuleConfig_NeighborInfoConfig &modNeighborInfo(void) const
    {
        return _modNeighborInfo;
    }

    inline const meshtastic_ModuleConfig_AmbientLightingConfig &modAmbientLighting(void) const
    {
        return _modAmbientLighting;
    }

    inline const meshtastic_ModuleConfig_DetectionSensorConfig &modDetectionSensor(void) const
    {
        return _modDetectionSensor;
    }

    inline const meshtastic_ModuleConfig_PaxcounterConfig &modPaxcounter(void) const
    {
        return _modPaxcounter;
    }

    inline const map<uint8_t, meshtastic_Channel> &channels(void) const
    {
        return _channels;
    }

    inline const map<uint32_t, meshtastic_Position> &positions(void) const {
        return _positions;
    }

    inline const map<uint32_t, meshtastic_DeviceMetrics> &deviceMetrics(void) const
    {
        return _deviceMetrics;
    }

    inline const map<uint32_t, meshtastic_EnvironmentMetrics> &environmentMetrics(void) const
    {
        return _environmentMetrics;
    }

    inline const map<uint32_t, meshtastic_AirQualityMetrics> &airQualityMetrics(void) const
    {
        return _airQualityMetrics;
    }

    inline const map<uint32_t, meshtastic_PowerMetrics> &powerMetrics(void) const
    {
        return _powerMetrics;
    }

    inline const map<uint32_t, meshtastic_LocalStats> &localStats(void) const
    {
        return _localStats;
    }

    inline const map<uint32_t, meshtastic_HealthMetrics> &healthMetrics(void) const
    {
        return _healthMetrics;
    }

    inline const map<uint32_t, meshtastic_HostMetrics> &hostMetrics(void) const
    {
        return _hostMetrics;
    }

protected:

    static void mtEvent(struct mt_client *mtc,
                        const void *packet, size_t size,
                        const meshtastic_FromRadio *fromRadio);

    virtual void updateNodeFromPacket(const meshtastic_MeshPacket &packet);
    virtual void gotPacket(const meshtastic_MeshPacket &packet);
    virtual void gotMyNodeInfo(const meshtastic_MyNodeInfo &myNodeInfo);
    virtual void gotNodeInfo(const meshtastic_NodeInfo &nodeInfo);
    virtual void gotConfig(const meshtastic_Config &config);
    virtual void gotLoraConfig(const meshtastic_Config_LoRaConfig &c);
    virtual void gotDeviceConfig(const meshtastic_Config_DeviceConfig &c);
    virtual void gotPositionConfig(const meshtastic_Config_PositionConfig &c);
    virtual void gotPowerConfig(const meshtastic_Config_PowerConfig &c);
    virtual void gotNetworkConfig(const meshtastic_Config_NetworkConfig &c);
    virtual void gotDisplayConfig(const meshtastic_Config_DisplayConfig &c);
    virtual void gotBluetoothConfig(const meshtastic_Config_BluetoothConfig &c);
    virtual void gotSecurityConfig(const meshtastic_Config_SecurityConfig &c);
    virtual void gotSessionkeyConfig(const meshtastic_Config_SessionkeyConfig &c);
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
    virtual void gotTextMessage(const meshtastic_MeshPacket &packet,
                                const string &message);
    virtual void gotPosition(const meshtastic_MeshPacket &packet,
                             const meshtastic_Position &position);
    virtual void gotUser(const meshtastic_MeshPacket &packet,
                         const meshtastic_User &user);
    virtual void gotRouting(const meshtastic_MeshPacket &packet,
                            const meshtastic_Routing &routing);
    virtual void gotAdminMessage(const meshtastic_MeshPacket &packet,
                                 const meshtastic_AdminMessage &adminMessage);
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

public:

    struct mt_client _mtc;

protected:

    mutable recursive_mutex _mutex;

    bool _isConnected;
    bool _isClockSynced;
    bool _bootAnnounced;
    time_t _since;
    meshtastic_MyNodeInfo _myNodeInfo;
    map<uint32_t, meshtastic_NodeInfo> _nodeInfos;
    meshtastic_Config_LoRaConfig _loraConfig;
    meshtastic_Config_DeviceConfig _deviceConfig;
    meshtastic_Config_PositionConfig _positionConfig;
    meshtastic_Config_PowerConfig _powerConfig;
    meshtastic_Config_NetworkConfig _networkConfig;
    meshtastic_Config_DisplayConfig _displayConfig;
    meshtastic_Config_BluetoothConfig _bluetoothConfig;
    meshtastic_Config_SecurityConfig _securityConfig;
    meshtastic_Config_SessionkeyConfig _sessionkeyConfig;
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
    map<uint8_t, meshtastic_Channel> _channels;
    map<uint32_t, meshtastic_Position> _positions;
    map<uint32_t, meshtastic_DeviceMetrics> _deviceMetrics;
    map<uint32_t, meshtastic_EnvironmentMetrics> _environmentMetrics;
    map<uint32_t, meshtastic_AirQualityMetrics> _airQualityMetrics;
    map<uint32_t, meshtastic_PowerMetrics> _powerMetrics;
    map<uint32_t, meshtastic_LocalStats> _localStats;
    map<uint32_t, meshtastic_HealthMetrics> _healthMetrics;
    map<uint32_t, meshtastic_HostMetrics> _hostMetrics;

    shared_ptr<BaseNvm> _nvm;
    int _robotChannel;
    time_t _lastHourlyTask;

public:

    inline void resetMeshStats(void) {
        _dmRx = 0;
        _dmTx = 0;
        _cmRx = 0;
        _cmTx = 0;
        _countWantConfigs = 0;
        _countHeartbeats = 0;
        _countTextMessages = 0;
    }

    uint32_t meshDeviceBytesReceived(void) const;
    uint32_t meshDeviceBytesSent(void) const;
    uint32_t meshDevicePacketsReceived(void) const;
    uint32_t meshDevicePacketsSent(void) const;
    uint32_t meshDeviceLastReceivedSecondsAgo(void) const;
    inline uint32_t meshDeviceLastRecivedSecondsAgo(void) const {
        return meshDeviceLastReceivedSecondsAgo();
    }

    inline uint32_t dmRx(void) const {
        return _dmRx;
    }

    inline uint32_t dmTx(void) const {
        return _dmTx;
    }

    inline uint32_t cmRx(void) const {
        return _cmRx;
    }

    inline uint32_t cmTx(void) const {
        return _cmTx;
    }

    inline uint32_t countWantConfigs(void) const {
        return _countWantConfigs;
    }

    inline uint32_t countHeartbeats(void) const {
        return _countHeartbeats;
    }

    inline uint32_t countHearbeats(void) const {
        return countHeartbeats();
    }

    inline uint32_t countTextMessages(void) const {
        return _countTextMessages;
    }

protected:

    string _banner;
    string _version;
    string _built;
    string _copyright;
    string _firmwareVersion;

    uint32_t _dmRx;
    uint32_t _dmTx;
    uint32_t _cmRx;
    uint32_t _cmTx;

    uint32_t _countWantConfigs;
    uint32_t _countHeartbeats;
    uint32_t _countTextMessages;

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
