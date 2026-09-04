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
    bzero(&_positionConfig, sizeof(_positionConfig));
    bzero(&_powerConfig, sizeof(_powerConfig));
    bzero(&_networkConfig, sizeof(_networkConfig));
    bzero(&_displayConfig, sizeof(_displayConfig));
    bzero(&_bluetoothConfig, sizeof(_bluetoothConfig));
    bzero(&_securityConfig, sizeof(_securityConfig));
    bzero(&_sessionkeyConfig, sizeof(_sessionkeyConfig));
    bzero(&_queueStatus, sizeof(_queueStatus));
    bzero(&_deviceMetadata, sizeof(_deviceMetadata));
    bzero(&_deviceUIConfig, sizeof(_deviceUIConfig));
    bzero(&_modMQTT, sizeof(_modMQTT));
    bzero(&_modSerial, sizeof(_modSerial));
    bzero(&_modExternalNotification, sizeof(_modExternalNotification));
    bzero(&_modStoreForward, sizeof(_modStoreForward));
    bzero(&_modRangeTest, sizeof(_modRangeTest));
    bzero(&_modTelemetry, sizeof(_modTelemetry));
    bzero(&_modCannedMessage, sizeof(_modCannedMessage));
    bzero(&_modAudio, sizeof(_modAudio));
    bzero(&_modRemoteHardware, sizeof(_modRemoteHardware));
    bzero(&_modNeighborInfo, sizeof(_modNeighborInfo));
    bzero(&_modAmbientLighting, sizeof(_modAmbientLighting));
    bzero(&_modDetectionSensor, sizeof(_modDetectionSensor));
    bzero(&_modPaxcounter, sizeof(_modPaxcounter));
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
    bzero(&_positionConfig, sizeof(_positionConfig));
    bzero(&_powerConfig, sizeof(_powerConfig));
    bzero(&_networkConfig, sizeof(_networkConfig));
    bzero(&_displayConfig, sizeof(_displayConfig));
    bzero(&_bluetoothConfig, sizeof(_bluetoothConfig));
    bzero(&_securityConfig, sizeof(_securityConfig));
    bzero(&_sessionkeyConfig, sizeof(_sessionkeyConfig));
    bzero(&_queueStatus, sizeof(_queueStatus));
    bzero(&_deviceMetadata, sizeof(_deviceMetadata));
    bzero(&_deviceUIConfig, sizeof(_deviceUIConfig));
    _fileInfos.clear();
    bzero(&_modMQTT, sizeof(_modMQTT));
    bzero(&_modSerial, sizeof(_modSerial));
    bzero(&_modExternalNotification, sizeof(_modExternalNotification));
    bzero(&_modStoreForward, sizeof(_modStoreForward));
    bzero(&_modRangeTest, sizeof(_modRangeTest));
    bzero(&_modTelemetry, sizeof(_modTelemetry));
    bzero(&_modCannedMessage, sizeof(_modCannedMessage));
    bzero(&_modAudio, sizeof(_modAudio));
    bzero(&_modRemoteHardware, sizeof(_modRemoteHardware));
    bzero(&_modNeighborInfo, sizeof(_modNeighborInfo));
    bzero(&_modAmbientLighting, sizeof(_modAmbientLighting));
    bzero(&_modDetectionSensor, sizeof(_modDetectionSensor));
    bzero(&_modPaxcounter, sizeof(_modPaxcounter));
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

unsigned int SimpleClient::hopsAway(uint32_t node_num) const
{
    lock_guard<recursive_mutex> lock(_mutex);
    uint8_t hops = 0xffU;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    it = _nodeInfos.find(node_num);
    if ((it != _nodeInfos.end()) && it->second.has_hops_away) {
        hops = it->second.hops_away;
    }

    return (unsigned int) hops;
}

