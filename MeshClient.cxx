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
    : SimpleClient()
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

void MeshClient::clear(void)
{
    SimpleClient::clear();

    _deviceConfig = meshtastic_Config_DeviceConfig();
    _positionConfig = meshtastic_Config_PositionConfig();
    _powerConfig = meshtastic_Config_PowerConfig();
    _networkConfig = meshtastic_Config_NetworkConfig();
    _displayConfig = meshtastic_Config_DisplayConfig();
    _bluetoothConfig = meshtastic_Config_BluetoothConfig();
    _securityConfig = meshtastic_Config_SecurityConfig();
    _sessionkeyConfig = meshtastic_Config_SessionkeyConfig();
    _queueStatus = meshtastic_QueueStatus();
    _deviceMetadata = meshtastic_DeviceMetadata();
    _deviceUIConfig = meshtastic_DeviceUIConfig();
    _fileInfos.clear();
    _modMQTT = meshtastic_ModuleConfig_MQTTConfig();
    _modSerial = meshtastic_ModuleConfig_SerialConfig();
    _modExternalNotification = meshtastic_ModuleConfig_ExternalNotificationConfig();
    _modStoreForward = meshtastic_ModuleConfig_StoreForwardConfig();
    _modRangeTest = meshtastic_ModuleConfig_RangeTestConfig();
    _modTelemetry = meshtastic_ModuleConfig_TelemetryConfig();
    _modCannedMessage = meshtastic_ModuleConfig_CannedMessageConfig();
    _modAudio = meshtastic_ModuleConfig_AudioConfig();
    _modRemoteHardware = meshtastic_ModuleConfig_RemoteHardwareConfig();
    _modNeighborInfo = meshtastic_ModuleConfig_NeighborInfoConfig();
    _modAmbientLighting = meshtastic_ModuleConfig_AmbientLightingConfig();
    _modDetectionSensor = meshtastic_ModuleConfig_DetectionSensorConfig();
    _modPaxcounter = meshtastic_ModuleConfig_PaxcounterConfig();
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
    result = SimpleClient::sendDisconnect();
    _mutex.unlock();

    return result;
}

bool MeshClient::sendWantConfig(void)
{
    bool result = false;

    _mutex.lock();
    result = SimpleClient::sendWantConfig();
    _mutex.unlock();

    return result;
}

bool MeshClient::sendHeartbeat(void)
{
    bool result = false;

    _mutex.lock();
    result = SimpleClient::sendHeartbeat();
    _mutex.unlock();

    return result;
}

bool MeshClient::textMessage(uint32_t dest, uint8_t channel,
                             const string &message,
                             unsigned int hop_start, bool want_ack)
{
    bool result = false;

    _mutex.lock();
    result = SimpleClient::textMessage(dest, channel, message,
                                       hop_start, want_ack);
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


unsigned int MeshClient::hopsAway(uint32_t node_num) const
{
    uint8_t hops = 0xffU;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    it = _nodeInfos.find(node_num);
    if ((it != _nodeInfos.end()) && it->second.has_hops_away) {
        hops = it->second.hops_away;
    }

    return (unsigned int) hops;
}

unsigned int MeshClient::hopsAway(const meshtastic_MeshPacket &packet) const
{
    return hopsAway(packet.from);
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
    case meshtastic_FromRadio_moduleConfig_tag:
        client->gotModuleConfig(fromRadio->moduleConfig);
        break;
    case meshtastic_FromRadio_channel_tag:
        client->gotChannel(fromRadio->channel);
        break;
    case meshtastic_FromRadio_config_complete_id_tag:
        client->gotConfigCompleteId(fromRadio->config_complete_id);
        break;
    case meshtastic_FromRadio_rebooted_tag:
        client->gotRebooted(fromRadio->rebooted);
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
    case meshtastic_FromRadio_mqttClientProxyMessage_tag:
        client->gotMqttClientProxyMessage(fromRadio->mqttClientProxyMessage);
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
        case meshtastic_PortNum_ADMIN_APP:
        {
            meshtastic_AdminMessage admmsg;
            stream = pb_istream_from_buffer(packet.decoded.payload.bytes,
                                            packet.decoded.payload.size);
            ret = pb_decode(&stream, meshtastic_AdminMessage_fields, &admmsg);
            if (ret == 1) {
                gotAdminMessage(packet, admmsg);
            } else {
                cerr << "pb_decode admmsg failed!" << endl;
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
    SimpleClient::gotChannel(channel);
    if (_verbose) {
        cout << channel;
    }
}

void MeshClient::gotConfigCompleteId(uint32_t id)
{
    SimpleClient::gotConfigCompleteId(id);
    if (_verbose) {
        cout << "ConfigCompleteId: 0x"
             << hex << setfill('0') << setw(8) << id << dec << endl;
    }
}

void MeshClient::gotRebooted(bool rebooted)
{
    SimpleClient::gotRebooted(rebooted);
    if (_verbose) {
        cout << "Rebooted: %d\n" << (int) rebooted << endl;
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

void MeshClient::gotMqttClientProxyMessage(const meshtastic_MqttClientProxyMessage &m)
{
    if (_verbose) {
        cout << m;
    }
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
    time_t last, last_want_config, now;

    last = now = time(NULL);
    last_want_config = 0;

    sendDisconnect();

    while (_isRunning) {
        now = time(NULL);
        if (!isConnected() && ((now - last_want_config) >= 5)) {
            ret = sendWantConfig();
            if (ret != true) {
                _isRunning = false;
                continue;
            }

            last_want_config = time(NULL);
        }

        ret = mt_serial_process(&_mtc, timeout_ms);
        if (ret != 0) {
            _isRunning = false;
            continue;
        }

        if (!isConnected()) {
            continue;
        }

        now = time(NULL);
        if (_heartbeatSeconds > 0) {
            if ((now - last) >= (time_t) _heartbeatSeconds) {
                if (sendHeartbeat() != true) {
                    _isRunning = false;
                    break;
                }
                last = now;
            }
        }
    }

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
