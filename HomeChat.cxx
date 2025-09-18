/*
 * HomeChat.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdarg.h>
#include <time.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <functional>
#include <HomeChat.hxx>

void toLowercase(string &s)
{
    transform(s.begin(), s.end(), s.begin(),
              [](unsigned char c) { return tolower(c); }
        );
}

void trimWhitespace(string &s)
{
    s.erase(s.begin(),
            find_if(s.begin(), s.end(),
                    [](unsigned char ch) {
                        return !isspace(ch);
                    }));
    s.erase(find_if(s.rbegin(), s.rend(),
                    [](unsigned char ch) {
                        return !isspace(ch);
                    }).base(), s.end());
}

HomeChat::HomeChat(shared_ptr<SimpleClient> client)
{
    setClient(client);
    _since = time(NULL);
    clearAuthchansAdminsMates();
}

HomeChat::~HomeChat()
{
    _client = NULL;
}

void HomeChat::setClient(shared_ptr<SimpleClient> client)
{
    _client = client;
}

void HomeChat::setNvm(shared_ptr<BaseNvm> nvm)
{
    _nvm = nvm;
}

static bool operator==(const struct vprintf_callback &lhs,
                       const struct vprintf_callback &rhs)
{
    return (lhs.ctx == rhs.ctx) && (lhs.vprintf == rhs.vprintf);
}

void HomeChat::addPrintfCallback(const struct vprintf_callback &cb)
{
    if ((cb.ctx != NULL) && (cb.vprintf != NULL) &&
        (find(_vpfcb.begin(), _vpfcb.end(), cb) == _vpfcb.end())) {
        _vpfcb.push_back(cb);
    }
}

void HomeChat::addPrintfCallback(
    void *ctx, int (*vprintf)(void *, const char *, va_list))
{
    struct vprintf_callback cb = {
        .ctx = ctx,
        .vprintf = vprintf,
    };

    addPrintfCallback(cb);
}

void HomeChat::delPrintfCallback(const struct vprintf_callback &cb)
{
    _vpfcb.erase(remove(_vpfcb.begin(), _vpfcb.end(), cb), _vpfcb.end());
}

void HomeChat::delPrintfCallback(
    void *ctx, int (*vprintf)(void *, const char *, va_list))
{
    struct vprintf_callback cb = {
        .ctx = ctx,
        .vprintf = vprintf,
    };

    delPrintfCallback(cb);
}

void HomeChat::clearAuthchansAdminsMates(void)
{
    _authchans.clear();
    _admins.clear();
    _mates.clear();
}

bool HomeChat::addAuthChannel(const string &channel,
                              const meshtastic_ChannelSettings_psk_t &psk,
                              bool ignoreDup)
{
    map<string, meshtastic_ChannelSettings_psk_t>::iterator it;

    it = _authchans.find(channel);
    if (it != _authchans.end()) {
        if (ignoreDup == false) {
            return false;
        }
    }

    _authchans[channel] = psk;

    return true;
}

bool HomeChat::addAdmin(uint32_t node_num,
                        const meshtastic_User_public_key_t &pubkey,
                        bool ignoreDup)
{
    map<uint32_t, meshtastic_User_public_key_t>::iterator it;

    it = _admins.find(node_num);
    if (it != _admins.end()) {
        if (ignoreDup == false) {
            return false;
        }
    }

    _admins[node_num] = pubkey;

    return true;
}

bool HomeChat::addMate(uint32_t node_num,
                       const meshtastic_User_public_key_t &pubkey,
                       bool ignoreDup)
{
    map<uint32_t, meshtastic_User_public_key_t>::iterator it;

    it = _mates.find(node_num);
    if (it != _mates.end()) {
        if (ignoreDup == false) {
            return false;
        }
    }

    _mates[node_num] = pubkey;

    return true;
}

bool HomeChat::isAuthChannel(const string &channel) const
{
    bool isAuthChan = false;
    uint8_t chanId = 0xffU;
    map<uint8_t, meshtastic_Channel>::const_iterator itChan;
    map<string, meshtastic_ChannelSettings_psk_t>::const_iterator itAuthChan;

    chanId = _client->getChannel(channel);
    if (chanId == 0xffU) {
        goto done;
    }

    itChan = _client->channels().find(chanId);
    if (itChan == _client->channels().end()) {
        goto done;
    } else if (itChan->second.has_settings == false) {
        goto done;
    }

    itAuthChan = _authchans.find(channel);
    if (itAuthChan != _authchans.end()) {
        if ((itChan->second.settings.psk.size ==
             itAuthChan->second.size) &&
            (memcmp(itChan->second.settings.psk.bytes,
                    itAuthChan->second.bytes,
                    itAuthChan->second.size) == 0)) {
            isAuthChan = true;
        }
    }

done:

    return isAuthChan;
}

const map<uint32_t, meshtastic_User_public_key_t> &
HomeChat::admins(void) const
{
    return _admins;
}

const map<uint32_t, meshtastic_User_public_key_t> &
HomeChat::mates(void) const
{
    return _mates;
}

bool HomeChat::handleTextMessage(const meshtastic_MeshPacket &packet,
                                 const string &_message)
{
    bool result = false;
    bool directMessage = false;
    bool channelMessage = false;
    bool addressed2Me = false;
    string message = _message;
    string first_word;
    string reply;
    uint32_t dest = 0xffffffffU;
    uint8_t channel = 0xffU;
    bool isAdmin = false;
    bool isMate = false;

    if (_client == NULL) {
        goto done;
    }

    if (packet.to == _client->whoami()) {
        directMessage = true;
        dest = packet.from;
        channel = packet.channel;
        this->printf("%s:%c%s\n",
                     _client->getDisplayName(packet.from).c_str(),
                     message.find('\n') == string::npos ? ' ' : '\n',
                     message.c_str());
    } else {
        channelMessage = true;
        dest = 0xffffffffU;
        channel = packet.channel;
        this->printf("%s on #%s:%c%s\n",
                     _client->getDisplayName(packet.from).c_str(),
                     _client->getChannelName(packet.channel).c_str(),
                     message.find('\n') == string::npos ? ' ' : '\n',
                     message.c_str());
    }

    // get first word
    trimWhitespace(message);
    first_word = message.substr(0, message.find(' '));
    toLowercase(first_word);

    if (channelMessage &&
        isAuthChannel(_client->getChannelName(packet.channel))) {
        // on authorized channel, add sender as a mate (if not already)
        if (_nvm->addNvmMate(_client->lookupShortName(packet.from),
                             *_client,
                             false)) {
            _nvm->saveNvm();
            syncFromNvm();
        }
    }

    if (channelMessage &&
        ((first_word == _client->lookupShortName(_client->whoami())) ||
         (first_word == _client->lookupLongName(_client->whoami())) ||
         (first_word.find(_client->whoamiString()) != string::npos) ||
         (first_word == "all"))) {
        // message is addressed to me
        addressed2Me = true;
        message = message.substr(first_word.size());
        trimWhitespace(message);
    }

    getAuthority(packet.from, isAdmin, isMate);

    // rollcall on channel
    if (channelMessage && (first_word == "rollcall") && (isAdmin || isMate)) {
        reply = handleRollcall(packet.from, message);
        goto done;
    }

    // check for authority
    if ((directMessage || addressed2Me) && !isAdmin && !isMate) {
        if (first_word != "all") {
            if (message != getLastMessageFrom(packet.from)) {
                reply = _client->lookupShortName(packet.from) +
                    ", you are not authorized to speak to me!";
            } else {
                reply = "";  // don't be repetitive
            }
        } else {
            reply = "";  // mute if 'all' was the target
        }
        goto done;
    }

    // uptime
    if ((directMessage || addressed2Me) && (message == "uptime")) {
        reply = handleUptime(packet.from, message);
        goto done;
    }

    // zerohops
    if ((directMessage || addressed2Me) && (message == "zerohops")) {
        reply = handleZeroHops(packet.from, message);
        goto done;
    }

    // nodes
    if ((directMessage || addressed2Me) && (message == "nodes")) {
        reply = handleNodes(packet.from, message);
        goto done;
    }

    // meshstats
    if ((directMessage || addressed2Me) && (message == "meshstats")) {
        reply = handleMeshStats(packet.from, message);
        goto done;
    }

    // authchan
    if ((directMessage || addressed2Me) && (first_word == "authchan")) {
        reply = handleAuthchan(packet.from, message);
        goto done;
    }

    // admin
    if ((directMessage || addressed2Me) && (first_word == "admin")) {
        reply = handleAdmin(packet.from, message);
        goto done;
    }

    // mate
    if ((directMessage || addressed2Me) && (first_word == "mate")) {
        reply = handleMate(packet.from, message);
        goto done;
    }

    // status
    if ((directMessage || addressed2Me) && (message == "status")) {
        reply = handleStatus(packet.from, message);
        goto done;
    }

    // wcfg
    if ((directMessage || addressed2Me) && (message == "wcfg")) {
        reply = handleWcfg(packet.from, message);
        goto done;
    }

    // env
    if ((directMessage || addressed2Me) && (message == "env")) {
        reply = handleEnv(packet.from, message);
        if (reply.empty()) {
            reply = "I don't have environment metrics...";
        }
        goto done;
    }

    reply = handleUnknown(packet.from, message);

done:

    setLastMessageFrom(packet.from, message);

    if (!reply.empty()) {
        result = _client->textMessage(dest, channel, reply);
        if (result == false) {
            this->printf("textMessage '%s' failed!\n",
                         reply.c_str());
        } else {
            this->printf("my_reply to %s: %s\n",
                         _client->getDisplayName(packet.from).c_str(),
                         reply.c_str());
        }
    }

    return result;
}

bool HomeChat::syncFromNvm(void)
{
    bool result = true;

    clearAuthchansAdminsMates();

    for (vector<struct nvm_authchan_entry>::const_iterator it =
             _nvm->nvmAuthchans().begin(); it != _nvm->nvmAuthchans().end();
         it++) {
        if (addAuthChannel(it->name, it->psk) == false) {
            result = false;
        }
    }

    for (vector<struct nvm_admin_entry>::const_iterator it =
             _nvm->nvmAdmins().begin(); it != _nvm->nvmAdmins().end();
         it++) {
        if (addAdmin(it->node_num, it->pubkey) == false) {
            result = false;
        }
    }

    for (vector<struct nvm_mate_entry>::const_iterator it =
             _nvm->nvmMates().begin(); it != _nvm->nvmMates().end();
         it++) {
        if (addMate(it->node_num, it->pubkey) == false) {
            result = false;
        }
    }

    return result;
}

void HomeChat::getAuthority(uint32_t node_num,
                            bool &isAdmin, bool &isMate) const
{
    map<uint32_t, meshtastic_NodeInfo>::const_iterator itNodeInfo;
    map<uint32_t, meshtastic_User_public_key_t>::const_iterator itAdmin;
    map<uint32_t, meshtastic_User_public_key_t>::const_iterator itMate;

    isAdmin = false;
    isMate = false;

    itNodeInfo = _client->nodeInfos().find(node_num);
    if (itNodeInfo == _client->nodeInfos().end()) {
        goto done;
    } else if (itNodeInfo->second.has_user == false) {
        goto done;
    }

    itAdmin = _admins.find(node_num);
    if (itAdmin != _admins.end()) {
        if ((itNodeInfo->second.user.public_key.size ==
             itAdmin->second.size) &&
            (memcmp(itNodeInfo->second.user.public_key.bytes,
                    itAdmin->second.bytes, itAdmin->second.size) == 0)) {
            isAdmin = true;
        }
    }

    itMate = _mates.find(node_num);
    if (itMate != _mates.end()) {
        if ((itNodeInfo->second.user.public_key.size ==
             itMate->second.size) &&
            (memcmp(itNodeInfo->second.user.public_key.bytes,
                    itMate->second.bytes, itMate->second.size) == 0)) {
            isMate = true;
        }
    }

done:

    return;
}

void HomeChat::setLastMessageFrom(uint32_t node_num, const string &message)
{
    _lastMessageFrom[node_num] = message;
}

string HomeChat::getLastMessageFrom(uint32_t node_num) const
{
    string s;
    map<uint32_t, string>::const_iterator it;

    it = _lastMessageFrom.find(node_num);
    if (it != _lastMessageFrom.end()) {
        s = it->second;
    }

    return s;
}

string HomeChat::handleRollcall(uint32_t node_num, string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    reply = _client->lookupLongName(node_num) + ", " +
        _client->lookupLongName(_client->whoami()) +
        " is at your service";

    return reply;
}

string HomeChat::handleUptime(uint32_t node_num, string &message)
{
    time_t now;
    uint32_t upsec;
    unsigned int days, hour, min, sec;
    char buf[64];

    (void)(node_num);
    (void)(message);

    now = time(NULL);
    upsec = now - _since;
    sec = upsec % 60;
    min = (upsec / 60) % 60;
    hour = (upsec / 3600) % 24;
    days = (upsec) / 86400;
    if (days == 0) {
        snprintf(buf, sizeof(buf) - 1, "Up-time: %.2u:%.2u:%.2u",
                 hour, min, sec);
    } else {
        snprintf(buf, sizeof(buf) - 1, "Up-time: %ud %.2u:%.2u:%.2u",
                 days, hour, min, sec);
    }

    return string(buf);
}

string HomeChat::handleZeroHops(uint32_t node_num, string &message)
{
    string reply;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    (void)(node_num);
    (void)(message);

    reply = "my zero-hop neighbors:";
    for (it = _client->nodeInfos().begin();
         it != _client->nodeInfos().end();
         it++) {
        if (it->second.num == _client->whoami()) {
            continue;
        }

        if (!(it->second.has_hops_away) || (it->second.hops_away > 0)) {
            continue;
        }

        reply += "\n";
        reply += _client->getDisplayName(it->second.num);
    }

    return reply;
}

string HomeChat::handleNodes(uint32_t node_num, string &message)
{
    string reply;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;
    unsigned int hops, count;

    (void)(node_num);
    (void)(message);

    reply = "nodes seen: ";
    reply += to_string(_client->nodeInfos().size());

    for (hops = 0; hops < 15; hops++) {
        count = 0;
        for (it = _client->nodeInfos().begin();
             it != _client->nodeInfos().end();
             it++) {
            if (it->second.num == _client->whoami()) {
                continue;
            }

            if (it->second.hops_away == hops) {
                count++;
            }
        }

        if (count > 0) {
            reply += "\n";
            reply += "hop";
            reply += to_string(hops);
            reply += "=";
            reply += to_string(count);
        }
    }

    return reply;
}

string HomeChat::handleMeshStats(uint32_t node_num, string &message)
{
    stringstream ss;
    map<uint32_t, meshtastic_DeviceMetrics>::const_iterator dev;

    (void)(node_num);
    (void)(message);

    ss << "direct messages (sent/recv): "
       << to_string(_client->dmTx()) << "/" << to_string(_client->dmRx())
       << endl;
    ss << "channel messages (sent/recv): "
       << to_string(_client->cmTx()) << "/" << to_string(_client->cmRx());

    dev = _client->deviceMetrics().find(_client->whoami());
    if (dev != _client->deviceMetrics().end()) {
        if (dev->second.has_channel_utilization) {
            ss << endl << "channel_utilization: "
               << setprecision(3) << dev->second.channel_utilization << "%";
        }
        if (dev->second.has_air_util_tx) {
            ss << endl << "air_util_tx: "
               << setprecision(3) << dev->second.air_util_tx << "%";
        }
    }

    return ss.str();
}

string HomeChat::handleAuthchan(uint32_t node_num, string &message)
{
    stringstream ss;
    bool isAdmin = false, isMate = false;
    bool isViolated = false;
    char delimiter = ' ';
    istringstream iss;
    string token;
    vector<string> tokens;
    unsigned int i;
    bool result;
    int pass = 0, fail = 0;

    getAuthority(node_num, isAdmin, isMate);

    trimWhitespace(message);
    iss = istringstream(message);
    while (getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }

    if (tokens.size() == 1) {
        i = 0;
        for (map<string, meshtastic_ChannelSettings_psk_t>::const_iterator it =
                 _authchans.begin(); it != _authchans.end(); it++, i++) {
            if (i > 0) {
                ss << endl;
            }
            ss << it->first;
        }
    } else {
        if ((tokens.size() == 3) && (tokens[1] == "add")) {
            if (isAdmin == false) {
                isViolated = true;
                goto done;
            }

            if (_nvm->addNvmAuthChannel(tokens[2], *_client) &&
                _nvm->saveNvm() && syncFromNvm()) {
                result = true;
            } else {
                result = false;
            }

            if (result == true) {
                ss << "added " << tokens[2];
            } else {
                ss << "add " << tokens[2] << " failed!";
            }
        } else if ((tokens.size() == 3) && (tokens[1] == "del")) {
            if (isAdmin == false) {
                isViolated = true;
                goto done;
            }

            if (_nvm->delNvmAuthChannel(tokens[2]) &&
                _nvm->saveNvm() && syncFromNvm()) {
                result = true;
            } else {
                result = false;
            }

            if (result == true) {
                ss << "deleted " << tokens[2];
            } else {
                ss << "delete " << tokens[2] << " failed!";
            }
        } else if ((tokens.size() >= 3) && (tokens[1] == "set")) {
            if (isAdmin == false) {
                isViolated = true;
                goto done;
            }

            _nvm->clearNvmAuthChannels();
            for (i = 2; i < tokens.size(); i++) {
                result = _nvm->addNvmAuthChannel(tokens[i], *_client);
                if (result == true) {
                    pass++;
                } else {
                    fail++;
                }
            }
            result = _nvm->saveNvm();
            syncFromNvm();

            ss << "set " << pass << " authchan entries";
            if (fail > 0) {
                ss << " (" << fail << " entries failed to set!)";
            }
        } else {
            ss << "syntax error!";
        }
    }

done:

    if (isViolated) {
        ss.clear();
        ss << _client->getDisplayName(node_num) << ", you are not an admin!";
    }

    return ss.str();
}

string HomeChat::handleAdmin(uint32_t node_num, string &message)
{
    stringstream ss;
    bool isAdmin = false, isMate = false;
    bool isViolated = false;
    char delimiter = ' ';
    istringstream iss;
    string token;
    vector<string> tokens;
    unsigned int i;
    bool result;
    int pass = 0, fail = 0;

    getAuthority(node_num, isAdmin, isMate);

    trimWhitespace(message);
    iss = istringstream(message);
    while (getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }


    if (tokens.size() == 1) {
        i = 0;
        for (map<uint32_t, meshtastic_User_public_key_t>::const_iterator it =
                 _admins.begin(); it != _admins.end(); it++, i++) {
            if (i > 0) {
                ss << endl;
            }
            ss << _client->getDisplayName(it->first);
        }
    } else {
        if ((tokens.size() == 3) && (tokens[1] == "add")) {
            if (isAdmin == false) {
                isViolated = true;
                goto done;
            }

            if (_nvm->addNvmAdmin(tokens[2], *_client) &&
                _nvm->saveNvm() && syncFromNvm()) {
                result = true;
            } else {
                result = false;
            }

            if (result == true) {
                ss << "added " << tokens[2];
            } else {
                ss << "add " << tokens[2] << " failed!";
            }
        } else if ((tokens.size() == 3) && (tokens[1] == "del")) {
            if (isAdmin == false) {
                isViolated = true;
                goto done;
            }

            if (_nvm->delNvmAdmin(tokens[2], *_client) &&
                _nvm->saveNvm() && syncFromNvm()) {
                result = true;
            } else {
                result = false;
            }

            if (result == true) {
                ss << "deleted " << tokens[2];
            } else {
                ss << "delete " << tokens[2] << " failed!";
            }
        } else if ((tokens.size() >= 3) && (tokens[1] == "set")) {
            if (isAdmin == false) {
                isViolated = true;
                goto done;
            }

            _nvm->clearNvmAdmins();
            for (i = 2; i < tokens.size(); i++) {
                result = _nvm->addNvmAdmin(tokens[i], *_client);
                if (result == true) {
                    pass++;
                } else {
                    fail++;
                }
            }
            result = _nvm->saveNvm();
            syncFromNvm();

            ss << "set " << pass << " admin entries";
            if (fail > 0) {
                ss << " (" << fail << " entries failed to set!)";
            }
        } else {
            ss << "syntax error!";
        }
    }

done:

    if (isViolated) {
        ss.clear();
        ss << _client->getDisplayName(node_num) << ", you are not an admin!";
    }

    return ss.str();
}

string HomeChat::handleMate(uint32_t node_num, string &message)
{
    stringstream ss;
    bool isAdmin = false, isMate = false;
    bool isViolated = false;
    char delimiter = ' ';
    istringstream iss;
    string token;
    vector<string> tokens;
    unsigned int i;
    bool result;
    int pass = 0, fail = 0;

    getAuthority(node_num, isAdmin, isMate);

    trimWhitespace(message);
    iss = istringstream(message);
    while (getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }


    if (tokens.size() == 1) {
        i = 0;
        for (map<uint32_t, meshtastic_User_public_key_t>::const_iterator it =
                 _mates.begin(); it != _mates.end(); it++, i++) {
            if (i > 0) {
                ss << endl;
            }
            ss << _client->getDisplayName(it->first);
        }
    } else {
        if ((tokens.size() == 3) && (tokens[1] == "add")) {
            if (isAdmin == false) {
                isViolated = true;
                goto done;
            }

            if (_nvm->addNvmMate(tokens[2], *_client) &&
                _nvm->saveNvm() && syncFromNvm()) {
                result = true;
            } else {
                result = false;
            }

            if (result == true) {
                ss << "added " << tokens[2];
            } else {
                ss << "add " << tokens[2] << " failed!";
            }
        } else if ((tokens.size() == 3) && (tokens[1] == "del")) {
            if (isAdmin == false) {
                isViolated = true;
                goto done;
            }

            if (_nvm->delNvmMate(tokens[2], *_client) &&
                _nvm->saveNvm() && syncFromNvm()) {
                result = true;
            } else {
                result = false;
            }

            if (result == true) {
                ss << "deleted " << tokens[2];
            } else {
                ss << "delete " << tokens[2] << " failed!";
            }
        } else if ((tokens.size() >= 3) && (tokens[1] == "set")) {
            if (isAdmin == false) {
                isViolated = true;
                goto done;
            }

            _nvm->clearNvmMates();
            for (i = 2; i < tokens.size(); i++) {
                result = _nvm->addNvmMate(tokens[i], *_client);
                if (result == true) {
                    pass++;
                } else {
                    fail++;
                }
            }
            result = _nvm->saveNvm();
            syncFromNvm();

            ss << "set " << pass << " mate entries";
            if (fail > 0) {
                ss << " (" << fail << " entries failed to set!)";
            }
        } else {
            ss << "syntax error!";
        }
    }

done:

    if (isViolated) {
        ss.clear();
        ss << _client->getDisplayName(node_num) << ", you are not an admin!";
    }

    return ss.str();
}

string HomeChat::handleEnv(uint32_t node_num, string &message)
{
    stringstream ss;
    map<uint32_t, meshtastic_EnvironmentMetrics>::const_iterator env;

    (void)(node_num);
    (void)(message);

    env = _client->environmentMetrics().find(_client->whoami());
    if (env != _client->environmentMetrics().end()) {
        if (env->second.has_temperature) {
            ss << "temperature: ";
            ss << setprecision(3) << env->second.temperature;
        }
        if (env->second.has_relative_humidity) {
            if (ss.tellp() != 0) {
                ss << endl;
            }
            ss << "relative_humidity: ";
            ss << setprecision(3) << env->second.relative_humidity;
        }
        if (env->second.has_barometric_pressure) {
            if (ss.tellp() != 0) {
                ss << endl;
            }
            ss << "barometric_pressure: ";
                ss << setprecision(3) << env->second.barometric_pressure;
        }
    }

    return ss.str();
}

string HomeChat::handleWcfg(uint32_t node_num, string &message)
{
    string reply;
    bool result;

    (void)(node_num);
    (void)(message);

    result = _client->sendWantConfig();
    if (result) {
        reply = "ok";
    } else {
        reply = "failed";
    }

    return reply;
}

string HomeChat::handleStatus(uint32_t node_num, string &message)
{
    (void)(node_num);
    (void)(message);

    return string();
}

string HomeChat::handleUnknown(uint32_t node_num, string &message)
{
    (void)(node_num);
    (void)(message);

    return string();
}

int HomeChat::printf(const char *format, ...) const
{
    int ret = 0;
    va_list ap;

    va_start(ap, format);
    ret = this->vprintf(format, ap);
    for (vector<struct vprintf_callback>::const_iterator it = _vpfcb.begin();
         it != _vpfcb.end(); it++) {
        it->vprintf(it->ctx, format, ap);
    }
    va_end(ap);

    return ret;
}

int HomeChat::vprintf(const char *format, va_list ap) const
{
    (void)(format);
    (void)(ap);

    return 0;
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
