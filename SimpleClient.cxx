/*
 * SimpleClient.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <SimpleClient.hxx>

SimpleClient::SimpleClient()
{
    bzero(&_mtc, sizeof(_mtc));
    _mtc.handler = this->mtEvent;
    _mtc.ctx = this;
    _isConnected = false;
    resetMeshStats();
}

SimpleClient::~SimpleClient()
{

}

void SimpleClient::clear(void)
{
    _nodeInfos.clear();
    _loraConfig = meshtastic_Config_LoRaConfig();
    _channels.clear();
    _positions.clear();
    _deviceMetrics.clear();
    _environmentMetrics.clear();
    _airQualityMetrics.clear();
    _powerMetrics.clear();
    _localStats.clear();
    _healthMetrics.clear();
    _hostMetrics.clear();
}

uint32_t SimpleClient::whoami(void) const
{
    return _myNodeInfo.my_node_num;
}

string SimpleClient::whoamiString(void) const
{
    return idString(whoami());
}

string SimpleClient::idString(uint32_t id) const
{
    char buf[16];
#if defined(LIB_PICO_PLATFORM) || defined(ESP_PLATFORM)
    snprintf(buf, sizeof(buf) - 1, "!%.8lx", id);
#else
    snprintf(buf, sizeof(buf) - 1, "!%.8x", id);
#endif
    return string(buf);
}

string SimpleClient::lookupLongName(uint32_t id, bool noUnprintable) const
{
    string s;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    if (id == 0xffffffffU) {
        return "broadcast";
    }

    it = _nodeInfos.find(id);
    if (it != _nodeInfos.end()) {
        for (unsigned int i = 0; i < sizeof(it->second.user.long_name); i++) {
            char c = it->second.user.long_name[i];
            if (c == '\0') {
                break;
            }
            if (!noUnprintable || isprint(c)) {
                s += c;
            } else {
                s += "?";
            }
        }
    }

    return s;
}

string SimpleClient::lookupShortName(uint32_t id, bool noUnprintable) const
{
    string s;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    if (id == 0xffffffffU) {
        return "****";
    }

    it = _nodeInfos.find(id);
    if ((it != _nodeInfos.end()) && (it->second.user.short_name[0] != '\0')) {
        for (unsigned int i = 0; i < 4; i++) {
            char c = it->second.user.short_name[i];
            if (!noUnprintable || isprint(c)) {
                s += c;
            } else {
                s += "?";
            }
        }
    }

    if (s.empty()) {
        char buf[8];
        snprintf(buf, sizeof(buf) - 1, "%.4x", (uint16_t) (id & 0xffffU));
        s = buf;
    }

    return s;
}

string SimpleClient::getDisplayName(uint32_t id, bool noUnprintable) const
{
    stringstream ss;

    ss << lookupShortName(id, noUnprintable)
       << " (!" << hex << setfill('0') << setw(8) << id << ")";

    return ss.str();
}

uint32_t SimpleClient::getId(const string &name) const
{
    uint32_t id = 0xffffffffU;
    uint32_t node_num = 0xffffffffU;
    map<uint8_t, meshtastic_Channel>::const_iterator it;

    if ((name.size() > 0) && (name[0] == '!')) {
        try {
            string hexstr = name.substr(1);
            node_num = static_cast<uint32_t>(std::stoul(hexstr, nullptr, 16));
        } catch (const invalid_argument &e) {
        } catch (const out_of_range &e) {
        }
    }

    for (map<uint32_t, meshtastic_NodeInfo>::const_iterator it =
             _nodeInfos.begin(); it != _nodeInfos.end(); it++) {
        if (node_num == it->second.num) {
            id = it->first;
            break;
        }

        if (it->second.has_user == false) {
            continue;
        }

        if (name == it->second.user.short_name) {
            id = it->first;
            break;
        } else if (name == it->second.user.long_name) {
            id = it->first;
            break;
        }
    }

    return id;
}

string SimpleClient::getChannelName(uint8_t channel) const
{
    string name;
    map<uint8_t, meshtastic_Channel>::const_iterator it;

    it = _channels.find(channel);
    if (it != _channels.end()) {
        if (it->second.has_settings) {
            name = it->second.settings.name;
            if (name.empty()) {
                switch (_loraConfig.modem_preset) {
                case meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST:
                    name = "LongFast";
                    break;
                case meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW :
                    name = "LongSlow";
                    break;
                case meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW:
                    name = "VeryLongSlow";
                    break;
                case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW:
                    name = "MediumSlow";
                    break;
                case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST:
                    name = "MediumFast";
                    break;
                case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW:
                    name = "ShortSlow";
                    break;
                case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST:
                    name = "ShortFast";
                    break;
                case meshtastic_Config_LoRaConfig_ModemPreset_LONG_MODERATE:
                    name = "LongModerate";
                    break;
                case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_TURBO:
                    name = "ShortTurbo";
                    break;
                default:
                    break;
                }
            }
        }
    }

    return name;
}

uint8_t SimpleClient::getChannel(const string &name) const
{
    uint8_t channel = 0xffU;

    for (map<uint8_t, meshtastic_Channel>::const_iterator it =
             _channels.begin(); it != _channels.end(); it++) {
        if (name == getChannelName(it->first)) {
            channel = it->first;
            break;
        }
    }

    return channel;
}

bool SimpleClient::isChannelValid(uint8_t channel) const
{
    map<uint8_t, meshtastic_Channel>::const_iterator it;

    it = _channels.find(channel);
    if (it == _channels.end()) {
        return false;
    }

    if (it->second.has_settings == false) {
        return false;
    }

    if (it->second.role == meshtastic_Channel_Role_DISABLED) {
        return false;
    }

    return true;
}

void SimpleClient::mtEvent(struct mt_client *mtc,
                           const void *packet, size_t size,
                           const meshtastic_FromRadio *fromRadio)
{
    SimpleClient *sc = (SimpleClient *) mtc->ctx;

    (void)(packet);
    (void)(size);

    switch (fromRadio->which_payload_variant) {
    case meshtastic_FromRadio_packet_tag:
        sc->gotPacket(fromRadio->packet);
        break;
    case meshtastic_FromRadio_my_info_tag:
        sc->gotMyNodeInfo(fromRadio->my_info);
        break;
    case meshtastic_FromRadio_node_info_tag:
        sc->gotNodeInfo(fromRadio->node_info);
        break;
    case meshtastic_FromRadio_config_tag :
        sc->gotConfig(fromRadio->config);
        break;
    case meshtastic_FromRadio_channel_tag:
        sc->gotChannel(fromRadio->channel);
        break;
    case meshtastic_FromRadio_config_complete_id_tag:
        sc->gotConfigCompleteId(fromRadio->config_complete_id);
        break;
    case meshtastic_FromRadio_rebooted_tag:
        sc->gotRebooted(fromRadio->rebooted);
        break;
        break;
    default:
        break;
    }
}

bool SimpleClient::sendDisconnect(void)
{
    bool result = false;

    result = (mt_send_disconnect(&_mtc) == 0);
    if (result) {
        clear();
        _isConnected = false;
    }

    return result;
}

bool SimpleClient::sendWantConfig(void)
{
    bool result = false;

    result = (mt_send_want_config(&_mtc) == 0);
    if (result) {
        _countWantConfigs++;
    }

    return result;
}

bool SimpleClient::sendHeartbeat(void)
{
    bool result = false;

    result = (mt_send_heartbeat(&_mtc) == 0);
    if (result) {
        _countHeartbeats++;
    }

    return result;
}

bool SimpleClient::textMessage(uint32_t dest, uint8_t channel,
                             const string &message,
                             unsigned int hop_start, bool want_ack)
{
    bool result = false;

    if (hop_start == 0) {
        hop_start = _loraConfig.hop_limit;
    }

    if (message.size() <= 200) {
        result = (mt_text_message(&_mtc, dest, channel,
                                  message.c_str(),
                                  hop_start, want_ack) == 0);
        if (result) {
            if (dest == 0xffffffffU) {
                _cmTx++;
            } else {
                _dmTx++;
            }
        }
    } else {
#if 0
        string multipart = message;
        string substring;
        size_t pos;
        size_t count = 0;

        while (!multipart.empty()) {
            if (multipart.size() > 200) {
                pos = multipart.rfind('\n', 200);
                if (pos == string::npos) {
                    result = false;
                    break;
                } else {
                    substring = multipart.substr(0, pos);
                    multipart = multipart.substr(pos);
                }
            } else {
                substring = multipart;
                multipart.clear();
            }

            if (count > 5) {
                substring = "... <truncated> ...";
                multipart.clear();
            }

            result = (mt_text_message(&_mtc, dest, channel,
                                      substring.c_str(),
                                      hop_start, want_ack) == 0);
            if (result) {
                if (dest == 0xffffffffU) {
                    _cmTx++;
                } else {
                    _dmTx++;
                }
            } else {
                break;
            }

            count++;
        }
#else
        string modified = message.substr(0, 180) + "\n...<truncated>...";
        result = (mt_text_message(&_mtc, dest, channel,
                                  modified.c_str(),
                                  hop_start, want_ack) == 0);
        if (result) {
            if (dest == 0xffffffffU) {
                _cmTx++;
            } else {
                _dmTx++;
            }
        }
#endif
    }

    if (result) {
        _countTextMessages++;
    }

    return result;
}

void SimpleClient::gotConfig(const meshtastic_Config &config)
{
    switch (config.which_payload_variant) {
    case meshtastic_Config_lora_tag:
        gotLoraConfig(config.payload_variant.lora);
        break;
    default:
        break;
    }
}

void SimpleClient::gotLoraConfig(const meshtastic_Config_LoRaConfig &c)
{
    _loraConfig = c;
}

void SimpleClient::gotPacket(const meshtastic_MeshPacket &packet)
{
    int ret;
    pb_istream_t stream;

    if (packet.which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
        switch (packet.decoded.portnum) {
        case meshtastic_PortNum_TEXT_MESSAGE_APP:
            if (packet.which_payload_variant ==
                meshtastic_MeshPacket_decoded_tag) {
                string message((const char *) packet.decoded.payload.bytes,
                               packet.decoded.payload.size);
                gotTextMessage(packet, message);
            }
            break;
        case meshtastic_PortNum_POSITION_APP:
        {
            meshtastic_Position position;
            bzero(&position, sizeof(position));
            stream = pb_istream_from_buffer(packet.decoded.payload.bytes,
                                            packet.decoded.payload.size);
            ret = pb_decode(&stream, meshtastic_Position_fields, &position);
            if (ret == 1) {
                gotPosition(packet, position);
            }
        }
            break;
        case meshtastic_PortNum_NODEINFO_APP:
        {
            meshtastic_User user;
            bzero(&user, sizeof(user));
            stream = pb_istream_from_buffer(packet.decoded.payload.bytes,
                                            packet.decoded.payload.size);
            ret = pb_decode(&stream, meshtastic_User_fields, &user);
            if (ret == 1) {
                gotUser(packet, user);
            }
        }
            break;
        case meshtastic_PortNum_ROUTING_APP:
        {
            meshtastic_Routing routing;
            bzero(&routing, sizeof(routing));
            stream = pb_istream_from_buffer(packet.decoded.payload.bytes,
                                            packet.decoded.payload.size);
            ret = pb_decode(&stream, meshtastic_Routing_fields, &routing);
            if (ret == 1) {
                gotRouting(packet, routing);
            }
        }
            break;
        case meshtastic_PortNum_TELEMETRY_APP:
        {
            meshtastic_Telemetry telemetry;
            bzero(&telemetry, sizeof(telemetry));
            stream = pb_istream_from_buffer(packet.decoded.payload.bytes,
                                            packet.decoded.payload.size);
            ret = pb_decode(&stream, meshtastic_Telemetry_fields, &telemetry);
            if (ret == 1) {
                gotTelemetry(packet, telemetry);
            }
        }
            break;
        case meshtastic_PortNum_TRACEROUTE_APP:
        {
            meshtastic_RouteDiscovery routeDiscovery;
            bzero(&routeDiscovery, sizeof(routeDiscovery));
            stream = pb_istream_from_buffer(packet.decoded.payload.bytes,
                                            packet.decoded.payload.size);
            ret = pb_decode(&stream, meshtastic_RouteDiscovery_fields,
                            &routeDiscovery);
            if (ret == 1) {
                gotTraceRoute(packet, routeDiscovery);
            }
        }
            break;
        default:
            break;
        }
    }
}

void SimpleClient::gotMyNodeInfo(const meshtastic_MyNodeInfo &myNodeInfo)
{
    _myNodeInfo = myNodeInfo;
}

void SimpleClient::gotNodeInfo(const meshtastic_NodeInfo &nodeInfo)
{
    uint32_t num = nodeInfo.num;

    _nodeInfos[num] = nodeInfo;
}

void SimpleClient::gotChannel(const meshtastic_Channel &channel)
{
    uint8_t index = channel.index;

    _channels[index] = channel;
}

void SimpleClient::gotConfigCompleteId(uint32_t id)
{
    (void)(id);
    _isConnected = true;
}

void SimpleClient::gotRebooted(bool rebooted)
{
    if (rebooted) {
        clear();
        _isConnected = false;
    }
}

void SimpleClient::gotTextMessage(const meshtastic_MeshPacket &packet,
                                  const string &message)
{
    (void)(message);

    if (packet.to == whoami()) {
        _dmRx++;
    } else {
        _cmRx++;
    }
}

void SimpleClient::gotPosition(const meshtastic_MeshPacket &packet,
                               const meshtastic_Position &position)
{
    (void)(packet);
    (void)(position);
}

void SimpleClient::gotUser(const meshtastic_MeshPacket &packet,
                           const meshtastic_User &user)
{
    if (_nodeInfos.find(packet.from) == _nodeInfos.end()) {
        meshtastic_NodeInfo nodeInfo;

        bzero(&nodeInfo, sizeof(nodeInfo));
        nodeInfo.user = user;
        _nodeInfos[packet.from] = nodeInfo;
    } else {
        _nodeInfos[packet.from].user = user;
    }
}

void SimpleClient::gotRouting(const meshtastic_MeshPacket &packet,
                              const meshtastic_Routing &routing)
{
    (void)(packet);
    (void)(routing);
}

void SimpleClient::gotAdminMessage(const meshtastic_MeshPacket &packet,
                                   const meshtastic_AdminMessage &adminMessage)
{
    (void)(packet);
    (void)(adminMessage);
}

void SimpleClient::gotTelemetry(const meshtastic_MeshPacket &packet,
                                const meshtastic_Telemetry &telemetry)
{
    switch (telemetry.which_variant) {
    case meshtastic_Telemetry_device_metrics_tag:
        gotDeviceMetrics(packet, telemetry.variant.device_metrics);
        break;
    case meshtastic_Telemetry_environment_metrics_tag:
        gotEnvironmentMetrics(packet, telemetry.variant.environment_metrics);
        break;
    case meshtastic_Telemetry_air_quality_metrics_tag:
        gotAirQualityMetrics(packet, telemetry.variant.air_quality_metrics);
        break;
    case meshtastic_Telemetry_power_metrics_tag:
        gotPowerMetrics(packet, telemetry.variant.power_metrics);
        break;
    case meshtastic_Telemetry_local_stats_tag:
        gotLocalStats(packet, telemetry.variant.local_stats);
        break;
    case meshtastic_Telemetry_health_metrics_tag:
        gotHealthMetrics(packet, telemetry.variant.health_metrics);
        break;
    case meshtastic_Telemetry_host_metrics_tag:
        gotHostMetrics(packet, telemetry.variant.host_metrics);
        break;
    default:
        break;
    }
}

void SimpleClient::gotDeviceMetrics(const meshtastic_MeshPacket &packet,
                                    const meshtastic_DeviceMetrics &metrics)
{
    _deviceMetrics[packet.from] = metrics;
}

void SimpleClient::gotEnvironmentMetrics(const meshtastic_MeshPacket &packet,
                                         const meshtastic_EnvironmentMetrics &metrics)
{
    _environmentMetrics[packet.from] = metrics;
}

void SimpleClient::gotAirQualityMetrics(const meshtastic_MeshPacket &packet,
                                        const meshtastic_AirQualityMetrics &metrics)
{
    _airQualityMetrics[packet.from] = metrics;
}

void SimpleClient::gotPowerMetrics(const meshtastic_MeshPacket &packet,
                                   const meshtastic_PowerMetrics &metrics)
{
    _powerMetrics[packet.from] = metrics;
}

void SimpleClient::gotLocalStats(const meshtastic_MeshPacket &packet,
                                 const meshtastic_LocalStats &stats)
{
    _localStats[packet.from] = stats;
}

void SimpleClient::gotHealthMetrics(const meshtastic_MeshPacket &packet,
                                    const meshtastic_HealthMetrics &metrics)
{
    _healthMetrics[packet.from] = metrics;
}

void SimpleClient::gotHostMetrics(const meshtastic_MeshPacket &packet,
                                  const meshtastic_HostMetrics &metrics)
{
    _hostMetrics[packet.from] = metrics;
}

void SimpleClient::gotTraceRoute(const meshtastic_MeshPacket &packet,
                                 const meshtastic_RouteDiscovery &routeDiscovery)
{
    (void)(packet);
    (void)(routeDiscovery);
}

uint32_t SimpleClient::meshDeviceBytesReceived(void) const
{
    return _mtc.bytes_rx;
}

uint32_t SimpleClient::meshDeviceBytesSent(void) const
{
    return _mtc.bytes_tx;
}

uint32_t SimpleClient::meshDevicePacketsReceived(void) const
{
    return _mtc.packets_rx;
}

uint32_t SimpleClient::meshDevicePacketsSent(void) const
{
    return _mtc.packets_tx;
}

uint32_t SimpleClient::meshDeviceLastRecivedSecondsAgo(void) const
{
    return (uint32_t) (mt_impl_now() - _mtc.last_packet_ts);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
