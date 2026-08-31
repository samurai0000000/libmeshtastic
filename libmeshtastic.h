/*
 * libmeshtastic.h
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef LIBMESHTASTIC_H
#define LIBMESHTASTIC_H

#if !defined(EXTERN_C_BEGIN)
#if defined(__cplusplus)
#define EXTERN_C_BEGIN extern "C" {
#else
#define EXTERN_C_BEGIN
#endif
#endif

#if !defined(EXTERN_C_END)
#if defined(__cplusplus)
#define EXTERN_C_END }
#else
#define EXTERN_C_END
#endif
#endif

EXTERN_C_BEGIN

#include <time.h>
#include <meshtastic/mesh.pb.h>
#include <meshtastic/admin.pb.h>
#include <meshtastic/config.pb.h>
#include <meshtastic/module_config.pb.h>
#include <meshtastic/channel.pb.h>
#include <meshtastic/device_ui.pb.h>
#include <meshtastic/telemetry.pb.h>
#include <meshtastic/remote_hardware.pb.h>
#include <meshtastic/portnums.pb.h>
#include <meshtastic/connection_status.pb.h>
#include <meshtastic/mqtt.pb.h>
#include <pb_encode.h>
#include <pb_decode.h>

struct mt_pb_header {
    uint8_t start1;
#define MT_PB_START1 0x94U
    uint8_t start2;
#define MT_PB_START2 0xc3U
    uint8_t h_len;
    uint8_t l_len;
};

struct mt_client
{
    uint32_t type;
#define MT_CLIENT_SERIAL 0
    int fd;
    const char *device;
    uint8_t inbuf[sizeof(struct mt_pb_header) + 512];
    size_t inbuf_len;
    void (*handler)(struct mt_client *mtc, const void *packet, size_t size,
                    const meshtastic_FromRadio *from_radio);
    void (*logger)(struct mt_client *mtc, const char *msg, size_t len);
    void *ctx;
    uint32_t bytes_rx;
    uint32_t bytes_tx;
    uint32_t packets_rx;
    uint32_t packets_tx;
    uint32_t last_packet_ts;
    uint32_t last_byte_ts;
};

#if defined(LIB_PICO_PLATFORM)
extern void mt_serial_init(void);
extern void mt_serial_write(unsigned int chan, const void *buf, size_t len);
#endif

extern int mt_serial_attach(struct mt_client *mtc, const char *device);
extern int mt_serial_detach(struct mt_client *mtc);
extern int mt_serial_process(struct mt_client *mtc, uint32_t timeout_ms);
extern int mt_serial_send(struct mt_client *mtc, const uint8_t *packet,
                          size_t size);

/* Protocol Base / Core Operations */
extern int mt_recv_packet(struct mt_client *mtc, uint8_t *packet, size_t size);
extern int mt_send_null(struct mt_client *mtc);
extern int mt_send_disconnect(struct mt_client *mtc);
extern int mt_send_heartbeat(struct mt_client *mtc);
extern int mt_send_want_config(struct mt_client *mtc);
extern int mt_text_message(struct mt_client *mtc,
                           uint32_t dest, uint8_t channel,
                           const char *message,
                           unsigned int hop_start, bool want_ack);
extern int mt_send_admin_message(struct mt_client *mtc, uint32_t dest,
                                 const meshtastic_AdminMessage *admin_message,
                                 bool want_response);

/* Group 1: Device Configuration (meshtastic_Config) */
extern int mt_admin_message_get_config_request(struct mt_client *mtc,
                                               uint32_t dest,
                                               meshtastic_AdminMessage_ConfigType config_type);
extern int mt_admin_message_set_config(struct mt_client *mtc,
                                       uint32_t dest,
                                       const meshtastic_Config *config);
extern int mt_admin_message_set_device_config(struct mt_client *mtc,
                                              uint32_t dest,
                                              const meshtastic_Config_DeviceConfig *config);
