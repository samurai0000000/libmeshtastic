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
#include <BaseNvm.hxx>

using namespace std;

extern void toLowercase(string &s);
extern void trimWhitespace(string &s);

struct vprintf_callback {
    void *ctx;
    int (*vprintf)(void *ctx, const char *format, va_list ap);
};

class HomeChat {

public:

    HomeChat(shared_ptr<SimpleClient> client = NULL);
    ~HomeChat();

    virtual void setClient(shared_ptr<SimpleClient> client);
    virtual void setNvm(shared_ptr<BaseNvm> nvm);

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

    virtual string handleRollcall(uint32_t node_num, string &message);
    virtual string handleUptime(uint32_t node_num, string &message);
    virtual string handleZeroHops(uint32_t node_num, string &message);
    virtual string handleNodes(uint32_t node_num, string &message);
    virtual string handleMeshStats(uint32_t node_num, string &message);
    virtual string handleAuthchan(uint32_t node_num, string &message);
    virtual string handleAdmin(uint32_t node_num, string &message);
    virtual string handleMate(uint32_t node_num, string &message);
    virtual string handleEnv(uint32_t node_num, string &message);
    virtual string handleStatus(uint32_t node_num, string &message);
    virtual string handleWcfg(uint32_t node_num, string &message);
    virtual string handleUnknown(uint32_t node_num, string &message);

    virtual int printf(const char *format, ...) const;
    virtual int vprintf(const char *format, va_list ap) const;

protected:

    shared_ptr<SimpleClient> _client;
    shared_ptr<BaseNvm> _nvm;

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
