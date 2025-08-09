/*
 * BaseNVM.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef BASENVM_HXX
#define BASENVM_HXX

#include <string>
#include <vector>
#include <libmeshtastic.h>
#include <SimpleClient.hxx>

using namespace std;

struct nvm_authchan_entry {
    char name[12];
    meshtastic_ChannelSettings_psk_t psk;
} __attribute__((packed));

struct nvm_admin_entry {
    uint32_t node_num;
    meshtastic_User_public_key_t pubkey;
} __attribute__((packed));

struct nvm_mate_entry {
    uint32_t node_num;
    meshtastic_User_public_key_t pubkey;
} __attribute__((packed));

class BaseNVM {

public:

    BaseNVM();
    ~BaseNVM();

    virtual bool loadNvm(void) = 0;
    virtual bool saveNvm(void) = 0;

    inline const vector<struct nvm_authchan_entry> &nvmAuthchans(void) {
        return _nvm_authchans;
    }

    inline const vector<struct nvm_admin_entry> &nvmAdmins(void) {
        return _nvm_admins;
    }

    inline const vector<struct nvm_mate_entry> &nvmMates(void) {
        return _nvm_mates;
    }

    bool addNvmAuthChannel(const string &channel,
                           meshtastic_ChannelSettings_psk_t psk,
                           bool ignoreDup = true);
    bool addNvmAuthChannel(const string &channel, const SimpleClient &client);
    bool delNvmAuthChannel(const string &channel);
    bool addNvmAdmin(uint32_t node_num,
                     meshtastic_User_public_key_t pubkey,
                     bool ignoreDup = true);
    bool addNvmAdmin(const string &name, const SimpleClient &client);
    bool delNvmAdmin(uint32_t node_num);
    bool delNvmAdmin(const string &name, const SimpleClient &client);
    bool addNvmMate(uint32_t node_num,
                    meshtastic_User_public_key_t pubkey,
                    bool ignoreDup = true);
    bool addNvmMate(const string &name, const SimpleClient &client);
    bool delNvmMate(uint32_t node_num);
    bool delNvmMate(const string &name, const SimpleClient &client);

protected:

    vector<struct nvm_authchan_entry> _nvm_authchans;
    vector<struct nvm_admin_entry> _nvm_admins;
    vector<struct nvm_mate_entry> _nvm_mates;

};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
