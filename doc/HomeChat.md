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
  - Announces presence in prose, addressed to the asker. An optional
    argument targets a single node, and nodes that do not match stay
    silent:
    ```text
    <asker long name>, <my long name> is at your service
    ```
  - Robot firmwares override this with a machine-readable
    `rollcall: app=<app> ver=<x.y.z> hw=<platform> caps=<list>`.
- **`uptime`**
  - Reports host uptime only. The day field is omitted below 24 hours:
    ```text
    uptime: <hh>:<mm>:<ss>
    uptime: <N>d <hh>:<mm>:<ss>
    ```
- **`version`**
  - Reports the banner, version, Meshtastic firmware version where
    known, build stamp, and copyright, one per line, with no `version:`
    prefix. Replies `unknown!` when the client supplied none of them:
    ```text
    <banner>
    <version>
    Meshtastic firmware: <firmwareVersion>
    <built>
    <copyright>
    ```
- **`status`**
  - The base implementation returns an empty string, which suppresses
    the reply entirely. Subclasses override it; `meshroof` and
    `meshpump` answer with their own `status: <key>=<value> ...` lines.
- **`env`**
  - Reports environmental telemetry from the client's cached metrics.
    Each field is omitted when the sensor is absent, and the whole
    reply is empty when no metrics are cached at all:
    ```text
    env: temp=<c> rh=<pct> bp=<hpa>
    ```
  - Subclasses append their own onboard temperature, such as
    `temp_chip=` on `meshroof` or `temp_board=` on `meshroom`. Because
    the base part can be empty, an override's field may arrive with no
    `env:` prefix in front of it.
- **`zerohops`**
  - Lists the display names of nodes heard directly, comma separated.
    The reply verb is singular and carries no count, SNR, or RSSI:
    ```text
    zerohop: nodes=<name>,<name>,...
    ```
- **`nodes`**
  - Reports the size of the node database followed by a per-hop
    breakdown, omitting hop counts that are zero:
    ```text
    nodes: count=<N> hop0=<N> hop1=<N> ...
    ```
- **`meshstats`**
  - Reports message counters in prose across multiple lines, with
    channel utilization appended when device metrics are available:
    ```text
    direct messages (sent/recv): <N>/<N>
    channel messages (sent/recv): <N>/<N>
    channel_utilization: <pct>%
    air_util_tx: <pct>%
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
