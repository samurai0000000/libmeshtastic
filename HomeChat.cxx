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

HomeChat::HomeChat(shared_ptr<SimpleClient> client)
{
    setClient(client);
    _since = time(NULL);
}

HomeChat::~HomeChat()
{
    _client = NULL;
}

void HomeChat::setClient(shared_ptr<SimpleClient> client)
{
    _client = client;
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

    if (_client == NULL) {
        goto done;
    }

    if (packet.to == _client->whoami()) {
        directMessage = true;
        dest = packet.from;
        channel = packet.channel;
        this->printf("%s: %s\n",
                     _client->getDisplayName(packet.from).c_str(),
                     message.c_str());
    } else {
        channelMessage = true;
        dest = 0xffffffffU;
        channel = packet.channel;
        this->printf("%s on #%s: %s\n",
                     _client->getDisplayName(packet.from).c_str(),
                     _client->getChannelName(packet.channel).c_str(),
                     message.c_str());
    }

    // tolower
    transform(message.begin(), message.end(), message.begin(),
              [](unsigned char c) { return tolower(c); }
        );
    // trim white space and punct (front)
    message.erase(message.begin(),
                  find_if(message.begin(), message.end(),
                          [](unsigned char ch) {
                              return !isspace(ch) && !ispunct(ch);
                          }));
    // trim white spaces and punct (back)
    message.erase(find_if(message.rbegin(), message.rend(),
                          [](unsigned char ch) {
                              return !isspace(ch) && !ispunct(ch);
                          }).base(), message.end());

    // get first word
    first_word = message.substr(0, message.find(' '));

    if (channelMessage &&
        ((first_word == _client->lookupShortName(_client->whoami())) ||
         (first_word == _client->lookupLongName(_client->whoami())) ||
         (first_word.find(_client->whoamiString()) != string::npos) ||
         (first_word == "all"))) {
        // message is addressed to me
        addressed2Me = true;
        message = message.substr(first_word.size());
        // trim white spaces and punct (front)
        message.erase(message.begin(),
                      find_if(message.begin(), message.end(),
                              [](unsigned char ch) {
                                  return !isspace(ch) && !ispunct(ch);
                              }));
        // trim white space and punct (back)
        message.erase(find_if(message.rbegin(), message.rend(),
                              [](unsigned char ch) {
                                  return !isspace(ch) && !ispunct(ch);
                              }).base(), message.end());
    }

    // rollcall on channel
    if (channelMessage && (first_word == "rollcall") &&
        isAuthorized(packet.from)) {
        reply = _client->lookupShortName(packet.from) + ", " +
            _client->lookupShortName(_client->whoami()) +
            " is at your service";
        goto done;
    }

    // check for authority
    if ((directMessage || addressed2Me) && !isAuthorized(packet.from)) {
        reply = _client->lookupShortName(packet.from) +
            ", you are not authorized to speak to me!";
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

    // status
    if ((directMessage || addressed2Me) && (message == "status")) {
        reply = handleStatus(packet.from, message);
        goto done;
    }

    // env
    if ((directMessage || addressed2Me) && (message == "env")) {
        stringstream ss;
        map<uint32_t, meshtastic_EnvironmentMetrics>::const_iterator env;
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
            reply = ss.str();
        } else {
            reply = "I don't have environment metrics";
        }
        goto done;
    }

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

bool HomeChat::isAuthorized(uint32_t node_num) const
{
    (void)(node_num);

    return true;
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

string HomeChat::handleUptime(uint32_t node_num, const string &message)
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

string HomeChat::handleZeroHops(uint32_t node_num, const string &message)
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

string HomeChat::handleNodes(uint32_t node_num, const string &message)
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

string HomeChat::handleMeshStats(uint32_t node_num, const string &message)
{
    string reply;

    (void)(node_num);
    (void)(message);

    reply = "direct messages (sent/recv): ";
    reply += to_string(_client->dmTx());
    reply += "/";
    reply += to_string(_client->dmRx());
    reply += "\n";
    reply += "channel messages (sent/recv): ";
    reply += to_string(_client->cmTx());
    reply += "/";
    reply += to_string(_client->cmRx());

    return reply;
}

string HomeChat::handleStatus(uint32_t node_num, const string &message)
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
