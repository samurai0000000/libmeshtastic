/*
 * MeshPrint.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <iostream>
#include <iomanip>
#include <MeshPrint.hxx>

#define INDENT os << indent(0)

struct indent {

    indent(int level) : level(level) {}

private:

    friend ostream &operator<<(std::ostream &stream, const indent &val);

    int level;

};

static int custom_index = -1;

static void adjust_indent(ostream &os, int level)
{
    int levels;

    levels = os.iword(custom_index);
    levels += level;
    os.iword(custom_index) = levels;
}

ostream &operator<<(ostream &os, const indent &val)
{
    int levels = 0;

    if (custom_index == -1) {
        custom_index = ios_base::xalloc();
    }

    levels = os.iword(custom_index);
    levels += val.level;
    os.iword(custom_index) = levels;

    for(int i = 0; i < levels; i++) {
        os << " ";
    }

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_MeshPacket &p)
{
    INDENT << "Packet {" << endl;
    adjust_indent(os, 2);
    INDENT << "from: !" << hex << setw(8) << p.from << dec << endl;
    INDENT << "to: !" << hex << setw(8) << p.to << dec << endl;
    INDENT << "channel: " << (int) p.channel << endl;
    if (p.which_payload_variant == meshtastic_MeshPacket_encrypted_tag) {
        INDENT << "payload: encrypted" << endl;
    } else {
        os << p.decoded;
    }
    INDENT << "id: " << p.id << endl;
    INDENT << "rx_time: " << p.rx_time << endl;
    INDENT << "rx_snr: " << p.rx_snr << endl;
    INDENT << "hop_limit: " << (unsigned int) p.hop_limit << endl;
    INDENT << "want_ack: " << (unsigned int) p.want_ack << endl;
    INDENT << "rx_rssi: " << p.rx_rssi << endl;
    switch (p.delayed) {
    case meshtastic_MeshPacket_Delayed_NO_DELAY:
        INDENT << "delayed: no" << endl;
        break;
    case meshtastic_MeshPacket_Delayed_DELAYED_BROADCAST:
        INDENT << "delayed: broadcast" << endl;
        break;
    case meshtastic_MeshPacket_Delayed_DELAYED_DIRECT:
        INDENT << "delayed: direct" << endl;
        break;
    default:
        break;
    }
    INDENT << "via_mqtt: " << (unsigned int) p.via_mqtt << endl;
    INDENT << "hop_start: " << (unsigned int) p.hop_start << endl;
    if (p.public_key.size > 0) {
        INDENT << "public_key: ";
        for (size_t i = 0; i < p.public_key.size; i++) {
            os << hex << setfill('0') << setw(2)
               << static_cast<unsigned int>(p.public_key.bytes[i]);
        }
        os << dec << endl;
    }
    INDENT << "pki_encrypted: " << (unsigned int) p.pki_encrypted << endl;
    INDENT << "next_hop: " << (unsigned int) p.next_hop << endl;
    INDENT << "relay_node: " << (unsigned int) p.relay_node << endl;
    INDENT << "tx_after: " << (unsigned int) p.tx_after << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Data &d)
{
    bool printable = true;
    pb_istream_t stream;
    int ret;

    for (size_t i = 0; i < d.payload.size; i++) {
        if (!isprint(d.payload.bytes[i])) {
            printable = false;
            break;
        }
    }

    INDENT << "data {" << endl;
    adjust_indent(os, 2);
    INDENT << "portnum: " << d.portnum << endl;
    if (!printable) {
        INDENT << "payload: ";
        for (size_t i = 0; i < d.payload.size; i++) {
            os << hex << setfill('0') << setw(2)
               << static_cast<unsigned int>(d.payload.bytes[i]);
        }
        os << dec << endl;
    } else {
        INDENT << "payload: ";
        for (size_t i = 0; i < d.payload.size; i++) {
            os << d.payload.bytes[i];
        }
        os << endl;
    }
    adjust_indent(os, 2);
    switch (d.portnum) {
    case meshtastic_PortNum_TELEMETRY_APP:
        meshtastic_Telemetry telemetry;
        memset(&telemetry, 0x0, sizeof(telemetry));
        stream = pb_istream_from_buffer(d.payload.bytes, d.payload.size);
        ret = pb_decode(&stream, meshtastic_Telemetry_fields, &telemetry);
        if (ret == 1) {
            os << telemetry;
        }
        break;
    default:
        break;
    }
    adjust_indent(os, -2);
    INDENT << "want_response: " << (int) d.want_response << endl;
    INDENT << "dest: !" << hex << setfill('0') << setw(8)
           << d.dest << dec << endl;
    INDENT << "source: !" << hex << setfill('0') << setw(8)
           << d.source << dec << endl;
    INDENT << "request_id: " << (unsigned int) d.request_id << endl;
    INDENT << "reply_id: " << (unsigned int) d.reply_id << endl;
    INDENT << "emoji: " << (unsigned int) d.emoji << endl;
    if (d.has_bitfield) {
        INDENT << "bitfield: 0x" << hex << setfill('0') << setw(2)
               << static_cast<unsigned int>(d.bitfield) << dec << endl;
    }

    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_PortNum &p)
{
    switch (p) {
    case meshtastic_PortNum_UNKNOWN_APP:
        os << "unknown_app";
        break;
    case meshtastic_PortNum_TEXT_MESSAGE_APP:
        os << "text_message_app";
        break;
    case meshtastic_PortNum_REMOTE_HARDWARE_APP:
        os << "remote_hardware_app";
        break;
    case meshtastic_PortNum_POSITION_APP:
        os << "position_app";
        break;
    case meshtastic_PortNum_NODEINFO_APP:
        os << "nodeinfo_app";
        break;
    case meshtastic_PortNum_ROUTING_APP:
        os << "routing_app";
        break;
    case meshtastic_PortNum_ADMIN_APP:
        os << "admin_app";
        break;
    case meshtastic_PortNum_TEXT_MESSAGE_COMPRESSED_APP:
        os << "text_message_compressed_app";
        break;
    case meshtastic_PortNum_WAYPOINT_APP:
        os << "waypoint_app";
        break;
    case meshtastic_PortNum_AUDIO_APP:
        os << "audio_app";
        break;
    case meshtastic_PortNum_DETECTION_SENSOR_APP:
        os << "sensor_app";
        break;
    case meshtastic_PortNum_ALERT_APP:
        os << "alert+app";
        break;
    case meshtastic_PortNum_REPLY_APP:
        os << "reply_app";
        break;
    case meshtastic_PortNum_IP_TUNNEL_APP:
        os << "ip_tunnel_app";
        break;
    case meshtastic_PortNum_PAXCOUNTER_APP:
        os << "paxcounter_app";
        break;
    case meshtastic_PortNum_SERIAL_APP:
        os << "serial_app";
        break;
    case meshtastic_PortNum_STORE_FORWARD_APP:
        os << "store_forward_app";
        break;
    case meshtastic_PortNum_RANGE_TEST_APP:
        os << "range_test_app";
        break;
    case meshtastic_PortNum_TELEMETRY_APP:
        os << "telemetry_app";
        break;
    case meshtastic_PortNum_ZPS_APP:
        os << "zps_app";
        break;
    case meshtastic_PortNum_SIMULATOR_APP:
        os << "simulator_app";
        break;
    case meshtastic_PortNum_TRACEROUTE_APP:
        os << "traceroute_app";
        break;
    case meshtastic_PortNum_NEIGHBORINFO_APP:
        os << "neighborinfo_app";
        break;
    case meshtastic_PortNum_ATAK_PLUGIN:
        os << "atak_plugin";
        break;
    case meshtastic_PortNum_MAP_REPORT_APP:
        os << "map_report_app";
        break;
    case meshtastic_PortNum_POWERSTRESS_APP:
        os << "powerstress_app";
        break;
    case meshtastic_PortNum_RETICULUM_TUNNEL_APP:
        os << "reticulum_tunnel_app";
        break;
    case meshtastic_PortNum_PRIVATE_APP:
        os << "private_app";
        break;
    case meshtastic_PortNum_ATAK_FORWARDER:
        os << "atak_forwarder";
        break;
    default:
        break;
    }

    os << " (" << (int) p << ")";

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Telemetry &t)
{
    INDENT << "telemetry {" << endl;
    adjust_indent(os, 2);
    INDENT << "time: " << t.time << endl;
    switch (t.which_variant) {
    case meshtastic_Telemetry_device_metrics_tag:
        os << t.variant.device_metrics;
        break;
    case meshtastic_Telemetry_environment_metrics_tag:
        os << t.variant.environment_metrics;
        break;
    case meshtastic_Telemetry_air_quality_metrics_tag:
        os << t.variant.air_quality_metrics;
        break;
    case meshtastic_Telemetry_power_metrics_tag:
        os << t.variant.power_metrics;
        break;
    case meshtastic_Telemetry_local_stats_tag:
        os << t.variant.local_stats;
        break;
    case meshtastic_Telemetry_health_metrics_tag:
        os << t.variant.health_metrics;
        break;
    case meshtastic_Telemetry_host_metrics_tag:
        os << t.variant.host_metrics;
        break;
    default:
        break;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_DeviceMetrics &m)
{
    INDENT << "device_metrics {" << endl;
    adjust_indent(os, 2);
    if (m.has_battery_level) {
        INDENT << "battery_level: " << m.battery_level << endl;
    }
    if (m.has_voltage) {
        INDENT << "voltage: " << m.voltage << endl;
    }
    if (m.has_channel_utilization) {
        INDENT << "channel_utilization: " << m.channel_utilization << endl;
    }
    if (m.has_air_util_tx) {
        INDENT << "air_util_tx: " << m.air_util_tx << endl;
    }
    if (m.has_uptime_seconds) {
        INDENT << "uptime_seconds: " << m.uptime_seconds << endl;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_EnvironmentMetrics &m)
{
    INDENT << "environment_metrics {" << endl;
    adjust_indent(os, 2);
    if (m.has_temperature) {
        INDENT << "temperature: " << m.temperature << endl;
    }
    if (m.has_relative_humidity) {
        INDENT << "relative_humidity: " << m.relative_humidity << endl;
    }
    if (m.has_barometric_pressure) {
        INDENT << "barometric_pressure: " << m.barometric_pressure << endl;
    }
    if (m.has_gas_resistance) {
        INDENT << "has_gas_resistance: " << m.gas_resistance << endl;
    }
    if (m.has_voltage) {
        INDENT << "voltage: " << m.voltage << endl;
    }
    if (m.has_current) {
        INDENT << "current: " << m.current << endl;
    }
    if (m.has_iaq) {
        INDENT << "iaq: " << m.iaq << endl;
    }
    if (m.has_distance) {
        INDENT << "distance: " << m.distance << endl;
    }
    if (m.has_lux) {
        INDENT << "lux: " << m.lux << endl;
    }
    if (m.has_white_lux) {
        INDENT << "white_lux: " << m.white_lux << endl;
    }
    if (m.has_ir_lux) {
        INDENT << "ir_lux: " << m.ir_lux << endl;
    }
    if (m.has_uv_lux) {
        INDENT << "uv_lux: " << m.uv_lux << endl;
    }
    if (m.has_wind_direction) {
        INDENT << "wind_direction: " << (int) m.wind_direction << endl;
    }
    if (m.has_wind_speed) {
        INDENT << "wind_speed: " << m.wind_speed << endl;
    }
    if (m.has_weight) {
        INDENT << "weight: " << m.weight << endl;
    }
    if (m.has_wind_gust) {
        INDENT << "wind_gust: " << m.wind_gust << endl;
    }
    if (m.has_radiation) {
        INDENT << "radiation: " << m.radiation << endl;
    }
    if (m.has_rainfall_1h) {
        INDENT << "rainfall_1h: " << m.rainfall_1h << endl;
    }
    if (m.has_rainfall_24h) {
        INDENT << "rainfall_24h: " << m.rainfall_24h << endl;
    }
    if (m.has_soil_moisture) {
        INDENT << "soil_moisture: " << m.soil_moisture << endl;
    }
    if (m.has_soil_temperature) {
        INDENT << "soil_temperature: " << m.soil_temperature << endl;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_AirQualityMetrics &m)
{
    INDENT << "air_quality_metrics {" << endl;
    adjust_indent(os, 2);
    if (m.has_pm10_standard) {
        INDENT << "pm10_standard: " << m.pm10_standard << endl;
    }
    if (m.has_pm25_standard) {
        INDENT << "pm25_standard: " << m.pm25_standard << endl;
    }
    if (m.has_pm100_standard) {
        INDENT << "pm100_standard: " << m.pm100_standard << endl;
    }
    if (m.has_pm10_environmental) {
        INDENT << "pm10_environmental: " << m.pm10_environmental << endl;
    }
    if (m.has_pm25_environmental) {
        INDENT << "pm25_environmental: " << m.pm25_environmental << endl;
    }
    if (m.has_pm100_environmental) {
        INDENT << "pm100_environmental: " << m.pm100_environmental << endl;
    }
    if (m.has_particles_03um) {
        INDENT << "particles_03m: " << m.particles_03um << endl;
    }
    if (m.has_particles_05um) {
        INDENT << "particles_05m: " << m.particles_05um << endl;
    }
    if (m.has_particles_10um) {
        INDENT << "particles_10m: " << m.particles_10um << endl;
    }
    if (m.has_particles_25um) {
        INDENT << "particles_25m: " << m.particles_25um << endl;
    }
    if (m.has_particles_50um) {
        INDENT << "particles_50m: " << m.particles_50um << endl;
    }
    if (m.has_particles_100um) {
        INDENT << "particles_100m: " << m.particles_100um << endl;
    }
    if (m.has_co2) {
        INDENT << "co2: " << m.co2 << endl;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_PowerMetrics &m)
{
    INDENT << "power_metrics {" << endl;
    adjust_indent(os, 2);
    if (m.has_ch1_voltage) {
        INDENT << "ch1_voltage: " << m.ch1_voltage << endl;
    }
    if (m.has_ch1_current) {
        INDENT << "ch1_currnet: " << m.ch1_current << endl;
    }
    if (m.has_ch2_voltage) {
        INDENT << "ch2_voltage: " << m.ch2_voltage << endl;
    }
    if (m.has_ch2_current) {
        INDENT << "ch2_current: " << m.ch2_current << endl;
    }
    if (m.has_ch3_voltage) {
        INDENT << "ch3_voltage: " << m.ch3_voltage << endl;
    }
    if (m.has_ch3_current) {
        INDENT << "ch3_current: " << m.ch3_current << endl;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_LocalStats &m)
{
    INDENT << "local_stats {" << endl;
    adjust_indent(os, 2);
    INDENT << "uptime_seconds: " << m.uptime_seconds << endl;
    INDENT << "channel_utilization: " << m.channel_utilization << endl;
    INDENT << "air_util_tx: " << m.air_util_tx << endl;
    INDENT << "num_packets_tx: " << m.num_packets_tx << endl;
    INDENT << "num_packets_rx: " << m.num_packets_rx << endl;
    INDENT << "num_packets_rx_bad: " << m.num_packets_rx_bad << endl;
    INDENT << "num_online_nodes: " << m.num_online_nodes << endl;
    INDENT << "num_total_nodes: " << m.num_total_nodes << endl;
    INDENT << "num_rx_dupe: " << m.num_rx_dupe << endl;
    INDENT << "num_tx_delay: " << m.num_tx_relay << endl;
    INDENT << "num_tx_relay_canceled: " << m.num_tx_relay_canceled << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_HealthMetrics &m)
{
    INDENT << "health_metrics {" << endl;
    adjust_indent(os, 2);
    if (m.has_heart_bpm) {
        INDENT << "heart_bpm: " << (int) m.heart_bpm << endl;
    }
    if (m.has_spO2) {
        INDENT << "spO2: " << (int) m.spO2 << endl;
    }
    if (m.has_temperature) {
        INDENT << "temperature: " << m.temperature << endl;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_HostMetrics &m)
{
    INDENT << "host_metrics {" << endl;
    adjust_indent(os, 2);
    INDENT << "uptime_seconds: " << m.uptime_seconds << endl;
    INDENT << "freemem_bytes: " << m.freemem_bytes << endl;
    INDENT << "diskfree1_bytes: " << m.diskfree1_bytes << endl;
    if (m.has_diskfree2_bytes) {
        INDENT << "diskfree2_bytes: " << m.diskfree2_bytes << endl;
    }
    if (m.has_diskfree3_bytes) {
        INDENT << "diskfree3_bytes: " << m.diskfree3_bytes << endl;
    }
    INDENT << "load1: " << (int) m.load1 << endl;
    INDENT << "load5: " << (int) m.load5 << endl;
    INDENT << "load15: " << (int) m.load15 << endl;
    if (m.has_user_string) {
        INDENT << "user_string: " << m.user_string << endl;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_MyNodeInfo &m)
{
    INDENT << "MyNodeInfo {" << endl;
    adjust_indent(os, 2);
    INDENT << "my_node_num: " << (unsigned int) m.my_node_num << endl;
    INDENT << "reboot_count: " << m.reboot_count << endl;
    INDENT << "min_app_version: " << m.min_app_version << endl;
    INDENT << "device_id: ";
    for (size_t i = 0; i < m.device_id.size; i++) {
        if (i > 0) {
            os << ":";
        }
        os << hex << setfill('0') << setw(2)
           << static_cast<unsigned int>(m.device_id.bytes[i]);
    }
    os << dec << endl;
    INDENT << "pio_env: " << m.pio_env << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_NodeInfo &i)
{
    INDENT << "NodeInfo {" << endl;
    adjust_indent(os, 2);
    INDENT << "num: " <<  (unsigned int) i.num << endl;
    if (i.has_user) {
        os << i.user;
    }
    if (i.has_position) {
        os << i.position;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_User &u)
{
    INDENT << "user { " << endl;
    adjust_indent(os, 2);
    INDENT << "id: " << u.id << endl;
    INDENT << "long_name: " << u.long_name << endl;
    INDENT << "short_name: " << u.short_name << endl;
    INDENT << "macaddr: ";
    for (size_t i = 0; i < sizeof(u.macaddr); i++) {
        if (i > 0) {
            os << ":";
        }
        os << hex << setfill('0') << setw(2)
           << static_cast<unsigned int>(u.macaddr[i]);
    }
    os << dec << endl;
    INDENT << "hw_model: " << (unsigned int) u.hw_model << endl;
    INDENT << "is_licensed: " << (int) u.is_licensed << endl;
    INDENT << "role: " << (unsigned int) u.role << endl;
    INDENT << "public_key: ";
    for (size_t i = 0; i < u.public_key.size; i++) {
        os << hex << setfill('0') << setw(2)
           << static_cast<unsigned int>(u.public_key.bytes[i]);
    }
    os << dec << endl;
    if (u.has_is_unmessagable) {
        INDENT << "is_unmessagebale: " << (int) u.is_unmessagable << endl;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Position &p)
{
    INDENT << "position {" << endl;
    adjust_indent(os, 2);
    if (p.has_latitude_i) {
        INDENT << "latitude_i: " << p.latitude_i << endl;
    }
    if (p.has_longitude_i) {
        INDENT << "longitude_i: " << p.longitude_i << endl;
    }
    if (p.has_altitude) {
        INDENT << "altitude: " << p.altitude << endl;
    }
    INDENT << "time: " << p.time << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Routing &r)
{
    INDENT << "routing {" << endl;
    adjust_indent(os, 2);
    switch (r.which_variant) {
    case meshtastic_Routing_route_request_tag:
        INDENT << "route_request {" << endl;
        adjust_indent(os, 2);
        os << r.route_request;
        adjust_indent(os, -2);
        os << "}" << endl;
        break;
    case meshtastic_Routing_route_reply_tag:
        INDENT << "route_reply {" << endl;
        adjust_indent(os, 2);
        os << r.route_reply;
        adjust_indent(os, -2);
        os << "}" << endl;
        break;
    case meshtastic_Routing_error_reason_tag:
        INDENT << "route_error: ";
        switch (r.error_reason) {
        case meshtastic_Routing_Error_NONE:
            os << "none";
            break;
        case meshtastic_Routing_Error_NO_ROUTE:
            os << "no_route";
            break;
        case meshtastic_Routing_Error_GOT_NAK:
            os << "got_nak";
            break;
        case meshtastic_Routing_Error_TIMEOUT:
            os << "timeout";
            break;
        case meshtastic_Routing_Error_NO_INTERFACE:
            os << "no_interface";
            break;
        case meshtastic_Routing_Error_MAX_RETRANSMIT:
            os << "max_retransmit";
            break;
        case meshtastic_Routing_Error_NO_CHANNEL:
            os << "no_channel";
            break;
        case meshtastic_Routing_Error_TOO_LARGE:
            os << "too_large";
            break;
        case meshtastic_Routing_Error_NO_RESPONSE:
            os << "no_response";
            break;
        case meshtastic_Routing_Error_DUTY_CYCLE_LIMIT:
            os << "duty_cyle_limit";
            break;
        case meshtastic_Routing_Error_BAD_REQUEST:
            os << "bad_request";
            break;
        case meshtastic_Routing_Error_NOT_AUTHORIZED:
            os << "not_authorized";
            break;
        case meshtastic_Routing_Error_PKI_UNKNOWN_PUBKEY:
            os << "pki_unknown_pubkey";
            break;
        case meshtastic_Routing_Error_ADMIN_BAD_SESSION_KEY:
            os << "admin_bad_session_key";
            break;
        default:
            os << "undefined";
            break;
        }
        os << endl;
        break;
    default:
        break;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_RouteDiscovery &r)
{
    unsigned int i;

    INDENT << "route_discovery {" << endl;
    adjust_indent(os, 2);
    INDENT << "route towards: ";
    for (i = 0; i < r.route_count; i++) {
        if (i > 0) {
            os << " -> ";
        }
        os << "!" << hex << setfill('0') << setw(8) << r.route[i];
        if (i < r.snr_towards_count) {
            if (r.snr_towards[i] != INT8_MIN) {
                os << "(" << dec << (((float) r.snr_towards[i]) / 4.0)
                   << "dB)";
            }
        }
    }
    os << endl;
    INDENT << "route back: ";
    for (i = 0; i < r.route_back_count; i++) {
        if (i > 0) {
            os << " -> ";
        }
        os << "!" << hex << setfill('0') << setw(8) << r.route_back[i];
        if (i < r.snr_back_count) {
            if (r.snr_back[i] != INT8_MIN) {
                os << "(" << dec << (((float) r.snr_back[i]) / 4.0)
                   << "dB)";
            }
        }
    }
    os << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config &c)
{
    INDENT << "Config {" << endl;
    adjust_indent(os, 2);
    switch (c.which_payload_variant) {
    case meshtastic_Config_device_tag:
        os << c.payload_variant.device;
        break;
    case meshtastic_Config_position_tag:
        os << c.payload_variant.position;
        break;
    case meshtastic_Config_power_tag:
        os << c.payload_variant.power;
        break;
    case meshtastic_Config_network_tag:
        os << c.payload_variant.network;
        break;
    case meshtastic_Config_display_tag:
        os << c.payload_variant.display;
        break;
    case meshtastic_Config_lora_tag:
        os << c.payload_variant.lora;
        break;
    case meshtastic_Config_bluetooth_tag:
        os << c.payload_variant.bluetooth;
        break;
    case meshtastic_Config_security_tag:
        os << c.payload_variant.security;
        break;
    case meshtastic_Config_sessionkey_tag:
        os << c.payload_variant.sessionkey;
        break;
    case meshtastic_Config_device_ui_tag:
        os << c.payload_variant.device_ui;
        break;
    default:
        break;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config_DeviceConfig &c)
{
    INDENT << "device {" << endl;
    adjust_indent(os, 2);
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config_PositionConfig &c)
{
    INDENT << "position {" << endl;
    adjust_indent(os, 2);
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config_PowerConfig &c)
{
    INDENT << "power {" << endl;
    adjust_indent(os, 2);
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config_NetworkConfig &c)
{
    INDENT << "network {" << endl;
    adjust_indent(os, 2);
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config_DisplayConfig &c)
{
    INDENT << "display {" << endl;
    adjust_indent(os, 2);
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config_LoRaConfig &c)
{
    INDENT << "lora {" << endl;
    adjust_indent(os, 2);
    INDENT << "use_preset: " << (int) c.use_preset << endl;
    INDENT << "modem_preset: ";
    switch (c.modem_preset) {
    case meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST:
        os << "LongFast";
        break;
    case meshtastic_Config_LoRaConfig_ModemPreset_LONG_SLOW :
        os << "LongSlow";
        break;
    case meshtastic_Config_LoRaConfig_ModemPreset_VERY_LONG_SLOW:
        os << "VeryLongSlow";
        break;
    case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_SLOW:
        os << "MediumSlow";
        break;
    case meshtastic_Config_LoRaConfig_ModemPreset_MEDIUM_FAST:
        os << "MediumFast";
        break;
    case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_SLOW:
        os << "ShortSlow";
        break;
    case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_FAST:
        os << "ShortFast";
        break;
    case meshtastic_Config_LoRaConfig_ModemPreset_LONG_MODERATE:
        os << "LongModerate";
        break;
    case meshtastic_Config_LoRaConfig_ModemPreset_SHORT_TURBO:
        os << "ShortTurbo";
        break;
    default:
        break;
    }
    os << endl;
    INDENT << "bandwidth: " << (unsigned int) c.bandwidth << endl;
    INDENT << "spread_factor: " << (unsigned int) c.spread_factor << endl;
    INDENT << "coding_rate: " << (int) c.coding_rate << "/8" << endl;
    INDENT << "frequency_offset: " << c.frequency_offset << endl;
    INDENT << "region: ";
    switch (c.region) {
    case meshtastic_Config_LoRaConfig_RegionCode_UNSET:
        os << "unset";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_US:
        os << "US";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_EU_433:
        os << "EU_433";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_EU_868:
        os << "EU_868";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_CN:
        os << "CN";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_JP:
        os << "JP";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_ANZ:
        os << "ANZ";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_KR:
        os << "KR";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_TW:
        os << "TW";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_RU:
        os << "RU";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_IN:
        os << "IN";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_NZ_865:
        os << "NZ_865";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_TH:
        os << "TH";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_LORA_24:
        os << "LORA_24";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_UA_433:
        os << "UA_433";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_UA_868:
        os << "UA_868";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_MY_433:
        os << "MY_433";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_MY_919:
        os << "MY_919";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_SG_923:
        os << "SG_923";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_PH_433:
        os << "PH_433";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_PH_868:
        os << "PH_868";
        break;
    case meshtastic_Config_LoRaConfig_RegionCode_PH_915:
        os << "PH_915";
        break;
    default:
        break;
    }
    os << endl;
    INDENT << "hop_limit: " << c.hop_limit << endl;
    INDENT << "tx_enabled: " << (int) c.tx_enabled << endl;
    INDENT << "tx_power: " << (int) c.tx_power << endl;
    INDENT << "channel_num: " << (int) c.channel_num << endl;
    INDENT << "override_duty_cycle: " << (int) c.override_duty_cycle << endl;
    INDENT << "sx126x_rx_boosted_gain: " << (int) c.sx126x_rx_boosted_gain << endl;
    INDENT << "override_frequency: " << c.override_frequency << endl;
    INDENT << "pa_fan_disabled: " << (int) c.pa_fan_disabled << endl;
    for (unsigned int i = 0; i < c.ignore_incoming_count; i++) {
        INDENT << "ignore_incoming[" << i << "]: !"
               << hex << setfill('0') << setw(8)
               << c.ignore_incoming[i] << dec << endl;
    }
    INDENT << "ignore_mqtt: " << (int) c.ignore_mqtt << endl;
    INDENT << "config_ok_to_mqtt: " << (int) c.config_ok_to_mqtt << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config_BluetoothConfig &c)
{
    INDENT << "bluetooth {" << endl;
    adjust_indent(os, 2);
    INDENT << "enabled: " << (int) c.enabled << endl;
    INDENT << "mode: ";
    switch (c.mode) {
    case meshtastic_Config_BluetoothConfig_PairingMode_RANDOM_PIN:
        os << "random_pin";
        break;
    case meshtastic_Config_BluetoothConfig_PairingMode_FIXED_PIN:
        os << "fixed_pin";
        break;
    case meshtastic_Config_BluetoothConfig_PairingMode_NO_PIN:
        os << "no_pin";
        break;
    default:
        break;
    }
    os << endl;
    INDENT << "fixed_pin: " << setfill('0') << setw(6)
           << (int) c.fixed_pin << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config_SecurityConfig &c)
{
    INDENT << "security {" << endl;
    adjust_indent(os, 2);
    INDENT << "public_key: ";
    for (size_t i = 0; i < c.public_key.size; i++) {
        os << hex << setfill('0') << setw(2)
           << static_cast<unsigned int>(c.public_key.bytes[i]);
    }
    os << dec << endl;
    INDENT << "private_key: ";
    for (size_t i = 0; i < c.private_key.size; i++) {
        os << hex << setfill('0') << setw(2)
           << static_cast<unsigned int>(c.private_key.bytes[i]);
    }
    os << dec << endl;
    for (unsigned int j = 0; j < c.admin_key_count; j++) {
        INDENT << "admin_key[" << j << "]: ";
        for (size_t i = 0; i < c.admin_key[j].size; i++) {
            os << hex << setfill('0') << setw(2)
               << static_cast<unsigned int>(c.admin_key[j].bytes[i]);
        }
        os << dec << endl;
    }
    INDENT << "is_managed: " << (int) c.is_managed << endl;
    INDENT << "serial_enabled: " << (int) c.serial_enabled << endl;
    INDENT << "debug_log_api_enabled: " << (int) c.debug_log_api_enabled << endl;
    INDENT << "admin_channel_enabled: " << (int) c.admin_channel_enabled << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Config_SessionkeyConfig &c)
{
    INDENT << "sessionkey {" << endl;
    adjust_indent(os, 2);
    INDENT << "dummy_field: 0x" << hex << setfill('0') << setw(2)
           << static_cast<unsigned int>(c.dummy_field) << dec << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig &c)
{
    INDENT << "ModuleConfig {" << endl;
    adjust_indent(os, 2);
    switch (c.which_payload_variant) {
    case meshtastic_ModuleConfig_mqtt_tag:
        os << c.payload_variant.mqtt;
        break;
    case meshtastic_ModuleConfig_serial_tag:
        os << c.payload_variant.serial;
        break;
    case meshtastic_ModuleConfig_external_notification_tag:
        os << c.payload_variant.external_notification;
        break;
    case meshtastic_ModuleConfig_store_forward_tag:
        os << c.payload_variant.store_forward;
        break;
    case meshtastic_ModuleConfig_range_test_tag:
        os << c.payload_variant.range_test;
        break;
    case meshtastic_ModuleConfig_telemetry_tag:
        os << c.payload_variant.telemetry;
        break;
    case meshtastic_ModuleConfig_canned_message_tag:
        os << c.payload_variant.canned_message;
        break;
    case meshtastic_ModuleConfig_audio_tag:
        os << c.payload_variant.audio;
        break;
    case meshtastic_ModuleConfig_remote_hardware_tag:
        os << c.payload_variant.remote_hardware;
        break;
    case meshtastic_ModuleConfig_neighbor_info_tag:
        os << c.payload_variant.neighbor_info;
        break;
    case meshtastic_ModuleConfig_ambient_lighting_tag:
        os << c.payload_variant.ambient_lighting;
        break;
    case meshtastic_ModuleConfig_detection_sensor_tag:
        os << c.payload_variant.detection_sensor;
        break;
    case meshtastic_ModuleConfig_paxcounter_tag:
        os << c.payload_variant.paxcounter;
        break;
    default:
        break;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_MQTTConfig &c)
{
    INDENT << "module_config_mqtt {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_SerialConfig &c)
{
    INDENT << "module_config_serial {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_ExternalNotificationConfig &c)
{
    INDENT << "module_config_external_notification {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_StoreForwardConfig &c)
{
    INDENT << "module_config_store_forward {" << endl;
    INDENT << "..." << endl;
    adjust_indent(os, 2);
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_RangeTestConfig &c)
{
    INDENT << "module_config_range_test {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_TelemetryConfig &c)
{
    INDENT << "module_config_telemetry {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_CannedMessageConfig &c)
{
    INDENT << "module_config_canned_message {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_AudioConfig &c)
{
    INDENT << "module_config_audio {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_RemoteHardwareConfig &c)
{
    INDENT << "module_config_remote_hardware {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_NeighborInfoConfig &c)
{
    INDENT << "module_config_neighbor_info {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_AmbientLightingConfig &c)
{
    INDENT << "module_config_ambient_lighting {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_DetectionSensorConfig &c)
{
    INDENT << "module_config_detection_sensor {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleConfig_PaxcounterConfig &c)
{
    INDENT << "module_config_paxcounter {" << endl;
    adjust_indent(os, 2);
    INDENT << "..." << endl;
    (void)(c);
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_Channel &c)
{
    INDENT << "Channel {" << endl;
    adjust_indent(os, 2);
    INDENT << "index: " << (int) c.index << endl;
    if (c.has_settings) {
        os << c.settings;
    }
    switch (c.role) {
    case meshtastic_Channel_Role_DISABLED:
        INDENT << "role: disabled" << endl;
        break;
    case meshtastic_Channel_Role_PRIMARY:
        INDENT << "role: primary" << endl;
        break;
    case meshtastic_Channel_Role_SECONDARY:
        INDENT << "role: secondary" << endl;
        break;
    default:
        break;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ChannelSettings &s)
{
    INDENT << "settings {" << endl;
    adjust_indent(os, 2);
    INDENT << "channel_num:" << s.channel_num << endl;
    if (s.psk.size > 0) {
        INDENT << "public_key: ";
        for (size_t i = 0; i < s.psk.size; i++) {
            os << hex << setfill('0') << setw(2)
               << static_cast<unsigned int>(s.psk.bytes[i]);
        }
        os << dec << endl;
    }
    INDENT << "name: " << s.name << endl;
    INDENT << "id: " << s.id << endl;
    INDENT << "uplink_enabled: " << (int) s.uplink_enabled << endl;
    INDENT << "downlink_enabled: " << (int) s.downlink_enabled << endl;
    if (s.has_module_settings) {
        os << s.module_settings;
    }
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_ModuleSettings &s)
{
    INDENT << "module-settings {" << endl;
    adjust_indent(os, 2);
    INDENT << "position_precision: " << s.position_precision << endl;
    INDENT << "is_client_muted: " << (int) s.is_client_muted << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_QueueStatus &q)
{
    INDENT << "QueueStatus {" << endl;
    adjust_indent(os, 2);
    INDENT << "res: " << (int) q.res << " " << endl;
    INDENT << "free: " << (int) q.free << endl;
    INDENT << "maxlen: " << (int) q.maxlen << endl;
    INDENT << "packet_id: " << q.mesh_packet_id << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_DeviceMetadata &m)
{
    INDENT << "DeviceMetadata {" << endl;
    adjust_indent(os, 2);
    INDENT << "firmware_version: " << m.firmware_version << endl;
    INDENT << "device_state_version: " << m.device_state_version << endl;
    INDENT << "canShutdown: " << (int) m.canShutdown << endl;
    INDENT << "hasWifi: " << (int) m.hasWifi << endl;
    INDENT << "hasBluetooth: " << (int) m.hasBluetooth << endl;
    INDENT << "hasEthernet: " << (int) m.hasEthernet<< endl;
    INDENT << "role: " << (unsigned int) m.role << endl;
    INDENT << "position_flags: 0x" << hex << setfill('0') << setw(8)
           << m.position_flags << dec << endl;

    INDENT << "hasRemoteHardware: " << (int) m.hasRemoteHardware << endl;
    INDENT << "hasPKC: " << (int) m.hasPKC << endl;
    INDENT << "excluded_modules: 0x" << hex << setfill('0') << setw(8)
           << m.excluded_modules << dec << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;

}

ostream &operator<<(ostream &os, const meshtastic_FileInfo &i)
{
    INDENT << "FileInfo {" << endl;
    adjust_indent(os, 2);
    INDENT << "file_name: " << i.file_name << endl;
    INDENT << "size_bytes: " << i.size_bytes << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_DeviceUIConfig &u)
{
    INDENT << "DeviceUIConfig {" << endl;
    adjust_indent(os, 2);
    INDENT << "version: " << u.version << endl;
    INDENT << "screen_brightness: " << (int) u.screen_brightness << endl;
    INDENT << "screen_timeout: " << (int) u.screen_timeout << endl;
    INDENT << "..." << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;
}

ostream &operator<<(ostream &os, const meshtastic_MqttClientProxyMessage &m)
{
    INDENT << "MqttClientProxyMessage {" << endl;
    adjust_indent(os, 2);
    INDENT << "topic: " << m.topic << endl;
    switch (m.which_payload_variant) {
    case meshtastic_MqttClientProxyMessage_data_tag:
        INDENT << "data: ";
        for (unsigned int i = 0; i < m.payload_variant.data.size; i++) {
            os << hex << setfill('0') << setw(2)
               << static_cast<unsigned int>(m.payload_variant.data.bytes[i]);
        }
        INDENT << dec << endl;
        break;
    case meshtastic_MqttClientProxyMessage_text_tag:
        INDENT << "text: " << m.payload_variant.text << endl;
        break;
    default:
        break;
    }
    INDENT << "retained: " << (int) m.retained << endl;
    adjust_indent(os, -2);
    INDENT << "}" << endl;

    return os;

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
