/*
 * SimpleClient.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef SIMPLECLIENT_HXX
#define SIMPLECLIENT_HXX

#include <string>
#include <map>
#include <mutex>
#include <libmeshtastic.h>

using namespace std;

/*
 * Suitable for use on resource-constraint MCU platforms.
 */
class SimpleClient {

public:

    SimpleClient();
    ~SimpleClient();

    virtual void clear(void);

    uint32_t whoami(void) const;
    string lookupLongName(uint32_t id) const;
    string lookupShortName(uint32_t id) const;
    string getDisplayName(uint32_t id) const;
    uint32_t getId(const string &name) const;
    string getChannelName(uint8_t channel) const;
    uint8_t getChannel(const string &name) const;
    bool isChannelValid(uint8_t channel) const;

    bool sendDisconnect(void);
    bool sendWantConfig(void);
    bool sendHeartbeat(void);

    bool textMessage(uint32_t dest, uint8_t channel, const string &message,
                     unsigned int hop_start = 3, bool want_ack = false);

public:

    inline bool isConnected(void) const
    {
        return _isConnected;
    }

    inline const meshtastic_MyNodeInfo &myNodeInfo(void) const
    {
        return _myNodeInfo;
    }

    inline const map<uint32_t, meshtastic_NodeInfo> &nodeInfos(void) const
    {
        return _nodeInfos;
    }

    inline const meshtastic_Config_LoRaConfig &loraConfig(void) const
    {
        return _loraConfig;
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

    virtual void gotPacket(const meshtastic_MeshPacket &packet);
    virtual void gotMyNodeInfo(const meshtastic_MyNodeInfo &myNodeInfo);
    virtual void gotNodeInfo(const meshtastic_NodeInfo &nodeInfo);
    virtual void gotConfig(const meshtastic_Config &config);
    virtual void gotLoraConfig(const meshtastic_Config_LoRaConfig &c);
    virtual void gotChannel(const meshtastic_Channel &channel);
    virtual void gotConfigCompleteId(uint32_t id);
    virtual void gotRebooted(bool rebooted);
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

    bool _isConnected;
    meshtastic_MyNodeInfo _myNodeInfo;
    map<uint32_t, meshtastic_NodeInfo> _nodeInfos;
    meshtastic_Config_LoRaConfig _loraConfig;
    map<uint8_t, meshtastic_Channel> _channels;
    map<uint32_t, meshtastic_Position> _positions;
    map<uint32_t, meshtastic_DeviceMetrics> _deviceMetrics;
    map<uint32_t, meshtastic_EnvironmentMetrics> _environmentMetrics;
    map<uint32_t, meshtastic_AirQualityMetrics> _airQualityMetrics;
    map<uint32_t, meshtastic_PowerMetrics> _powerMetrics;
    map<uint32_t, meshtastic_LocalStats> _localStats;
    map<uint32_t, meshtastic_HealthMetrics> _healthMetrics;
    map<uint32_t, meshtastic_HostMetrics> _hostMetrics;

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