extern int mt_admin_message_set_position_config(struct mt_client *mtc,
                                                uint32_t dest,
                                                const meshtastic_Config_PositionConfig *config);
extern int mt_admin_message_set_power_config(struct mt_client *mtc,
                                             uint32_t dest,
                                             const meshtastic_Config_PowerConfig *config);
extern int mt_admin_message_set_network_config(struct mt_client *mtc,
                                               uint32_t dest,
                                               const meshtastic_Config_NetworkConfig *config);
extern int mt_admin_message_set_display_config(struct mt_client *mtc,
                                               uint32_t dest,
                                               const meshtastic_Config_DisplayConfig *config);
extern int mt_admin_message_set_lora_config(struct mt_client *mtc,
                                            uint32_t dest,
                                            const meshtastic_Config_LoRaConfig *config);
extern int mt_admin_message_set_bluetooth_config(struct mt_client *mtc,
                                                 uint32_t dest,
                                                 const meshtastic_Config_BluetoothConfig *config);
extern int mt_admin_message_set_security_config(struct mt_client *mtc,
                                                uint32_t dest,
                                                const meshtastic_Config_SecurityConfig *config);
extern int mt_admin_message_set_sessionkey_config(struct mt_client *mtc,
                                                  uint32_t dest,
                                                  const meshtastic_Config_SessionkeyConfig *config);

/* Group 2: Module Configuration (meshtastic_ModuleConfig) */
extern int mt_admin_message_get_module_config_request(struct mt_client *mtc,
                                                      uint32_t dest,
                                                      meshtastic_AdminMessage_ModuleConfigType module_config_type);
extern int mt_admin_message_set_module_config(struct mt_client *mtc,
                                              uint32_t dest,
                                              const meshtastic_ModuleConfig *module_config);
extern int mt_admin_message_set_mqtt_config(struct mt_client *mtc,
                                            uint32_t dest,
                                            const meshtastic_ModuleConfig_MQTTConfig *config);
extern int mt_admin_message_set_serial_module_config(struct mt_client *mtc,
                                                     uint32_t dest,
                                                     const meshtastic_ModuleConfig_SerialConfig *config);
extern int mt_admin_message_set_extnotif_config(struct mt_client *mtc,
                                                uint32_t dest,
                                                const meshtastic_ModuleConfig_ExternalNotificationConfig *config);
extern int mt_admin_message_set_storeforward_config(struct mt_client *mtc,
                                                    uint32_t dest,
                                                    const meshtastic_ModuleConfig_StoreForwardConfig *config);
extern int mt_admin_message_set_rangetest_config(struct mt_client *mtc,
                                                 uint32_t dest,
                                                 const meshtastic_ModuleConfig_RangeTestConfig *config);
extern int mt_admin_message_set_telemetry_config(struct mt_client *mtc,
                                                 uint32_t dest,
                                                 const meshtastic_ModuleConfig_TelemetryConfig *config);
extern int mt_admin_message_set_cannedmsg_config(struct mt_client *mtc,
                                                 uint32_t dest,
                                                 const meshtastic_ModuleConfig_CannedMessageConfig *config);
extern int mt_admin_message_set_audio_config(struct mt_client *mtc,
                                             uint32_t dest,
                                             const meshtastic_ModuleConfig_AudioConfig *config);
extern int mt_admin_message_set_remotehardware_config(struct mt_client *mtc,
                                                      uint32_t dest,
                                                      const meshtastic_ModuleConfig_RemoteHardwareConfig *config);
extern int mt_admin_message_set_neighborinfo_config(struct mt_client *mtc,
                                                    uint32_t dest,
                                                    const meshtastic_ModuleConfig_NeighborInfoConfig *config);
extern int mt_admin_message_set_ambientlighting_config(struct mt_client *mtc,
                                                       uint32_t dest,
                                                       const meshtastic_ModuleConfig_AmbientLightingConfig *config);
