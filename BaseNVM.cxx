/*
 * BaseNVM.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <cstring>
#include <BaseNVM.hxx>

BaseNVM::BaseNVM()
{

}

BaseNVM::~BaseNVM()
{

}

bool BaseNVM::addNvmAuthChannel(const string &channel,
                                meshtastic_ChannelSettings_psk_t psk,
                                bool ignoreDup)
{
    bool result = false;
    struct nvm_authchan_entry entry;
    vector<struct nvm_authchan_entry>::iterator it;

    if (channel.length() > sizeof(entry.name)) {
        result = false;
        goto done;
    }

    for (it = _nvm_authchans.begin(); it != _nvm_authchans.end(); it++) {
        if (channel == it->name) {
            if (ignoreDup == false) {
                result = false;
                goto done;
            }
            break;
        }
    }

    memset(&entry, 0x0, sizeof(entry));
    memcpy(entry.name, channel.c_str(), channel.length());
    entry.psk.size = psk.size;
    memcpy(entry.psk.bytes, psk.bytes, psk.size);

    if (it == _nvm_authchans.end()) {
        _nvm_authchans.push_back(entry);
    } else {
        *it = entry;
    }

    result = true;

done:

    return result;
}


bool BaseNVM::addNvmAuthChannel(const string &channel,
                                const SimpleClient &client)
{
    bool result = false;
    map<uint8_t, meshtastic_Channel>::const_iterator it;

    for (it = client.channels().begin(); it != client.channels().end(); it++) {
        if (it->second.has_settings != true) {
            continue;
        }
        if (channel == it->second.settings.name) {
            break;
        }
    }

    if (it != client.channels().end()) {
        result = addNvmAuthChannel(channel, it->second.settings.psk);
    }

    return result;
}

bool BaseNVM::delNvmAuthChannel(const string &channel)
{
    vector<struct nvm_authchan_entry>::iterator it;

    for (it = _nvm_authchans.begin(); it != _nvm_authchans.end(); it++) {
        if (channel == it->name) {
            _nvm_authchans.erase(it);
            return true;
        }
    }

    return false;
}

bool BaseNVM::addNvmAdmin(uint32_t node_num,
                          meshtastic_User_public_key_t pubkey,
                          bool ignoreDup)
{
    bool result = false;
    struct nvm_admin_entry entry;
    vector<struct nvm_admin_entry>::iterator it;

    for (it = _nvm_admins.begin(); it != _nvm_admins.end(); it++) {
        if (node_num == it->node_num) {
            if (ignoreDup == false) {
                result = false;
                goto done;
            }
            break;
        }
    }

    memset(&entry, 0x0, sizeof(entry));
    entry.node_num = node_num;
    entry.pubkey.size = pubkey.size;
    memcpy(entry.pubkey.bytes, pubkey.bytes, pubkey.size);

    if (it == _nvm_admins.end()) {
        _nvm_admins.push_back(entry);
    } else {
        *it = entry;
    }

#if 0
    // admin is automatically a mate
    addNvmMate(node_num, pubkey, ignoreDup);
#endif

    result = true;

done:

    return result;
}

bool BaseNVM::addNvmAdmin(const string &name, const SimpleClient &client)
{
    bool result = false;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    for (it = client.nodeInfos().begin(); it != client.nodeInfos().end(); it++) {
        if (it->second.has_user != true) {
            continue;
        }
        string id = client.idString(it->second.num);
        string exid = "!" + id;
        if ((name == id) || (name == exid) ||
            (name == client.lookupShortName(it->second.num)) ||
            (name == client.lookupLongName(it->second.num))) {
            break;
        }
    }

    if (it != client.nodeInfos().end()) {
        result = addNvmAdmin(it->second.num, it->second.user.public_key);
    }

    return result;
}

bool BaseNVM::delNvmAdmin(uint32_t node_num)
{
    vector<struct nvm_admin_entry>::iterator it;

    for (it = _nvm_admins.begin(); it != _nvm_admins.end(); it++) {
        if (node_num == it->node_num) {
            _nvm_admins.erase(it);
            return true;
        }
    }

    return false;
}

bool BaseNVM::delNvmAdmin(const string &name, const SimpleClient &client)
{
    bool result = false;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    for (it = client.nodeInfos().begin(); it != client.nodeInfos().end(); it++) {
        if (it->second.has_user != true) {
            continue;
        }
        string id = client.idString(it->second.num);
        string exid = "!" + id;
        if ((name == id) || (name == exid) ||
            (name == client.lookupShortName(it->second.num)) ||
            (name == client.lookupLongName(it->second.num))) {
            break;
        }
    }

    if (it != client.nodeInfos().end()) {
        result = delNvmAdmin(it->second.num);
    }

    return result;
}

bool BaseNVM::addNvmMate(uint32_t node_num,
                         meshtastic_User_public_key_t pubkey,
                         bool ignoreDup)
{
    bool result = false;
    struct nvm_mate_entry entry;
    vector<struct nvm_mate_entry>::iterator it;

    for (it = _nvm_mates.begin(); it != _nvm_mates.end(); it++) {
        if (node_num == it->node_num) {
            if (ignoreDup == false) {
                result = false;
                goto done;
            }
            break;
        }
    }

    memset(&entry, 0x0, sizeof(entry));
    entry.node_num = node_num;
    entry.pubkey.size = pubkey.size;
    memcpy(entry.pubkey.bytes, pubkey.bytes, pubkey.size);

    if (it == _nvm_mates.end()) {
        _nvm_mates.push_back(entry);
    } else {
        *it = entry;
    }

    result = true;

done:

    return result;
}

bool BaseNVM::addNvmMate(const string &name, const SimpleClient &client)
{
    bool result = false;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    for (it = client.nodeInfos().begin(); it != client.nodeInfos().end(); it++) {
        if (it->second.has_user != true) {
            continue;
        }
        string id = client.idString(it->second.num);
        string exid = "!" + id;
        if ((name == id) || (name == exid) ||
            (name == client.lookupShortName(it->second.num)) ||
            (name == client.lookupLongName(it->second.num))) {
            break;
        }
    }

    if (it != client.nodeInfos().end()) {
        result = addNvmMate(it->second.num, it->second.user.public_key);
    }

    return result;
}

bool BaseNVM::delNvmMate(uint32_t node_num)
{
    vector<struct nvm_mate_entry>::iterator it;

    for (it = _nvm_mates.begin(); it != _nvm_mates.end(); it++) {
        if (node_num == it->node_num) {
            _nvm_mates.erase(it);
            return true;
        }
    }

    return false;
}

bool BaseNVM::delNvmMate(const string &name, const SimpleClient &client)
{
    bool result = false;
    map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    for (it = client.nodeInfos().begin(); it != client.nodeInfos().end(); it++) {
        if (it->second.has_user != true) {
            continue;
        }
        string id = client.idString(it->second.num);
        string exid = "!" + id;
        if ((name == id) || (name == exid) ||
            (name == client.lookupShortName(it->second.num)) ||
            (name == client.lookupLongName(it->second.num))) {
            break;
        }
    }

    if (it != client.nodeInfos().end()) {
#if 0
        delNvmAdmin(it->secon.num);  // automatic removal from admin
#endif
        result = delNvmMate(it->second.num);
    }

    return result;
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
