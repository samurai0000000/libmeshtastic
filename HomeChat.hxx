/*
 * HomeChat.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef HOMECHAT_HXX
#define HOMECHAT_HXX

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <SimpleClient.hxx>

using namespace std;

struct vprintf_callback {
    void *ctx;
    int (*vprintf)(void *ctx, const char *format, va_list ap);
};

class HomeChat {

public:

    HomeChat(shared_ptr<SimpleClient> client = NULL);
    ~HomeChat();

    virtual void setClient(shared_ptr<SimpleClient> client);

    void addPrintfCallback(const struct vprintf_callback &cb);
    void delPrintfCallback(const struct vprintf_callback &cb);

    virtual void clearAuthchansAdminsMates(void);
    virtual bool addAuthChannel(const string &channel,
                                const meshtastic_ChannelSettings_psk_t &psk,
                                bool ignoreDup = true);
    virtual bool addAdmin(uint32_t node_num,
                          const meshtastic_User_public_key_t &pubkey,
                          bool ignoreDup = true);
    virtual bool addMate(uint32_t node_num,
                         const meshtastic_User_public_key_t &pubkey,
                         bool ignoreDup = true);

    bool isAuthChannel(const string &channel) const;
    const map<uint32_t, meshtastic_User_public_key_t> &admins(void) const;
    const map<uint32_t, meshtastic_User_public_key_t> &mates(void) const;

    virtual bool handleTextMessage(const meshtastic_MeshPacket &packet,
                                   const string &message);

protected:

    virtual void getAuthority(uint32_t node_num,
                              bool &isAdmin, bool &isMate) const;
    virtual void setLastMessageFrom(uint32_t node_num, const string &message);
    virtual string getLastMessageFrom(uint32_t node_num) const;

    virtual string handleRollcall(uint32_t node_num, const string &message);
    virtual string handleMeshAuth(uint32_t node_num, const string &message);
    virtual string handleUptime(uint32_t node_num, const string &message);
    virtual string handleZeroHops(uint32_t node_num, const string &message);
    virtual string handleNodes(uint32_t node_num, const string &message);
    virtual string handleMeshStats(uint32_t node_num, const string &message);
    virtual string handleAuthchans(uint32_t node_num, const string &message);
    virtual string handleAdmins(uint32_t node_num, const string &message);
    virtual string handleMates(uint32_t node_num, const string &message);
    virtual string handleStatus(uint32_t node_num, const string &message);
    virtual string handleUnknown(uint32_t node_num, const string &message,
                                 bool isAdmin, bool isMate);

    virtual int printf(const char *format, ...) const;
    virtual int vprintf(const char *format, va_list ap) const;

protected:

    shared_ptr<SimpleClient> _client;

    time_t _since;
    map<uint32_t, string> _lastMessageFrom;
    map<string, meshtastic_ChannelSettings_psk_t> _authchans;
    map<uint32_t, meshtastic_User_public_key_t> _admins;
    map<uint32_t, meshtastic_User_public_key_t> _mates;

    vector<struct vprintf_callback> _vpfcb;

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