extern int mt_admin_message_set_detectionsensor_config(struct mt_client *mtc,
                                                       uint32_t dest,
                                                       const meshtastic_ModuleConfig_DetectionSensorConfig *config);
extern int mt_admin_message_set_paxcounter_config(struct mt_client *mtc,
                                                  uint32_t dest,
                                                  const meshtastic_ModuleConfig_PaxcounterConfig *config);

/* Group 3: Channel Configuration (meshtastic_Channel) */
extern int mt_admin_message_get_channel_request(struct mt_client *mtc,
                                                uint32_t dest,
                                                uint32_t channel_index);
extern int mt_admin_message_set_channel(struct mt_client *mtc,
                                        uint32_t dest,
                                        const meshtastic_Channel *channel);

/* Group 4: Owner Identity & Ham Radio Parameters (meshtastic_User, meshtastic_HamParameters) */
extern int mt_admin_message_get_owner_request(struct mt_client *mtc,
                                              uint32_t dest);
extern int mt_admin_message_set_owner(struct mt_client *mtc,
                                      uint32_t dest,
                                      const meshtastic_User *user);
extern int mt_admin_message_set_ham_mode(struct mt_client *mtc,
                                         uint32_t dest,
                                         const meshtastic_HamParameters *ham_params);

/* Group 5: Node Database & Contact Operations */
extern int mt_admin_message_set_favorite_node(struct mt_client *mtc,
                                              uint32_t dest,
                                              uint32_t nodenum);
extern int mt_admin_message_remove_favorite_node(struct mt_client *mtc,
                                                 uint32_t dest,
                                                 uint32_t nodenum);
extern int mt_admin_message_set_ignored_node(struct mt_client *mtc,
                                             uint32_t dest,
                                             uint32_t nodenum);
extern int mt_admin_message_remove_ignored_node(struct mt_client *mtc,
                                                uint32_t dest,
                                                uint32_t nodenum);
extern int mt_admin_message_remove_by_nodenum(struct mt_client *mtc,
                                              uint32_t dest,
                                              uint32_t nodenum);
extern int mt_admin_message_reset_nodedb(struct mt_client *mtc,
                                         uint32_t dest);
extern int mt_admin_message_add_contact(struct mt_client *mtc,
                                        uint32_t dest,
                                        const meshtastic_SharedContact *contact);

/* Group 6: Fixed Position & Location Operations (meshtastic_Position) */
extern int mt_admin_message_set_fixed_position(struct mt_client *mtc,
                                               uint32_t dest,
                                               const meshtastic_Position *position);
extern int mt_admin_message_remove_fixed_position(struct mt_client *mtc,
                                                  uint32_t dest);

/* Group 7: Device Status, UI Config & Diagnostics */
extern int mt_admin_message_device_metadata_request(struct mt_client *mtc);
extern int mt_admin_message_get_device_metadata_request(struct mt_client *mtc,
                                                        uint32_t dest);
extern int mt_admin_message_get_device_connection_status_request(struct mt_client *mtc,
                                                                 uint32_t dest);
extern int mt_admin_message_get_node_remote_hardware_pins_request(struct mt_client *mtc,
                                                                  uint32_t dest);
extern int mt_admin_message_get_canned_messages_request(struct mt_client *mtc,
                                                        uint32_t dest);
extern int mt_admin_message_set_canned_messages(struct mt_client *mtc,
                                                uint32_t dest,
                                                const char *messages);
extern int mt_admin_message_get_ringtone_request(struct mt_client *mtc,
                                                 uint32_t dest);
extern int mt_admin_message_set_ringtone_message(struct mt_client *mtc,
                                                 uint32_t dest,
                                                 const char *ringtone);
extern int mt_admin_message_get_ui_config_request(struct mt_client *mtc,
                                                  uint32_t dest);
extern int mt_admin_message_store_ui_config(struct mt_client *mtc,
                                            uint32_t dest,
                                            const meshtastic_DeviceUIConfig *ui_config);

