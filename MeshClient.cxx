/*
 * MeshClient.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <sstream>
#include <iostream>
#include <iomanip>
#include <LibMeshtastic.hxx>

#define DEFAULT_HEARTBEAT_SECONDS 30

MeshClient::MeshClient()
{
    _verbose = false;
    _logStderr = false;
    _heartbeatSeconds = DEFAULT_HEARTBEAT_SECONDS;
    _mtc.fd = -1;
    _mtc.device = NULL;
    _mtc.inbuf_len = 0;
    _mtc.handler = mtEvent;
    _mtc.logger = logEvent;
    _mtc.ctx = this;
    _thread = NULL;
    _isRunning = false;
}

MeshClient::~MeshClient()
{
    stop();
}

bool MeshClient::attachSerial(string device)
{
    bool result = false;

    if (mt_serial_attach(&_mtc, device.c_str()) != 0) {
        goto done;
    }

    _isRunning = true;
    _thread = make_shared<thread>(thread_function, this);

    result = true;

done:

    return result;
}

void MeshClient::detach(void)
{
    stop();
}

void MeshClient::join(void)
{
    if (_thread != NULL) {
        if (_thread->joinable()) {
            _thread->join();
        }
    }
}

bool MeshClient::verbose(void) const
{
    return _verbose;
}

void MeshClient::setVerbose(bool verbose)
{
    _verbose = verbose;
}

bool MeshClient::logStderr(void) const
{
    return _logStderr;
}

void MeshClient::enableLogStderr(bool enable)
{
    _logStderr = enable;
}

unsigned int MeshClient::heartbeatSeconds(void) const
{
    return _heartbeatSeconds;
}

void MeshClient::setHeartbeatSeconds(unsigned int seconds)
{
    _heartbeatSeconds = seconds;
}

bool MeshClient::sendDisconnect(void)
{
    bool result = false;

    _mutex.lock();
    result = (mt_send_disconnect(&_mtc) == 0);
    _mutex.unlock();

    return result;
}

bool MeshClient::sendWantConfig(void)
{
    bool result = false;

    _mutex.lock();
    result = (mt_send_want_config(&_mtc) == 0);
    _mutex.unlock();

    return result;
}

bool MeshClient::sendHeartbeat(void)
{
    bool result = false;

    _mutex.lock();
    result = (mt_send_heartbeat(&_mtc) == 0);
    _mutex.unlock();

    return result;
}

bool MeshClient::textMessage(uint32_t dest, uint8_t channel,
                             const string &message,
                             unsigned int hop_start, bool want_ack)
{
    bool result = false;

    if (hop_start == 0) {
        hop_start = _loraConfig.hop_limit;
    }

    _mutex.lock();
    result = (mt_text_message(&_mtc, dest, channel,
                              message.c_str(),
                              hop_start, want_ack) == 0);
    _mutex.unlock();

    return result;
}

bool MeshClient::adminMessageReboot(unsigned int seconds)
{
    bool result = false;

    _mutex.lock();
    result = (mt_admin_message_reboot(&_mtc, seconds) == 0);
    _mutex.unlock();

    return result;
}

uint32_t MeshClient::whoami(void) const
{
    return _myNodeInfo.my_node_num;
}

string MeshClient::lookupLongName(uint32_t id) const
{
    string s;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    if (id == 0xffffffffU) {
        return "broadcast";
    }

    it = _nodeInfos.find(id);
    if (it != _nodeInfos.end()) {
        s = it->second.user.long_name;
    }

    return s;
}

string MeshClient::lookupShortName(uint32_t id) const
{
    string s;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    if (id == 0xffffffffU) {
        return "broadcast";
    }

    it = _nodeInfos.find(id);
    if (it != _nodeInfos.end()) {
        s = it->second.user.short_name;
    }

    return s;
}

string MeshClient::getDisplayName(uint32_t id) const
{
    stringstream ss;

    ss << lookupShortName(id) << " (!" << hex << setfill('0') << setw(8)
       << id << ")";

    return ss.str();
}

string MeshClient::getChannelName(uint8_t channel) const
{
    string name;
    map<uint8_t, meshtastic_Channel>::const_iterator it;

    it = _channels.find(channel);
    if (it != _channels.end()) {
        if (it->second.has_settings) {
            name = it->second.settings.name;
        }
    }

    return name;
}

void MeshClient::mtEvent(struct mt_client *mtc,
                         const void *packet, size_t size,
                         const meshtastic_FromRadio *fromRadio)
{
    MeshClient *client = (MeshClient *) mtc->ctx;
    (void)(packet);
    (void)(size);

    switch (fromRadio->which_payload_variant) {
    case meshtastic_FromRadio_packet_tag:
        client->gotPacket(fromRadio->packet);
        break;
    case meshtastic_FromRadio_my_info_tag:
        client->gotMyNodeInfo(fromRadio->my_info);
        break;
    case meshtastic_FromRadio_node_info_tag:
        client->gotNodeInfo(fromRadio->node_info);
        break;
    case meshtastic_FromRadio_config_tag :
        client->gotConfig(fromRadio->config);
        break;
    case  meshtastic_FromRadio_moduleConfig_tag:
        client->gotModuleConfig(fromRadio->moduleConfig);
        break;
    case meshtastic_FromRadio_channel_tag:
        client->gotChannel(fromRadio->channel);
        break;
    case meshtastic_FromRadio_config_complete_id_tag:
        client->gotConfigCompleteId(fromRadio->config_complete_id);
        break;
    case meshtastic_FromRadio_queueStatus_tag:
        client->gotQueueStatus(fromRadio->queueStatus);
        break;
    case meshtastic_FromRadio_metadata_tag:
        client->gotDeviceMetadata(fromRadio->metadata);
        break;
    case meshtastic_FromRadio_fileInfo_tag:
        client->gotFileInfo(fromRadio->fileInfo);
        break;
    case meshtastic_FromRadio_deviceuiConfig_tag:
        client->gotDeviceUIConfig(fromRadio->deviceuiConfig);
        break;
    default:
        cout << "Unhandled radio variant: "
             << fromRadio->which_payload_variant << endl;
        break;
    }
}

void MeshClient::logEvent(struct mt_client *mtc, const char *msg, size_t size)
{
    MeshClient *client = (MeshClient *) mtc->ctx;
    if (client->_logStderr) {
        fwrite(msg, 1, size, stderr);
        fflush(stderr);
    }
}

void MeshClient::gotPacket(const meshtastic_MeshPacket &packet)
{
    int ret;
    pb_istream_t stream;

    if (_verbose) {
        cout << packet;
    }

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
            } else {
                cerr << "pb_decode position failed!" << endl;
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
            } else {
                cerr << "pb_decode user failed!" << endl;
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
            } else {
                cerr << "pb_decode routing failed!" << endl;
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
            } else {
                cerr << "pb_decode telmetry failed!" << endl;
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
            } else {
                cerr << "pb_decode routeDiscovery failed!" << endl;
            }
        }
            break;
        default:
            cout << "Unhandled portnum: "
                 << packet.decoded.portnum << endl;
            break;
        }
    }
}

void MeshClient::gotMyNodeInfo(const meshtastic_MyNodeInfo &myNodeInfo)
{
    _myNodeInfo = myNodeInfo;

    if (_verbose) {
        cout << _myNodeInfo;
    }
}

void MeshClient::gotNodeInfo(const meshtastic_NodeInfo &nodeInfo)
{
    uint32_t num = nodeInfo.num;

    _nodeInfos[num] = nodeInfo;

    if (_verbose) {
        cout << _nodeInfos[num];
    }
}

void MeshClient::gotConfig(const meshtastic_Config &config)
{
    switch (config.which_payload_variant) {
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
    case meshtastic_Config_lora_tag:
        gotLoraConfig(config.payload_variant.lora);
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

    if (_verbose) {
        cout << config;
    }
}

void MeshClient::gotDeviceConfig(const meshtastic_Config_DeviceConfig &c)
{
    _deviceConfig = c;
}

void MeshClient::gotPositionConfig(const meshtastic_Config_PositionConfig &c)
{
    _positionConfig = c;
}

void MeshClient::gotPowerConfig(const meshtastic_Config_PowerConfig &c)
{
    _powerConfig = c;
}

void MeshClient::gotNetworkConfig(const meshtastic_Config_NetworkConfig &c)
{
    _networkConfig = c;
}

void MeshClient::gotDisplayConfig(const meshtastic_Config_DisplayConfig &c)
{
    _displayConfig = c;
}

void MeshClient::gotLoraConfig(const meshtastic_Config_LoRaConfig &c)
{
    _loraConfig = c;
}

void MeshClient::gotBluetoothConfig(const meshtastic_Config_BluetoothConfig &c)
{
    _bluetoothConfig = c;
}

void MeshClient::gotSecurityConfig(const meshtastic_Config_SecurityConfig &c)
{
    _securityConfig = c;
}

void MeshClient::gotSessionkeyConfig(const meshtastic_Config_SessionkeyConfig &c)
{
    _sessionkeyConfig = c;
}

void MeshClient::gotModuleConfig(const meshtastic_ModuleConfig &moduleConfig)
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

    if (_verbose) {
        cout << moduleConfig;
    }
}

void MeshClient::gotModuleConfigMQTT(const meshtastic_ModuleConfig_MQTTConfig &c)
{
    _modMQTT = c;
}

void MeshClient::gotModuleConfigSerial(const meshtastic_ModuleConfig_SerialConfig &c)
{
    _modSerial = c;
}

void MeshClient::gotModuleConfigExternalNotification(const meshtastic_ModuleConfig_ExternalNotificationConfig &c)
{
    _modExternalNotification = c;
}

void MeshClient::gotModuleConfigStoreForward(const meshtastic_ModuleConfig_StoreForwardConfig &c)
{
    _modStoreForward = c;
}

void MeshClient::gotModuleConfigRangeTest(const meshtastic_ModuleConfig_RangeTestConfig &c)
{
    _modRangeTest = c;
}

void MeshClient::gotModuleConfigTelemetry(const meshtastic_ModuleConfig_TelemetryConfig &c)
{
    _modTelemetry = c;
}

void MeshClient::gotModuleConfigCannedMessage(const meshtastic_ModuleConfig_CannedMessageConfig &c)
{
    _modCannedMessage = c;
}

void MeshClient::gotModuleConfigAudio(const meshtastic_ModuleConfig_AudioConfig &c)
{
    _modAudio = c;
}

void MeshClient::gotModuleConfigRemoteHardware(const meshtastic_ModuleConfig_RemoteHardwareConfig &c)
{
    _modRemoteHardware = c;
}

void MeshClient::gotModuleConfigNeighborInfo(const meshtastic_ModuleConfig_NeighborInfoConfig &c)
{
    _modNeighborInfo = c;
}

void MeshClient::gotModuleConfigAmbientLighting(const meshtastic_ModuleConfig_AmbientLightingConfig &c)
{
    _modAmbientLighting = c;
}

void MeshClient::gotModuleConfigDetectionSensor(const meshtastic_ModuleConfig_DetectionSensorConfig &c)
{
    _modDetectionSensor = c;
}

void MeshClient::gotModuleConfigPaxcounter(const meshtastic_ModuleConfig_PaxcounterConfig &c)
{
    _modPaxcounter = c;
}

void MeshClient::gotChannel(const meshtastic_Channel &channel)
{
    uint8_t index = channel.index;

    _channels[index] = channel;

    if (_verbose) {
        cout << _channels[index];
    }
}

void MeshClient::gotConfigCompleteId(uint32_t id)
{
    if (_verbose) {
        cout << "ConfigCompleteId: 0x"
             << hex << setfill('0') << setw(8) << id << dec << endl;
    }
}

void MeshClient::gotQueueStatus(const meshtastic_QueueStatus &queueStatus)
{
    _queueStatus = queueStatus;

    if (_verbose) {
        cout << _queueStatus;
    }
}

void MeshClient::gotDeviceMetadata(const meshtastic_DeviceMetadata &deviceMetadata)
{
    _deviceMetadata = deviceMetadata;
    if (_verbose) {
        cout << _deviceMetadata;
    }
}

void MeshClient::gotFileInfo(const meshtastic_FileInfo &fileInfo)
{
    if (fileInfo.file_name[0] != '\0') {
        _fileInfos[string(fileInfo.file_name)] = fileInfo;
        if (_verbose) {
            cout << fileInfo;
        }
    }
}

void MeshClient::gotDeviceUIConfig(const meshtastic_DeviceUIConfig &deviceUIConfig)
{
    _deviceUIConfig = deviceUIConfig;
    if (_verbose) {
        cout << _deviceUIConfig;
    }
}

void MeshClient::gotTextMessage(const meshtastic_MeshPacket &packet,
                                const string &message)
{
    (void)(packet);
    (void)(message);
}

void MeshClient::gotPosition(const meshtastic_MeshPacket &packet,
                             const meshtastic_Position &position)
{
    _positions[packet.from] = position;
}

void MeshClient::gotUser(const meshtastic_MeshPacket &packet,
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

void MeshClient::gotRouting(const meshtastic_MeshPacket &packet,
                            const meshtastic_Routing &routing)
{
    (void)(packet);
    (void)(routing);
}

void MeshClient::gotTelemetry(const meshtastic_MeshPacket &packet,
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

void MeshClient::gotDeviceMetrics(const meshtastic_MeshPacket &packet,
                                  const meshtastic_DeviceMetrics &metrics)
{
    _deviceMetrics[packet.from] = metrics;
}

void MeshClient::gotEnvironmentMetrics(const meshtastic_MeshPacket &packet,
                                       const meshtastic_EnvironmentMetrics &metrics)
{
    _environmentMetrics[packet.from] = metrics;
}

void MeshClient::gotAirQualityMetrics(const meshtastic_MeshPacket &packet,
                                      const meshtastic_AirQualityMetrics &metrics)
{
    _airQualityMetrics[packet.from] = metrics;
}

void MeshClient::gotPowerMetrics(const meshtastic_MeshPacket &packet,
                                 const meshtastic_PowerMetrics &metrics)
{
    _powerMetrics[packet.from] = metrics;
}

void MeshClient::gotLocalStats(const meshtastic_MeshPacket &packet,
                               const meshtastic_LocalStats &stats)
{
    _localStats[packet.from] = stats;
}

void MeshClient::gotHealthMetrics(const meshtastic_MeshPacket &packet,
                                  const meshtastic_HealthMetrics &metrics)
{
    _healthMetrics[packet.from] = metrics;
}

void MeshClient::gotHostMetrics(const meshtastic_MeshPacket &packet,
                                const meshtastic_HostMetrics &metrics)
{
    _hostMetrics[packet.from] = metrics;
}

void MeshClient::gotTraceRoute(const meshtastic_MeshPacket &packet,
                               const meshtastic_RouteDiscovery &routeDiscovery)
{
    (void)(packet);
    (void)(routeDiscovery);
}

void MeshClient::stop(void)
{
    _isRunning = false;
}

void MeshClient::thread_function(MeshClient *mtc)
{
    mtc->run();
}

void MeshClient::run(void)
{
    int ret = 0;
    uint32_t timeout_ms = 1000;
    time_t last, now;

    last = now = time(NULL);

    if (sendWantConfig() != true) {
        goto done;
    }

    while (_isRunning) {
        ret = mt_serial_process(&_mtc, timeout_ms);
        if (ret != 0) {
            _isRunning = false;
            continue;
        }

        if (_heartbeatSeconds > 0) {
            now = time(NULL);
            if ((now - last) >= (time_t) _heartbeatSeconds) {
                if (sendHeartbeat() != true) {
                    goto done;
                }
                last = now;
            }
        }
    }

done:

    sendDisconnect();
    mt_serial_detach(&_mtc);

    return;
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