unsigned int SimpleClient::hopsAway(const meshtastic_MeshPacket &packet) const
{
    if (packet.hop_start >= packet.hop_limit) {
        return (unsigned int)(packet.hop_start - packet.hop_limit);
    }

    return hopsAway(packet.from);
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
    case meshtastic_FromRadio_moduleConfig_tag:
        sc->gotModuleConfig(fromRadio->moduleConfig);
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
    case meshtastic_FromRadio_queueStatus_tag:
        sc->gotQueueStatus(fromRadio->queueStatus);
        break;
    case meshtastic_FromRadio_metadata_tag:
        sc->gotDeviceMetadata(fromRadio->metadata);
        break;
    case meshtastic_FromRadio_fileInfo_tag:
        sc->gotFileInfo(fromRadio->fileInfo);
        break;
    case meshtastic_FromRadio_deviceuiConfig_tag:
        sc->gotDeviceUIConfig(fromRadio->deviceuiConfig);
        break;
    case meshtastic_FromRadio_mqttClientProxyMessage_tag:
        sc->gotMqttClientProxyMessage(fromRadio->mqttClientProxyMessage);
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

vector<meshtastic_NodeInfo> SimpleClient::getLastHeardNodes(uint32_t seconds) const
{
    lock_guard<recursive_mutex> lock(_mutex);
    vector<meshtastic_NodeInfo> nodes;
    time_t now = time(NULL);

    for (map<uint32_t, meshtastic_NodeInfo>::const_iterator it = _nodeInfos.begin();
         it != _nodeInfos.end(); it++) {
        const meshtastic_NodeInfo &info = it->second;
        if (seconds == 0) {
            nodes.push_back(info);
        } else if (info.last_heard > 0) {
            uint32_t diff;
            if (now >= (time_t) info.last_heard) {
                diff = (uint32_t)(now - (time_t) info.last_heard);
            } else {
                diff = (uint32_t)((time_t) info.last_heard - now);
            }

            if (diff <= seconds) {
                nodes.push_back(info);
            }
        }
    }

    return nodes;
}

bool SimpleClient::isProtectedNode(uint32_t nodeId) const
{
    if (nodeId == 0 || nodeId == 0xffffffffU || nodeId == whoami()) {
        return true;
    }

    if (_nvm != NULL) {
        for (const auto &admin : _nvm->nvmAdmins()) {
            if (admin.node_num == nodeId) {
                return true;
            }
        }
        for (const auto &mate : _nvm->nvmMates()) {
            if (mate.node_num == nodeId) {
                return true;
            }
        }
    }

    return false;
}

bool SimpleClient::purgeNode(uint32_t nodeId)
{
    if (isProtectedNode(nodeId)) {
        return false;
    }

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
        if (isProtectedNode(nodeId)) {
            continue;
        }

        const meshtastic_NodeInfo &info = it->second;
        uint32_t lastHeard = info.last_heard;
        bool qualifies = false;

        if (lastHeard > 0 && now >= (time_t) lastHeard) {
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

bool SimpleClient::adminSetUsePreset(bool use_preset, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_LoRaConfig c;
    if (dest == whoami()) {
        c = _loraConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.use_preset = use_preset;

    return sendLoraConfig(dest, c);
}

bool SimpleClient::adminSetModemPreset(meshtastic_Config_LoRaConfig_ModemPreset preset,
                                  uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_LoRaConfig c;
    if (dest == whoami()) {
        c = _loraConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.modem_preset = preset;

    return sendLoraConfig(dest, c);
}

bool SimpleClient::adminSetRegion(meshtastic_Config_LoRaConfig_RegionCode region,
                             uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_LoRaConfig c;
    if (dest == whoami()) {
        c = _loraConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.region = region;

    return sendLoraConfig(dest, c);
}

bool SimpleClient::adminSetHopLimit(uint32_t hop_limit, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_LoRaConfig c;
    if (dest == whoami()) {
        c = _loraConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.hop_limit = hop_limit;

    return sendLoraConfig(dest, c);
}

bool SimpleClient::adminSetTxEnabled(bool tx_enabled, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_LoRaConfig c;
    if (dest == whoami()) {
        c = _loraConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.tx_enabled = tx_enabled;

    return sendLoraConfig(dest, c);
}

bool SimpleClient::adminSetTxPower(int8_t tx_power, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_LoRaConfig c;
    if (dest == whoami()) {
        c = _loraConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.tx_power = tx_power;

    return sendLoraConfig(dest, c);
}

bool SimpleClient::adminSetChannelNum(uint16_t channel_num, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_LoRaConfig c;
    if (dest == whoami()) {
        c = _loraConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.channel_num = channel_num;

    return sendLoraConfig(dest, c);
}

bool SimpleClient::adminSetIgnoreMqtt(bool ignore_mqtt, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_LoRaConfig c;
    if (dest == whoami()) {
        c = _loraConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.ignore_mqtt = ignore_mqtt;

    return sendLoraConfig(dest, c);
}

bool SimpleClient::sendLoraConfig(uint32_t dest, const meshtastic_Config_LoRaConfig &c)
{
    if (mt_admin_message_set_lora_config(&_mtc, dest, &c) != 0) {
        return false;
    }
    if (dest == whoami()) {
        _loraConfig = c;
    }
    return true;
}

bool SimpleClient::adminSetRole(meshtastic_Config_DeviceConfig_Role role, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_DeviceConfig c;
    if (dest == whoami()) {
        c = _deviceConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.role = role;

    return sendDeviceConfig(dest, c);
}

bool SimpleClient::adminSetRebroadcastMode(meshtastic_Config_DeviceConfig_RebroadcastMode mode,
                                      uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_DeviceConfig c;
    if (dest == whoami()) {
        c = _deviceConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.rebroadcast_mode = mode;

    return sendDeviceConfig(dest, c);
}

bool SimpleClient::adminSetNodeInfoBroadcastSecs(uint32_t seconds, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_DeviceConfig c;
    if (dest == whoami()) {
        c = _deviceConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.node_info_broadcast_secs = seconds;

    return sendDeviceConfig(dest, c);
}

bool SimpleClient::adminSetTimezone(const string &tzdef, uint32_t dest)
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
    }

    return true;
}

bool SimpleClient::sendDeviceConfig(uint32_t dest, const meshtastic_Config_DeviceConfig &c)
{
    if (mt_admin_message_set_device_config(&_mtc, dest, &c) != 0) {
        return false;
    }
    if (dest == whoami()) {
        _deviceConfig = c;
    }
    return true;
}

bool SimpleClient::adminSetPositionBroadcastSecs(uint32_t seconds, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_PositionConfig c;
    if (dest == whoami()) {
        c = _positionConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.position_broadcast_secs = seconds;

    return sendPositionConfig(dest, c);
}

bool SimpleClient::sendPositionConfig(uint32_t dest, const meshtastic_Config_PositionConfig &c)
{
    if (mt_admin_message_set_position_config(&_mtc, dest, &c) != 0) {
        return false;
    }
    if (dest == whoami()) {
        _positionConfig = c;
    }
    return true;
}

bool SimpleClient::adminSetPublicKey(const meshtastic_Config_SecurityConfig_public_key_t &key,
                                uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_SecurityConfig c;
    if (dest == whoami()) {
        c = _securityConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.public_key = key;

    return sendSecurityConfig(dest, c);
}

bool SimpleClient::adminSetPrivateKey(const meshtastic_Config_SecurityConfig_private_key_t &key,
                                 uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_SecurityConfig c;
    if (dest == whoami()) {
        c = _securityConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.private_key = key;

    return sendSecurityConfig(dest, c);
}

bool SimpleClient::adminSetAdminKey(uint8_t slot,
                               const meshtastic_Config_SecurityConfig_admin_key_t &key,
                               uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (slot > 2) {
        return false;
    }

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_SecurityConfig c;
    if (dest == whoami()) {
        c = _securityConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.admin_key[slot] = key;
    if (key.size > 0) {
        if (c.admin_key_count < (pb_size_t)(slot + 1)) {
            c.admin_key_count = (pb_size_t)(slot + 1);
        }
    } else if (c.admin_key_count == (pb_size_t)(slot + 1)) {
        while (c.admin_key_count > 0 &&
               c.admin_key[c.admin_key_count - 1].size == 0) {
            c.admin_key_count--;
        }
    }

    return sendSecurityConfig(dest, c);
}

bool SimpleClient::adminSetIsManaged(bool is_managed, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_SecurityConfig c;
    if (dest == whoami()) {
        c = _securityConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.is_managed = is_managed;

    return sendSecurityConfig(dest, c);
}

bool SimpleClient::adminSetAdminChannelEnabled(bool enabled, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Config_SecurityConfig c;
    if (dest == whoami()) {
        c = _securityConfig;
    } else {
        bzero(&c, sizeof(c));
    }
    c.admin_channel_enabled = enabled;

    return sendSecurityConfig(dest, c);
}

bool SimpleClient::sendSecurityConfig(uint32_t dest, const meshtastic_Config_SecurityConfig &c)
{
    if (mt_admin_message_set_security_config(&_mtc, dest, &c) != 0) {
        return false;
    }
    if (dest == whoami()) {
        _securityConfig = c;
    }
    return true;
}

bool SimpleClient::adminSetChannelPsk(uint8_t index, const meshtastic_ChannelSettings_psk_t &psk,
                                 uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Channel c;
    bzero(&c, sizeof(c));
    c.index = (int8_t) index;
    if (dest == whoami()) {
        map<uint8_t, meshtastic_Channel>::const_iterator it = _channels.find(index);
        if (it != _channels.end()) {
            c = it->second;
        }
    }
    c.has_settings = true;
    c.settings.psk = psk;

    return sendChannel(dest, c);
}

bool SimpleClient::adminSetChannelName(uint8_t index, const string &name, uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Channel c;
    bzero(&c, sizeof(c));
    c.index = (int8_t) index;
    if (dest == whoami()) {
        map<uint8_t, meshtastic_Channel>::const_iterator it = _channels.find(index);
        if (it != _channels.end()) {
            c = it->second;
        }
    }
    c.has_settings = true;
    strncpy(c.settings.name, name.c_str(), sizeof(c.settings.name) - 1);
    c.settings.name[sizeof(c.settings.name) - 1] = '\0';

    return sendChannel(dest, c);
}

bool SimpleClient::adminSetChannelUplinkEnabled(uint8_t index, bool uplink_enabled,
                                           uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Channel c;
    bzero(&c, sizeof(c));
    c.index = (int8_t) index;
    if (dest == whoami()) {
        map<uint8_t, meshtastic_Channel>::const_iterator it = _channels.find(index);
        if (it != _channels.end()) {
            c = it->second;
        }
    }
    c.has_settings = true;
    c.settings.uplink_enabled = uplink_enabled;

    return sendChannel(dest, c);
}

bool SimpleClient::adminSetChannelDownlinkEnabled(uint8_t index, bool downlink_enabled,
                                             uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Channel c;
    bzero(&c, sizeof(c));
    c.index = (int8_t) index;
    if (dest == whoami()) {
        map<uint8_t, meshtastic_Channel>::const_iterator it = _channels.find(index);
        if (it != _channels.end()) {
            c = it->second;
        }
    }
    c.has_settings = true;
    c.settings.downlink_enabled = downlink_enabled;

    return sendChannel(dest, c);
}

bool SimpleClient::adminSetChannelRole(uint8_t index, meshtastic_Channel_Role role,
                                  uint32_t dest)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    meshtastic_Channel c;
    bzero(&c, sizeof(c));
    c.index = (int8_t) index;
    if (dest == whoami()) {
        map<uint8_t, meshtastic_Channel>::const_iterator it = _channels.find(index);
        if (it != _channels.end()) {
            c = it->second;
        }
    }
    c.role = role;

    return sendChannel(dest, c);
}

bool SimpleClient::sendChannel(uint32_t dest, const meshtastic_Channel &c)
{
    if (mt_admin_message_set_channel(&_mtc, dest, &c) != 0) {
        return false;
    }
    if (dest == whoami()) {
        _channels[(uint8_t) c.index] = c;
    }
    return true;
}

bool SimpleClient::sendAdminMessage(const meshtastic_AdminMessage &msg,
                                    uint32_t dest, bool want_response)
{
    lock_guard<recursive_mutex> lock(_mutex);

    if (dest == 0) {
        dest = whoami();
    }

    return (mt_send_admin_message(&_mtc, dest, &msg, want_response) == 0);
}

bool SimpleClient::getChannelRequest(uint8_t index, uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_get_channel_request_tag;
    msg.get_channel_request = (uint32_t) index + 1;

    return sendAdminMessage(msg, dest, true);
}

bool SimpleClient::getOwnerRequest(uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_get_owner_request_tag;
    msg.get_owner_request = true;

    return sendAdminMessage(msg, dest, true);
}

bool SimpleClient::getConfigRequest(meshtastic_AdminMessage_ConfigType type,
                                    uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_get_config_request_tag;
    msg.get_config_request = type;

    return sendAdminMessage(msg, dest, true);
}

bool SimpleClient::getDeviceMetadataRequest(uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant =
        meshtastic_AdminMessage_get_device_metadata_request_tag;
    msg.get_device_metadata_request = true;

    return sendAdminMessage(msg, dest, true);
}

bool SimpleClient::enterDfuMode(uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant =
        meshtastic_AdminMessage_enter_dfu_mode_request_tag;
    msg.enter_dfu_mode_request = true;

    return sendAdminMessage(msg, dest, false);
}

bool SimpleClient::adminSetTime(uint32_t seconds, uint32_t dest)
{
    meshtastic_AdminMessage msg;

    if (seconds == 0) {
        seconds = (uint32_t) time(NULL);
    }

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_set_time_only_tag;
    msg.set_time_only = seconds;

    return sendAdminMessage(msg, dest, false);
}

bool SimpleClient::beginEditSettings(uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_begin_edit_settings_tag;
    msg.begin_edit_settings = true;

    return sendAdminMessage(msg, dest, false);
}

bool SimpleClient::commitEditSettings(uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_commit_edit_settings_tag;
    msg.commit_edit_settings = true;

    return sendAdminMessage(msg, dest, false);
}

bool SimpleClient::factoryResetDevice(uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_factory_reset_device_tag;
    msg.factory_reset_device = 1;

    return sendAdminMessage(msg, dest, false);
}

bool SimpleClient::rebootOta(uint32_t seconds, uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_reboot_ota_seconds_tag;
    msg.reboot_ota_seconds = (int32_t) seconds;

    return sendAdminMessage(msg, dest, false);
}

bool SimpleClient::adminMessageReboot(unsigned int seconds, uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_reboot_seconds_tag;
    msg.reboot_seconds = (int32_t) seconds;

    return sendAdminMessage(msg, dest, false);
}

bool SimpleClient::shutdown(uint32_t seconds, uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_shutdown_seconds_tag;
    msg.shutdown_seconds = (int32_t) seconds;

    return sendAdminMessage(msg, dest, false);
}

bool SimpleClient::factoryResetConfig(uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_factory_reset_config_tag;
    msg.factory_reset_config = 1;

    return sendAdminMessage(msg, dest, false);
}

bool SimpleClient::resetNodeDb(uint32_t dest)
{
    meshtastic_AdminMessage msg;

    bzero(&msg, sizeof(msg));
    msg.which_payload_variant = meshtastic_AdminMessage_nodedb_reset_tag;
    msg.nodedb_reset = 1;

    return sendAdminMessage(msg, dest, false);
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
    case meshtastic_Config_position_tag:
        gotPositionConfig(config.payload_variant.position);
        break;
    case meshtastic_Config_power_tag:
        gotPowerConfig(config.payload_variant.power);
        break;
    case meshtastic_Config_network_tag:
        gotNetworkConfig(config.payload_variant.network);
        break;
    case meshtastic_Config_display_tag:
        gotDisplayConfig(config.payload_variant.display);
        break;
    case meshtastic_Config_bluetooth_tag:
        gotBluetoothConfig(config.payload_variant.bluetooth);
        break;
    case meshtastic_Config_security_tag:
        gotSecurityConfig(config.payload_variant.security);
        break;
    case meshtastic_Config_sessionkey_tag:
        gotSessionkeyConfig(config.payload_variant.sessionkey);
        break;
    case meshtastic_Config_device_ui_tag:
        gotDeviceUIConfig(config.payload_variant.device_ui);
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

void SimpleClient::gotPositionConfig(const meshtastic_Config_PositionConfig &c)
{
    _positionConfig = c;
}

void SimpleClient::gotPowerConfig(const meshtastic_Config_PowerConfig &c)
{
    _powerConfig = c;
}

void SimpleClient::gotNetworkConfig(const meshtastic_Config_NetworkConfig &c)
{
    _networkConfig = c;
}

void SimpleClient::gotDisplayConfig(const meshtastic_Config_DisplayConfig &c)
{
    _displayConfig = c;
}

void SimpleClient::gotBluetoothConfig(const meshtastic_Config_BluetoothConfig &c)
{
    _bluetoothConfig = c;
}

void SimpleClient::gotSecurityConfig(const meshtastic_Config_SecurityConfig &c)
{
    _securityConfig = c;
}

void SimpleClient::gotSessionkeyConfig(const meshtastic_Config_SessionkeyConfig &c)
{
    _sessionkeyConfig = c;
}

void SimpleClient::gotModuleConfig(const meshtastic_ModuleConfig &moduleConfig)
{
    switch (moduleConfig.which_payload_variant) {
    case meshtastic_ModuleConfig_mqtt_tag:
        gotModuleConfigMQTT(moduleConfig.payload_variant.mqtt);
        break;
    case meshtastic_ModuleConfig_serial_tag:
        gotModuleConfigSerial(moduleConfig.payload_variant.serial);
        break;
    case meshtastic_ModuleConfig_external_notification_tag:
        gotModuleConfigExternalNotification(moduleConfig.payload_variant.external_notification);
        break;
    case meshtastic_ModuleConfig_store_forward_tag:
        gotModuleConfigStoreForward(moduleConfig.payload_variant.store_forward);
        break;
    case meshtastic_ModuleConfig_range_test_tag:
        gotModuleConfigRangeTest(moduleConfig.payload_variant.range_test);
        break;
    case meshtastic_ModuleConfig_telemetry_tag:
        gotModuleConfigTelemetry(moduleConfig.payload_variant.telemetry);
        break;
    case meshtastic_ModuleConfig_canned_message_tag:
        gotModuleConfigCannedMessage(moduleConfig.payload_variant.canned_message);
        break;
    case meshtastic_ModuleConfig_audio_tag:
        gotModuleConfigAudio(moduleConfig.payload_variant.audio);
        break;
    case meshtastic_ModuleConfig_remote_hardware_tag:
        gotModuleConfigRemoteHardware(moduleConfig.payload_variant.remote_hardware);
        break;
    case meshtastic_ModuleConfig_neighbor_info_tag:
        gotModuleConfigNeighborInfo(moduleConfig.payload_variant.neighbor_info);
        break;
    case meshtastic_ModuleConfig_ambient_lighting_tag:
        gotModuleConfigAmbientLighting(moduleConfig.payload_variant.ambient_lighting);
        break;
    case meshtastic_ModuleConfig_detection_sensor_tag:
        gotModuleConfigDetectionSensor(moduleConfig.payload_variant.detection_sensor);
        break;
    case meshtastic_ModuleConfig_paxcounter_tag:
        gotModuleConfigPaxcounter(moduleConfig.payload_variant.paxcounter);
        break;
    default:
        break;
    }
}

void SimpleClient::gotModuleConfigMQTT(const meshtastic_ModuleConfig_MQTTConfig &c)
{
    _modMQTT = c;
}

void SimpleClient::gotModuleConfigSerial(const meshtastic_ModuleConfig_SerialConfig &c)
{
    _modSerial = c;
}

void SimpleClient::gotModuleConfigExternalNotification(const meshtastic_ModuleConfig_ExternalNotificationConfig &c)
{
    _modExternalNotification = c;
}

void SimpleClient::gotModuleConfigStoreForward(const meshtastic_ModuleConfig_StoreForwardConfig &c)
{
    _modStoreForward = c;
}

void SimpleClient::gotModuleConfigRangeTest(const meshtastic_ModuleConfig_RangeTestConfig &c)
{
    _modRangeTest = c;
}

void SimpleClient::gotModuleConfigTelemetry(const meshtastic_ModuleConfig_TelemetryConfig &c)
{
    _modTelemetry = c;
}

void SimpleClient::gotModuleConfigCannedMessage(const meshtastic_ModuleConfig_CannedMessageConfig &c)
{
    _modCannedMessage = c;
}

void SimpleClient::gotModuleConfigAudio(const meshtastic_ModuleConfig_AudioConfig &c)
{
    _modAudio = c;
}

void SimpleClient::gotModuleConfigRemoteHardware(const meshtastic_ModuleConfig_RemoteHardwareConfig &c)
{
    _modRemoteHardware = c;
}

void SimpleClient::gotModuleConfigNeighborInfo(const meshtastic_ModuleConfig_NeighborInfoConfig &c)
{
    _modNeighborInfo = c;
}

void SimpleClient::gotModuleConfigAmbientLighting(const meshtastic_ModuleConfig_AmbientLightingConfig &c)
{
    _modAmbientLighting = c;
}

void SimpleClient::gotModuleConfigDetectionSensor(const meshtastic_ModuleConfig_DetectionSensorConfig &c)
{
    _modDetectionSensor = c;
}

void SimpleClient::gotModuleConfigPaxcounter(const meshtastic_ModuleConfig_PaxcounterConfig &c)
{
    _modPaxcounter = c;
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
        string announcement = "boot-up: " + lookupShortName(whoami(), true);
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
    _deviceMetadata = deviceMetadata;
    _firmwareVersion = deviceMetadata.firmware_version;
}

void SimpleClient::gotQueueStatus(const meshtastic_QueueStatus &queueStatus)
{
    _queueStatus = queueStatus;
}

void SimpleClient::gotFileInfo(const meshtastic_FileInfo &fileInfo)
{
    if (fileInfo.file_name[0] != '\0') {
        _fileInfos[string(fileInfo.file_name)] = fileInfo;
    }
}

void SimpleClient::gotDeviceUIConfig(const meshtastic_DeviceUIConfig &deviceUIConfig)
{
    _deviceUIConfig = deviceUIConfig;
}

void SimpleClient::gotMqttClientProxyMessage(const meshtastic_MqttClientProxyMessage &m)
{
    (void)(m);
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
        string announcement = "boot-up: " + lookupShortName(whoami(), true);
        if (textMessage(0xffffffffU, (uint8_t) _robotChannel, announcement)) {
            _bootAnnounced = true;
        }
    }
}

void SimpleClient::houseKeeping(void)
{
    purgeOldNodes();

    uint32_t uptime = getUptime();
    uint32_t last;
    {
        lock_guard<recursive_mutex> lock(_mutex);
        last = (uint32_t) _lastHourlyTask;
    }
    if (uptime >= last && (uptime - last) >= 3600) {
        hourlyTask();
        lock_guard<recursive_mutex> lock(_mutex);
        _lastHourlyTask = (time_t) getUptime();
    }
}

void SimpleClient::hourlyTask(void)
{
    int robotChan = getRobotChannel();
    if (robotChan < 0 || !_isConnected) {
        return;
    }

    uint32_t upsec;
    unsigned int days, hour, min, sec;
    char buf[64];

    upsec = getUptime();
    sec = upsec % 60;
    min = (upsec / 60) % 60;
    hour = (upsec / 3600) % 24;
    days = (upsec) / 86400;
    if (days == 0) {
        snprintf(buf, sizeof(buf) - 1, "uptime: %.2u:%.2u:%.2u",
                 hour, min, sec);
    } else {
        snprintf(buf, sizeof(buf) - 1, "uptime: %ud %.2u:%.2u:%.2u",
                 days, hour, min, sec);
    }

    textMessage(0xffffffffU, (uint8_t) robotChan, buf);
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