/* Group 8: Device Lifecycle, Transactions & Maintenance */
extern int mt_admin_message_begin_edit_settings(struct mt_client *mtc,
                                                uint32_t dest);
extern int mt_admin_message_commit_edit_settings(struct mt_client *mtc,
                                                 uint32_t dest);
extern int mt_admin_message_reboot(struct mt_client *mtc,
                                   uint32_t dest,
                                   uint32_t seconds);
extern int mt_admin_message_reboot_ota(struct mt_client *mtc,
                                       uint32_t dest,
                                       uint32_t seconds);
extern int mt_admin_message_shutdown(struct mt_client *mtc,
                                     uint32_t dest,
                                     uint32_t seconds);
extern int mt_admin_message_factory_reset_config(struct mt_client *mtc,
                                                 uint32_t dest);
extern int mt_admin_message_factory_reset_device(struct mt_client *mtc,
                                                 uint32_t dest);
extern int mt_admin_message_enter_dfu_mode(struct mt_client *mtc,
                                           uint32_t dest);
extern int mt_admin_message_backup_preferences(struct mt_client *mtc,
                                               uint32_t dest,
                                               meshtastic_AdminMessage_BackupLocation location);
extern int mt_admin_message_restore_preferences(struct mt_client *mtc,
                                                uint32_t dest,
                                                meshtastic_AdminMessage_BackupLocation location);
extern int mt_admin_message_remove_backup_preferences(struct mt_client *mtc,
                                                      uint32_t dest,
                                                      meshtastic_AdminMessage_BackupLocation location);
extern int mt_admin_message_delete_file(struct mt_client *mtc,
                                        uint32_t dest,
                                        const char *filepath);
extern int mt_admin_message_set_scale(struct mt_client *mtc,
                                      uint32_t dest,
                                      uint32_t scale);
extern int mt_admin_message_exit_simulator(struct mt_client *mtc,
                                           uint32_t dest);
extern int mt_admin_message_set_time(struct mt_client *mtc,
                                     uint32_t dest,
                                     uint32_t epoch_seconds);
extern int mt_admin_message_set_tzdef(struct mt_client *mtc,
                                      uint32_t dest,
                                      const meshtastic_Config_DeviceConfig *current_device_config,
                                      const char *tzdef);

/* Group 9: Mesh Application Payloads (Non-Admin Ports) */
extern int mt_send_position(struct mt_client *mtc,
                            uint32_t dest, uint8_t channel,
                            const meshtastic_Position *position,
                            unsigned int hop_start, bool want_ack);
extern int mt_send_user(struct mt_client *mtc,
                        uint32_t dest, uint8_t channel,
                        const meshtastic_User *user,
                        unsigned int hop_start, bool want_ack);
extern int mt_send_waypoint(struct mt_client *mtc,
                            uint32_t dest, uint8_t channel,
                            const meshtastic_Waypoint *waypoint,
                            unsigned int hop_start, bool want_ack);
extern int mt_send_traceroute(struct mt_client *mtc,
                              uint32_t dest, uint8_t channel,
                              unsigned int hop_start);
extern int mt_send_telemetry(struct mt_client *mtc,
                             uint32_t dest, uint8_t channel,
                             const meshtastic_Telemetry *telemetry,
                             unsigned int hop_start, bool want_ack);
extern int mt_send_telemetry_req(struct mt_client *mtc,
                                 uint32_t dest, uint8_t channel,
                                 pb_size_t variant_tag,
                                 unsigned int hop_start);
extern int mt_send_remote_hardware_req(struct mt_client *mtc,
                                       uint32_t dest, uint8_t channel,
                                       meshtastic_HardwareMessage_Type type,
                                       uint64_t gpio_mask, uint64_t gpio_value,
                                       unsigned int hop_start, bool want_ack);

extern time_t mt_impl_now(void);

EXTERN_C_END

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
