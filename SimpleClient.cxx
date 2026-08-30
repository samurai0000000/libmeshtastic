/*
 * SimpleClient.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <sys/time.h>
#include <BaseNvm.hxx>
#include <SimpleClient.hxx>

SimpleClient::SimpleClient()
{
    bzero(&_mtc, sizeof(_mtc));
    _mtc.fd = -1;
    _mtc.handler = this->mtEvent;
    _mtc.ctx = this;
    _isConnected = false;
    _isClockSynced = false;
    _bootAnnounced = false;
    _since = time(NULL);
    bzero(&_myNodeInfo, sizeof(_myNodeInfo));
    bzero(&_loraConfig, sizeof(_loraConfig));
    bzero(&_deviceConfig, sizeof(_deviceConfig));
    _nvm = NULL;
    _robotChannel = -1;
    _lastHourlyTask = 0;
    resetMeshStats();
}

SimpleClient::~SimpleClient()
{

}

void SimpleClient::clear(void)
{
    lock_guard<recursive_mutex> lock(_mutex);

    _nodeInfos.clear();
    bzero(&_myNodeInfo, sizeof(_myNodeInfo));
    bzero(&_loraConfig, sizeof(_loraConfig));
    bzero(&_deviceConfig, sizeof(_deviceConfig));
    _channels.clear();
    _positions.clear();
    _deviceMetrics.clear();
    _environmentMetrics.clear();
    _airQualityMetrics.clear();
    _powerMetrics.clear();
    _localStats.clear();
    _healthMetrics.clear();
    _hostMetrics.clear();
    _firmwareVersion.clear();
    _robotChannel = -1;
    _lastHourlyTask = 0;
}

uint32_t SimpleClient::whoami(void) const
{
    lock_guard<recursive_mutex> lock(_mutex);
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
    lock_guard<recursive_mutex> lock(_mutex);
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
    lock_guard<recursive_mutex> lock(_mutex);
    string s;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    if (id == 0xffffffffU) {
        return "****";
    }

    it = _nodeInfos.find(id);
    if ((it != _nodeInfos.end()) && (it->second.user.short_name[0] != '\0')) {
        for (unsigned int i = 0; i < sizeof(it->second.user.short_name); i++) {
            char c = it->second.user.short_name[i];
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
    lock_guard<recursive_mutex> lock(_mutex);
    uint32_t id = 0xffffffffU;
    uint32_t node_num = 0xffffffffU;

    if ((name.size() > 0) && (name[0] == '!')) {
        try {
            string hexstr = name.substr(1);
            node_num = static_cast<uint32_t>(std::stoul(hexstr, nullptr, 16));
        } catch (const invalid_argument &e) {
        } catch (const out_of_range &e) {
        }

        if (node_num != 0xffffffffU) {
            return node_num;
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
    lock_guard<recursive_mutex> lock(_mutex);
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
    lock_guard<recursive_mutex> lock(_mutex);
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
    lock_guard<recursive_mutex> lock(_mutex);
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
    lock_guard<recursive_mutex> lock(sc->_mutex);

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
    case meshtastic_FromRadio_metadata_tag:
        sc->gotDeviceMetadata(fromRadio->metadata);
        break;
    default:
        break;
    }
}

bool SimpleClient::sendDisconnect(void)
{
    bool result = false;
    lock_guard<recursive_mutex> lock(_mutex);

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
    lock_guard<recursive_mutex> lock(_mutex);

    result = (mt_send_want_config(&_mtc) == 0);
    if (result) {
        _countWantConfigs++;
    }

    return result;
}

bool SimpleClient::sendHeartbeat(void)
{
    bool result = false;
    lock_guard<recursive_mutex> lock(_mutex);

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
    lock_guard<recursive_mutex> lock(_mutex);

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

bool SimpleClient::adminMessageReboot(unsigned int seconds)
{
    lock_guard<recursive_mutex> lock(_mutex);
    return (mt_admin_message_reboot(&_mtc, whoami(), seconds) == 0);
}

SimpleClient::NodeFilterRange::Iterator::Iterator(
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it,
    map<uint32_t, meshtastic_NodeInfo>::const_iterator end,
    uint32_t seconds, time_t now)
    : _it(it), _end(end), _seconds(seconds), _now(now)
{
    advanceToNextValid();
}

void SimpleClient::NodeFilterRange::Iterator::advanceToNextValid(void)
{
    while (_it != _end) {
        if (_seconds == 0) {
            break;
        }

        const meshtastic_NodeInfo &info = _it->second;
        if (info.last_heard > 0) {
            uint32_t diff;
            if (_now >= (time_t) info.last_heard) {
                diff = (uint32_t)(_now - (time_t) info.last_heard);
            } else {
                diff = (uint32_t)((time_t) info.last_heard - _now);
            }

            if (diff <= _seconds) {
                break;
            }
        }
        ++_it;
    }
}

const meshtastic_NodeInfo &SimpleClient::NodeFilterRange::Iterator::operator*(void) const
{
    return _it->second;
}

const meshtastic_NodeInfo *SimpleClient::NodeFilterRange::Iterator::operator->(void) const
{
    return &_it->second;
}

SimpleClient::NodeFilterRange::Iterator &SimpleClient::NodeFilterRange::Iterator::operator++(void)
{
    if (_it != _end) {
        ++_it;
        advanceToNextValid();
    }

    return *this;
}

SimpleClient::NodeFilterRange::Iterator SimpleClient::NodeFilterRange::Iterator::operator++(int)
{
    Iterator tmp = *this;
    ++(*this);
    return tmp;
}

bool SimpleClient::NodeFilterRange::Iterator::operator==(const Iterator &other) const
{
    return _it == other._it;
}

bool SimpleClient::NodeFilterRange::Iterator::operator!=(const Iterator &other) const
{
    return _it != other._it;
}

SimpleClient::NodeFilterRange::NodeFilterRange(
    const map<uint32_t, meshtastic_NodeInfo> &nodes,
    uint32_t seconds, time_t now)
    : _nodes(nodes), _seconds(seconds), _now(now)
{

}

SimpleClient::NodeFilterRange::Iterator SimpleClient::NodeFilterRange::begin(void) const
{
    return Iterator(_nodes.begin(), _nodes.end(), _seconds, _now);
}

SimpleClient::NodeFilterRange::Iterator SimpleClient::NodeFilterRange::end(void) const
{
    return Iterator(_nodes.end(), _nodes.end(), _seconds, _now);
}

SimpleClient::NodeFilterRange SimpleClient::getLastHeardNodes(uint32_t seconds) const
{
    return NodeFilterRange(_nodeInfos, seconds, time(NULL));
}

bool SimpleClient::commitEditSettings(void)
{
    lock_guard<recursive_mutex> lock(_mutex);
    return (mt_admin_message_commit_edit_settings(&_mtc, whoami()) == 0);
}

bool SimpleClient::purgeNode(uint32_t nodeId)
{
    bool result = false;
    lock_guard<recursive_mutex> lock(_mutex);

    result = (mt_admin_message_remove_by_nodenum(&_mtc, whoami(), nodeId) == 0);
    if (result) {
        _nodeInfos.erase(nodeId);
        _positions.erase(nodeId);
        _deviceMetrics.erase(nodeId);
        _environmentMetrics.erase(nodeId);
        _airQualityMetrics.erase(nodeId);
        _powerMetrics.erase(nodeId);
        _localStats.erase(nodeId);
        _healthMetrics.erase(nodeId);
        _hostMetrics.erase(nodeId);
    }

    return result;
}

bool SimpleClient::purgeNode(const string &shortName)
{
    uint32_t nodeId = getId(shortName);

    if (nodeId == 0xffffffffU) {
        return false;
    }

    return purgeNode(nodeId);
}

static string formatRelativeTime(time_t timestamp)
{
    if (timestamp == 0) {
        return "never";
    }

    time_t now = time(NULL);
    long diff = (long)(now - timestamp);
    if (diff < 0) {
        diff = 0;
    }

    if (diff < 60) {
        return to_string(diff) + "s ago";
    } else if (diff < 3600) {
        long min = diff / 60;
        long sec = diff % 60;
        if (sec > 0) {
            return to_string(min) + "m " + to_string(sec) + "s ago";
        }
        return to_string(min) + "m ago";
    } else if (diff < 86400) {
        long hour = diff / 3600;
        long min = (diff % 3600) / 60;
        if (min > 0) {
            return to_string(hour) + "h " + to_string(min) + "m ago";
        }
        return to_string(hour) + "h ago";
    } else {
        long day = diff / 86400;
        long hour = (diff % 86400) / 3600;
        if (hour > 0) {
            return to_string(day) + "d " + to_string(hour) + "h ago";
        }
        return to_string(day) + "d ago";
    }
}

bool SimpleClient::purgeOldNodes(void)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (!_isClockSynced) {
        return false;
    }

    time_t now = time(NULL);
    const uint32_t ONE_WEEK_SECONDS = 7 * 24 * 60 * 60; // 604800
    uint32_t targetNodeId = 0;
    bool candidateFound = false;
    uint32_t oldestLastHeard = 0xffffffffU;

    for (map<uint32_t, meshtastic_NodeInfo>::const_iterator it = _nodeInfos.begin();
         it != _nodeInfos.end(); it++) {
        uint32_t nodeId = it->first;
        if (nodeId == 0 || nodeId == 0xffffffffU || nodeId == whoami()) {
            continue;
        }

        const meshtastic_NodeInfo &info = it->second;
        uint32_t lastHeard = info.last_heard;
        bool qualifies = false;

        if (lastHeard == 0) {
            qualifies = true;
        } else if (now >= (time_t) lastHeard) {
            if ((uint32_t)(now - (time_t) lastHeard) > ONE_WEEK_SECONDS) {
                qualifies = true;
            }
        }

        if (qualifies) {
            if (!candidateFound || (lastHeard < oldestLastHeard)) {
                candidateFound = true;
                oldestLastHeard = lastHeard;
                targetNodeId = nodeId;
            }
        }
    }

    if (!candidateFound) {
        return false;
    }

    string idStr = idString(targetNodeId);
    string agoStr = formatRelativeTime((time_t) oldestLastHeard);

    if (purgeNode(targetNodeId)) {
        ::printf("Purged node '%s' last heard %s\n", idStr.c_str(), agoStr.c_str());
        return true;
    }

    return false;
}

bool SimpleClient::setTime(uint32_t seconds, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }
    if (seconds == 0) {
        seconds = (uint32_t) time(NULL);
    }

    return (mt_admin_message_set_time(&_mtc, dest, seconds) == 0);
}

bool SimpleClient::setTimezone(const string &tzdef, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    const meshtastic_Config_DeviceConfig *config_template =
        (dest == whoami()) ? &_deviceConfig : NULL;

    if (mt_admin_message_set_tzdef(&_mtc, dest, config_template, tzdef.c_str()) != 0) {
        return false;
    }

    if (dest == whoami()) {
        strncpy(_deviceConfig.tzdef, tzdef.c_str(), sizeof(_deviceConfig.tzdef) - 1);
        _deviceConfig.tzdef[sizeof(_deviceConfig.tzdef) - 1] = '\0';
        setenv("TZ", _deviceConfig.tzdef, 1);
        tzset();
        commitEditSettings();
    }

    return true;
}

void SimpleClient::gotConfig(const meshtastic_Config &config)
{
    switch (config.which_payload_variant) {
    case meshtastic_Config_lora_tag:
        gotLoraConfig(config.payload_variant.lora);
        break;
    case meshtastic_Config_device_tag:
        gotDeviceConfig(config.payload_variant.device);
        break;
    default:
        break;
    }
}

void SimpleClient::gotLoraConfig(const meshtastic_Config_LoRaConfig &c)
{
    _loraConfig = c;
}

void SimpleClient::gotDeviceConfig(const meshtastic_Config_DeviceConfig &c)
{
    _deviceConfig = c;
    if (c.tzdef[0] != '\0') {
        setenv("TZ", c.tzdef, 1);
        tzset();
    }
}

void SimpleClient::syncHostClock(uint32_t epoch_seconds)
{
    if (epoch_seconds < 1700000000U) {
        return;
    }

    time_t now = time(NULL);
    if ((now < 1700000000U) || ((time_t) epoch_seconds > now && ((time_t) epoch_seconds - now) >= 60)) {
        time_t delta = (time_t) epoch_seconds - now;
        struct timeval tv;
        tv.tv_sec = (time_t) epoch_seconds;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
        _since += delta;
        _mtc.last_packet_ts = epoch_seconds;
        _mtc.last_byte_ts = epoch_seconds;
    }
    if (_deviceConfig.tzdef[0] != '\0') {
        setenv("TZ", _deviceConfig.tzdef, 1);
        tzset();
    }
    _isClockSynced = true;
}

void SimpleClient::updateNodeFromPacket(const meshtastic_MeshPacket &packet)
{
    syncHostClock(packet.rx_time);

    if (packet.from == 0) {
        return;
    }

    meshtastic_NodeInfo &info = _nodeInfos[packet.from];
    info.num = packet.from;
    info.last_heard = (packet.rx_time > 0) ? packet.rx_time : (uint32_t) time(NULL);
    if (packet.rx_snr != 0.0f) {
        info.snr = packet.rx_snr;
    }
    if (packet.hop_start >= packet.hop_limit) {
        info.has_hops_away = true;
        info.hops_away = (uint32_t)(packet.hop_start - packet.hop_limit);
    }
    if (packet.channel != 0) {
        info.channel = packet.channel;
    }
}

void SimpleClient::gotPacket(const meshtastic_MeshPacket &packet)
{
    int ret;
    pb_istream_t stream;

    updateNodeFromPacket(packet);

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
        case meshtastic_PortNum_ADMIN_APP:
        {
            meshtastic_AdminMessage adminMessage;
            bzero(&adminMessage, sizeof(adminMessage));
            stream = pb_istream_from_buffer(packet.decoded.payload.bytes,
                                            packet.decoded.payload.size);
            ret = pb_decode(&stream, meshtastic_AdminMessage_fields,
                            &adminMessage);
            if (ret == 1) {
                gotAdminMessage(packet, adminMessage);
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
    setupAgent();
}

void SimpleClient::gotConfigCompleteId(uint32_t id)
{
    (void)(id);
    _isConnected = true;
    setupAgent();

    int robotChan = getRobotChannel();
    if (robotChan >= 0 && !_bootAnnounced) {
        string announcement = lookupLongName(whoami(), true);
        if (announcement.empty()) {
            announcement = whoamiString();
        }
        announcement += " is up";
        if (textMessage(0xffffffffU, (uint8_t) robotChan, announcement)) {
            _bootAnnounced = true;
        }
    }
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
    syncHostClock(position.time);
    syncHostClock(position.timestamp);
    _positions[packet.from] = position;
}

void SimpleClient::gotUser(const meshtastic_MeshPacket &packet,
                           const meshtastic_User &user)
{
    _nodeInfos[packet.from].num = packet.from;
    _nodeInfos[packet.from].has_user = true;
    _nodeInfos[packet.from].user = user;
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

    if (adminMessage.which_payload_variant ==
        meshtastic_AdminMessage_get_device_metadata_response_tag) {
        gotDeviceMetadata(adminMessage.get_device_metadata_response);
    }
}

void SimpleClient::gotDeviceMetadata(const meshtastic_DeviceMetadata &deviceMetadata)
{
    _firmwareVersion = deviceMetadata.firmware_version;
}

void SimpleClient::gotTelemetry(const meshtastic_MeshPacket &packet,
                                const meshtastic_Telemetry &telemetry)
{
    syncHostClock(telemetry.time);

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
    lock_guard<recursive_mutex> lock(_mutex);
    return _mtc.bytes_rx;
}

uint32_t SimpleClient::meshDeviceBytesSent(void) const
{
    lock_guard<recursive_mutex> lock(_mutex);
    return _mtc.bytes_tx;
}

uint32_t SimpleClient::meshDevicePacketsReceived(void) const
{
    lock_guard<recursive_mutex> lock(_mutex);
    return _mtc.packets_rx;
}

uint32_t SimpleClient::meshDevicePacketsSent(void) const
{
    lock_guard<recursive_mutex> lock(_mutex);
    return _mtc.packets_tx;
}

uint32_t SimpleClient::meshDeviceLastReceivedSecondsAgo(void) const
{
    lock_guard<recursive_mutex> lock(_mutex);
    time_t now = mt_impl_now();

    if (_mtc.last_packet_ts == 0 || now < (time_t) _mtc.last_packet_ts) {
        return 0;
    }

    if ((_mtc.last_packet_ts < 1700000000U) && (now >= 1700000000U)) {
        return 0;
    }

    return (uint32_t) (now - _mtc.last_packet_ts);
}



void SimpleClient::setNvm(shared_ptr<BaseNvm> nvm)
{
    _nvm = nvm;
    setupAgent();
}

static bool nvm_name_match(const char *authName, size_t authMaxLen, const string &chanName)
{
    size_t nlen = strnlen(authName, authMaxLen);
    if (chanName.size() != nlen) {
        return false;
    }
    return strncasecmp(authName, chanName.c_str(), nlen) == 0;
}

static bool is_in_authchan(const vector<struct nvm_authchan_entry> &authchans,
                           const string &chanName)
{
    if (chanName.empty()) {
        return false;
    }
    for (size_t i = 0; i < authchans.size(); i++) {
        if (nvm_name_match(authchans[i].name, sizeof(authchans[i].name), chanName)) {
            return true;
        }
    }
    return false;
}

static bool has_valid_key(const meshtastic_Channel &chan)
{
    if (!chan.has_settings) {
        return false;
    }
    if (chan.settings.psk.size == 0) {
        return false;
    }
    if (chan.settings.psk.size == 1 && chan.settings.psk.bytes[0] == 0) {
        return false;
    }
    return true;
}

void SimpleClient::setupAgent(void)
{
    lock_guard<recursive_mutex> lock(_mutex);

    _robotChannel = -1;

    if (_nvm == NULL) {
        return;
    }

    const vector<struct nvm_authchan_entry> &authchans = _nvm->nvmAuthchans();
    if (authchans.empty()) {
        return;
    }

    static const char *keywords[] = {
        "home",
        "robot",
        "automation",
        "assistant",
    };

    for (size_t k = 0; k < sizeof(keywords) / sizeof(keywords[0]); k++) {
        const char *kw = keywords[k];
        for (map<uint8_t, meshtastic_Channel>::const_iterator it = _channels.begin();
             it != _channels.end(); it++) {
            const meshtastic_Channel &chan = it->second;
            if (chan.role == meshtastic_Channel_Role_DISABLED || !has_valid_key(chan)) {
                continue;
            }
            string chanName = getChannelName(it->first);
            if (chanName.empty()) {
                chanName = chan.settings.name;
            }
            if (!is_in_authchan(authchans, chanName) &&
                !is_in_authchan(authchans, chan.settings.name)) {
                continue;
            }

            string chanNameLower = chanName;
            string kwStr = kw;
            for (size_t i = 0; i < chanNameLower.size(); i++) {
                chanNameLower[i] = tolower((unsigned char) chanNameLower[i]);
            }
            for (size_t i = 0; i < kwStr.size(); i++) {
                kwStr[i] = tolower((unsigned char) kwStr[i]);
            }
            if (chanNameLower.find(kwStr) != string::npos) {
                _robotChannel = it->first;
                goto resolved;
            }
        }
    }

    for (size_t a = 0; a < authchans.size(); a++) {
        for (map<uint8_t, meshtastic_Channel>::const_iterator it = _channels.begin();
             it != _channels.end(); it++) {
            const meshtastic_Channel &chan = it->second;
            if (chan.role == meshtastic_Channel_Role_DISABLED || !has_valid_key(chan)) {
                continue;
            }
            string chanName = getChannelName(it->first);
            if (chanName.empty()) {
                chanName = chan.settings.name;
            }
            if (nvm_name_match(authchans[a].name, sizeof(authchans[a].name), chanName) ||
                nvm_name_match(authchans[a].name, sizeof(authchans[a].name), chan.settings.name)) {
                _robotChannel = it->first;
                goto resolved;
            }
        }
    }

resolved:

    if (_robotChannel >= 0 && _isConnected && !_bootAnnounced) {
        string announcement = lookupLongName(whoami(), true);
        if (announcement.empty()) {
            announcement = whoamiString();
        }
        announcement += " is up";
        if (textMessage(0xffffffffU, (uint8_t) _robotChannel, announcement)) {
            _bootAnnounced = true;
        }
    }
}

void SimpleClient::houseKeeping(void)
{
    purgeOldNodes();

    time_t now = time(NULL);
    if (_lastHourlyTask == 0) {
        _lastHourlyTask = now;
    } else if (now < _lastHourlyTask) {
        _lastHourlyTask = now;
    } else if ((now - _lastHourlyTask) >= 3600) {
        hourlyTask();
        _lastHourlyTask = now;
    }
}

void SimpleClient::hourlyTask(void)
{
    int robotChan = getRobotChannel();
    if (robotChan < 0 || !_isConnected) {
        return;
    }

    stringstream ss;
    ss << fixed << setprecision(2);
    ss << "status";

    map<uint32_t, meshtastic_DeviceMetrics>::const_iterator dev =
        _deviceMetrics.find(whoami());
    if (dev != _deviceMetrics.end()) {
        if (dev->second.has_battery_level && dev->second.battery_level > 0 && dev->second.battery_level <= 100) {
            ss << " batt=" << dev->second.battery_level << "%";
        }
        if (dev->second.has_voltage && dev->second.voltage > 0.0f) {
            ss << " volt=" << dev->second.voltage << "V";
        }
        if (dev->second.has_channel_utilization) {
            ss << " ch_util=" << dev->second.channel_utilization << "%";
        }
        if (dev->second.has_air_util_tx) {
            ss << " air_tx=" << dev->second.air_util_tx << "%";
        }
    }

    map<uint32_t, meshtastic_EnvironmentMetrics>::const_iterator env =
        _environmentMetrics.find(whoami());
    if (env != _environmentMetrics.end()) {
        if (env->second.has_temperature) {
            ss << " temp=" << env->second.temperature << "C";
        }
        if (env->second.has_relative_humidity) {
            ss << " rh=" << env->second.relative_humidity << "%";
        }
        if (env->second.has_barometric_pressure) {
            ss << " press=" << env->second.barometric_pressure << "hPa";
        }
        if (env->second.has_gas_resistance) {
            ss << " gas=" << env->second.gas_resistance << "ohm";
        }
        if (env->second.has_iaq) {
            ss << " iaq=" << env->second.iaq;
        }
        if (env->second.has_lux) {
            ss << " lux=" << env->second.lux;
        }
    }

    map<uint32_t, meshtastic_AirQualityMetrics>::const_iterator aq =
        _airQualityMetrics.find(whoami());
    if (aq != _airQualityMetrics.end()) {
        if (aq->second.has_pm25_standard) {
            ss << " pm25=" << aq->second.pm25_standard;
        }
        if (aq->second.has_pm10_standard) {
            ss << " pm10=" << aq->second.pm10_standard;
        }
    }

    map<uint32_t, meshtastic_PowerMetrics>::const_iterator pwr =
        _powerMetrics.find(whoami());
    if (pwr != _powerMetrics.end()) {
        if (pwr->second.has_ch1_voltage) {
            ss << " ch1_v=" << pwr->second.ch1_voltage << "V";
        }
        if (pwr->second.has_ch1_current) {
            ss << " ch1_i=" << pwr->second.ch1_current << "mA";
        }
    }

    ss << " bytes=" << meshDeviceBytesReceived() << "/" << meshDeviceBytesSent();
    ss << " pkts=" << meshDevicePacketsReceived() << "/" << meshDevicePacketsSent();
    ss << " last=" << meshDeviceLastReceivedSecondsAgo() << "s";

    textMessage(0xffffffffU, (uint8_t) robotChan, ss.str());
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
