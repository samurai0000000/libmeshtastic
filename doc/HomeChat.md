# HomeChat Protocol and Message Reference

`HomeChat` is an interactive chatbot, remote administration agent, and control framework built on top of `libmeshtastic`. It allows authorized users and mate nodes on a Meshtastic mesh network to query status, inspect mesh topology, configure NVM settings, synchronize system clocks, and invoke domain-specific device controls over text messages.

---

## 1. Addressing and Routing

HomeChat processes incoming text messages received over direct messages (DM) or channels:

- **Direct Messages (DM)**:
  - Addressed directly to the node's `num` (`packet.to == whoami()`).
  - Replies are sent back as direct messages to the sender (`dest = packet.from`).
- **Channel Messages**:
  - Broadcast or group messages received on a channel (`packet.to == 0xffffffffU`).
  - HomeChat processes channel messages addressed with prefixes matching:
    - Node short name (e.g. `ROOF status`)
    - Node long name (e.g. `MeshRoof status`)
    - Node ID hex string (e.g. `!2bf941d4 status`)
    - Target `all` (e.g. `all rollcall`, `all version`)
  - Replies to addressed channel messages are broadcast back on that channel.

---

## 2. Authority and Security Model

To prevent unauthorized access, commands are gated by node authority levels:

| Role | Description | Capabilities |
| :--- | :--- | :--- |
| **Admin** | Node ID and public key registered in `nvmAdmins()`. | Full access: query status, modify `authchan`, add/del/clear `admin` and `mate` lists, execute control commands. |
| **Mate** | Node ID and public key registered in `nvmMates()` or `_mates`. | Operational access: query status, telemetry, rollcall, and device controls. Cannot alter admin or authchan lists. |
| **Authorized Channel** | Packet arrived on an authorized channel (`nvmAuthchans()`). | Senders on authorized channels are automatically learned as mates in NVM. Granted operational access. |
| **Unauthorized** | Senders not recognized as Admin, Mate, or from an Authorized Channel. | Blocked with `"you are not authorized to speak to me!"` (or muted for `all` targets). |

---

## 3. Built-in Command Reference

HomeChat replies adhere to a concise lowercase `key=val` format suitable for low-bandwidth LoRa transmission.

### 3.1 Status & Diagnostics

- **`rollcall`**
  - Responds with node identity and status:
    ```text
    <shortName> here!
    ```
- **`uptime`**
  - Reports system and radio connection uptime:
    ```text
    uptime: host=<days>d <hh>:<mm>:<ss> client=<days>d <hh>:<mm>:<ss>
    ```
- **`version`**
  - Reports software/firmware version string:
    ```text
    version: <versionString>
    ```
- **`status`**
  - Reports operational health and status summary (default: `status: operational`, extensible by subclasses).
- **`env`**
  - Reports environmental telemetry (temperature, relative humidity, pressure, gas resistance, IAQ).
- **`zerohops`**
  - Lists directly heard nodes (0 hops away) with SNR and RSSI:
    ```text
    zerohops: count=<N> !<id1>=<snr>dB ...
    ```
- **`nodes`**
  - Summarizes the total known nodes in the device database:
    ```text
    nodes: total=<N>
    ```
- **`meshstats`**
  - Reports LoRa mesh statistics (packets received, transmitted, duplicates, bad packets):
    ```text
    meshstats: rx=<N> tx=<N> dup=<N> bad=<N>
    ```
- **`wcfg`**
  - Reports network / WiFi configuration status where supported.

---

### 3.2 Configuration & Management (Admin Only)

- **`authchan`**
  - `authchan`: Lists configured authorized channels.
  - `authchan add <chanName> [psk]`: Adds an authorized channel.
  - `authchan del <chanName>`: Removes an authorized channel.
  - `authchan clear`: Clears all authorized channels.
- **`admin`**
  - `admin`: Lists configured admin node IDs and public keys.
  - `admin add <node>`: Adds an admin node (by hex ID `!xxxxxxxx` or short name).
  - `admin del <node>`: Removes an admin node.
  - `admin clear`: Clears configured admins.
  - `admin set <node1> [node2 ...]`: Replaces the admin list.
- **`mate`**
  - `mate`: Lists configured mate nodes.
  - `mate add <node>`: Adds a mate node.
  - `mate del <node>`: Removes a mate node.
  - `mate clear`: Clears configured mates.
  - `mate set <node1> [node2 ...]`: Replaces the mate list.

---

## 4. Automated Protocols & Broadcasts

### 4.1 Time Synchronization (`time: <epoch> [tz]`)
Nodes on authorized channels or direct messages can broadcast the current wall-clock epoch time and timezone string:
```text
time: 1740000000 Asia/Taipei
```
When received from an authorized sender, HomeChat synchronizes the host/microcontroller RTC and Meshtastic radio clock via `adminSetTime()` / `adminSetTimezone()`.

### 4.2 Boot-up Announcement
Upon establishing a connection to the Meshtastic radio, if a robot channel is resolved, HomeChat broadcasts an announcement on the robot channel:
```text
boot-up: <shortName>
```

---

## 5. Subclass Extensibility

Applications extend `HomeChat` by overriding:
- `handleUnknown(node_num, dest, channel, message)`: To intercept application-specific keywords (e.g. `tv`, `ac`, `pump`, `led`, `wifi`, `amplify`).
- `handleStatus(...)` / `handleEnv(...)`: To append local sensors, battery voltages, onboard temperatures, or custom subsystem telemetry.
