/*
 * MeshNVM.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <libconfig.h++>
#include <MeshNVM.hxx>

using namespace libconfig;

MeshNVM::MeshNVM()
{
    _node_num = 0x0U;
}

MeshNVM::~MeshNVM()
{

}

bool MeshNVM::setupFor(uint32_t node_num)
{
    bool result = false;
    string home;
    string path;
    char node_num_hex[16];
    int fd;

    if (node_num == 0x0U || node_num == (uint32_t) (-1)) {
        result = false;
        goto done;
    }

    home = getenv("HOME");
    if (home.empty()) {
        result = false;
        goto done;
    }

    snprintf(node_num_hex, sizeof(node_num_hex) - 1, "%.8x", node_num);
    path = home + "/.meshmon." + node_num_hex;

    // 'touch' to test the path validity
    fd = open(path.c_str(),
              O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK,
              0666);
    if (fd == -1) {
        result = false;
        goto done;
    } else {
        close(fd);
        fd = -1;
    }

    _path = path;
    result = true;

done:

    return result;
}

static string node_num_to_string(uint32_t node_num)
{
    char buf[16];
    uint8_t *bytes = (uint8_t *) &node_num;

    for (unsigned int i = 0; i < 4; i++) {
        sprintf(&buf[i * 2], "%.2x", bytes[i]);
    }

    return string(buf);
}

static bool string_to_node_num(const string &s, uint32_t *node_num)
{
    bool result = false;
    uint8_t *bytes = (uint8_t *) node_num;

    if (s.length() != 8) {
        goto done;
    }

    for (size_t i = 0; i < 8; i += 2) {
        string bytestr = s.substr(i, 2);
        uint8_t byteval = static_cast<uint8_t>(stoi(bytestr, nullptr, 16));
        bytes[i / 2] = byteval;
    }

    result = true;

done:

    return result;
}

static bool string_to_byte32(const string &s, uint8_t *bytes)
{
    bool result = false;

    if (s.length() != 64) {
        goto done;
    }

    for (size_t i = 0; i < 64; i += 2) {
        string bytestr = s.substr(i, 2);
        uint8_t byteval = static_cast<uint8_t>(stoi(bytestr, nullptr, 16));
        bytes[i / 2] = byteval;
    }

    result = true;

done:

    return result;
}

static string psk_to_string(const meshtastic_ChannelSettings_psk_t psk)
{
    stringstream ss;
    unsigned int i;

    for (i = 0; i < psk.size; i++) {
        ss << hex << setfill('0') << setw(2)
           << static_cast<unsigned int>(psk.bytes[i]);
    }

    return ss.str();
}

static string pubkey_to_string(const meshtastic_User_public_key_t pubkey)
{
    stringstream ss;
    unsigned int i;

    for (i = 0; i < pubkey.size; i++) {
        ss << hex << setfill('0') << setw(2)
           << static_cast<unsigned int>(pubkey.bytes[i]);
    }

    return ss.str();
}

bool MeshNVM::loadNvm(void)
{
    bool result = false;
    Config cfg;

    if (_path.empty()) {
        return false;
    }

    try {
        cfg.readFile(_path.c_str());
    } catch (FileIOException &e) {
        return false;
    } catch (ParseException &e) {
        return false;
    }

    _nvm_authchans.clear();
    _nvm_admins.clear();
    _nvm_mates.clear();

    Setting &root = cfg.getRoot();

    try {
        Setting &authchans = root["authchans"];
        for (int i = 0; i < authchans.getLength(); i++) {
            Setting &authchan = authchans[i];
            struct nvm_authchan_entry entry;
            size_t len;
            string name;
            string psk;

            bzero(&entry, sizeof(entry));
            authchan.lookupValue("name", name);
            authchan.lookupValue("psk", psk);
            entry.psk.size = 32;
            if (string_to_byte32(psk, entry.psk.bytes) == false) {
                continue;
            }
            len = name.size() > sizeof(entry.name) ?
                sizeof(entry.name) : name.size();
            memcpy(entry.name, name.c_str(), len);
            _nvm_authchans.push_back(entry);
        }
    } catch (SettingNotFoundException &e) {
    }

    try {
        Setting &admins = root["admins"];
        for (int i = 0; i < admins.getLength(); i++) {
            Setting &admin = admins[i];
            struct nvm_admin_entry entry;
            string node;
            uint32_t node_num;
            string pubkey;

            bzero(&entry, sizeof(entry));
            admin.lookupValue("node", node);
            admin.lookupValue("pubkey", pubkey);
            if (string_to_node_num(node, &node_num) == false) {
                continue;
            }
            entry.node_num = node_num;
            entry.pubkey.size = 32;
            if (string_to_byte32(pubkey, entry.pubkey.bytes) == false) {
                continue;
            }
            _nvm_admins.push_back(entry);
        }
    } catch (SettingNotFoundException &e) {
    }

    try {
        Setting &mates = root["mates"];
        for (int i = 0; i < mates.getLength(); i++) {
            Setting &mate = mates[i];
            struct nvm_mate_entry entry;
            string node;
            uint32_t node_num;
            string pubkey;

            bzero(&entry, sizeof(entry));
            mate.lookupValue("node", node);
            mate.lookupValue("pubkey", pubkey);
            if (string_to_node_num(node, &node_num) == false) {
                continue;
            }
            entry.node_num = node_num;
            entry.pubkey.size = 32;
            if (string_to_byte32(pubkey, entry.pubkey.bytes) == false) {
                continue;
            }
            _nvm_mates.push_back(entry);
        }
    } catch (SettingNotFoundException &e) {
    }

    result = true;
    _changed = true;

    return result;
}

bool MeshNVM::saveNvm(void)
{
    bool result = false;
    Config cfg;

    if (_path.empty()) {
        return false;
    }

    try {
        cfg.readFile(_path.c_str());
    } catch (FileIOException &e) {
        return false;
    } catch (ParseException &e) {
        return false;
    }

    Setting &root = cfg.getRoot();
    if(root.exists("authchans")) {
        root.remove("authchans");
    }
    Setting &authchans = root.add("authchans", Setting::TypeList);
    for (vector<struct nvm_authchan_entry>::const_iterator it =
             _nvm_authchans.begin(); it != _nvm_authchans.end(); it++) {
        Setting &authchan = authchans.add(Setting::TypeGroup);
        authchan.add("name", Setting::TypeString) = it->name;
        authchan.add("psk", Setting::TypeString) = psk_to_string(it->psk);
    }
    if (root.exists("admins")) {
        root.remove("admins");
    }
    Setting &admins = root.add("admins", Setting::TypeList);
    for (vector<struct nvm_admin_entry>::const_iterator it =
             _nvm_admins.begin(); it != _nvm_admins.end(); it++) {
        Setting &admin = admins.add(Setting::TypeGroup);
        admin.add("node", Setting::TypeString) = node_num_to_string(it->node_num);
        admin.add("pubkey", Setting::TypeString) = pubkey_to_string(it->pubkey);
    }
    if(root.exists("mates")) {
        root.remove("mates");
    }
    Setting &mates = root.add("mates", Setting::TypeList);
    for (vector<struct nvm_mate_entry>::const_iterator it =
             _nvm_mates.begin(); it != _nvm_mates.end(); it++) {
        Setting &mate = mates.add(Setting::TypeGroup);
        mate.add("node", Setting::TypeString) = node_num_to_string(it->node_num);
        mate.add("pubkey", Setting::TypeString) = pubkey_to_string(it->pubkey);
    }

    try {
        cfg.writeFile(_path.c_str());
    } catch (const FileIOException &e) {
        result = false;
        goto done;
    }

    result = true;
    _changed = true;

done:

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
