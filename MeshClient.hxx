/*
 * MeshClient.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHCLIENT_HXX
#define MESHCLIENT_HXX

#include <string>
#include <thread>
#include <memory>
#include <libmeshtastic.h>
#include <SimpleClient.hxx>

using namespace std;

class HomeChat;

/*
 * Suitable for use on a full system with OS (x86, aarch64, etc.)
 */
class MeshClient : public SimpleClient {

public:

    MeshClient();
    ~MeshClient();

    bool attachSerial(string device);
    void detach(void);
    void join(void);

    bool logStderr(void) const;
    void enableLogStderr(bool enable);

    unsigned int heartbeatSeconds(void) const;
    void setHeartbeatSeconds(unsigned int seconds);

    inline virtual HomeChat *getHomeChat(void) {
        return NULL;
    }

protected:

    static void logEvent(struct mt_client *, const char *, size_t);

    virtual void crontab(const struct tm *now);
    virtual void loop(void);

private:

    void stop(void);
    static void thread_function(MeshClient *mtc);
    void run(void);

private:

    bool _logStderr;
    unsigned int _heartbeatSeconds;

    shared_ptr<thread> _thread;
    bool _isRunning;

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
