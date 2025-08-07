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

/*
 * Suitable for use on resource-constraint MCU platforms.
 */
class HomeChat {

public:

    HomeChat(shared_ptr<SimpleClient> client = NULL);
    ~HomeChat();

    void setClient(shared_ptr<SimpleClient> client);

    virtual bool handleTextMessage(const meshtastic_MeshPacket &packet,
                                   const string &message);

protected:

    virtual bool isAuthorized(uint32_t node_num) const;
    virtual void setLastMessageFrom(uint32_t node_num, const string &message);
    virtual string getLastMessageFrom(uint32_t node_num) const;

    virtual string handleUptime(uint32_t node_num, const string &message);
    virtual string handleZeroHops(uint32_t node_num, const string &message);
    virtual string handleNodes(uint32_t node_num, const string &message);
    virtual string handleMeshStats(uint32_t node_num, const string &message);
    virtual string handleStatus(uint32_t node_num, const string &message);

    virtual int printf(const char *format, ...) const;
    virtual int vprintf(const char *format, va_list ap) const;

protected:

    shared_ptr<SimpleClient> _client;

    time_t _since;
    map<uint32_t, string> _lastMessageFrom;
    vector<uint32_t> _admins;
    vector<uint32_t> _mates;

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
