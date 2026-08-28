/*
 * SimpleShell.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef SIMPLESHELL_HXX
#define SIMPLESHELL_HXX

#include <string>
#include <memory>
#include <libmeshtastic.h>
#include <BaseNvm.hxx>

using namespace std;

#define HOUSEKEEPING_INTERVAL 60

class SimpleShell {

public:

    SimpleShell(shared_ptr<SimpleClient> client = NULL);
    ~SimpleShell();

    virtual void setClient(shared_ptr<SimpleClient> client);
    virtual void setNvm(shared_ptr<BaseNvm> nvm);

    inline void setNoEcho(bool noEcho) {
        _noEcho = noEcho;
    }

    inline virtual void attach(void *ctx) {
        _ctx = ctx;
    }
    inline virtual void detach(void) {
        _ctx = NULL;
    }

    inline void showWelcome(void) {
        this->printf("\n\x1b[2K");
        this->printf("%s\n", _client->banner().c_str());
        this->printf("%s\n", _client->version().c_str());
        if (!_client->firmwareVersion().empty()) {
            this->printf("Meshtastic: %s\n",
                         _client->firmwareVersion().c_str());
        }
        this->printf("%s\n", _client->built().c_str());
        this->printf("-------------------------------------------\n");
        this->printf("%s\n", _client->copyright().c_str());
        this->printf("> ");
    }

    virtual int process(void);

protected:

    shared_ptr<SimpleClient> _client;
    shared_ptr<BaseNvm> _nvm;

    virtual int tx_write(const uint8_t *buf, size_t size);
    virtual int printf(const char *format, ...);
    virtual int rx_ready(void) const;
    virtual int rx_read(uint8_t *buf, size_t size);

    virtual int exec(char *cmdline);
    virtual int help(int argc, char **argv);
    virtual int version(int argc, char **argv);
    virtual int system(int argc, char **argv);
    virtual int reboot(int argc, char **argv);
    virtual int status(int argc, char **argv);
    virtual int wcfg(int argc, char **argv);
    virtual int disc(int argc, char **argv);
    virtual int hb(int argc, char **argv);
    virtual int zerohops(int argc, char **argv);
    virtual int dm(int argc, char **argv);
    virtual int cm(int argc, char **argv);
    virtual int authchan(int argc, char **argv);
    virtual int admin(int argc, char **argv);
    virtual int mate(int argc, char **argv);
    virtual int nvm(int argc, char **argv);
    virtual int last(int argc, char **argv);
    virtual int purge(int argc, char **argv);
    virtual int unknown_command(int argc, char **argv);

    virtual void houseKeeping(void);

    time_t _since;
    time_t _lastHouseKeeping;

    vector<string> _help_list;

protected:

#define CMDLINE_SIZE 256

    void *_ctx;
    bool _noEcho;

    struct inproc {
        char cmdline[CMDLINE_SIZE];
        unsigned int i;
    };

    struct inproc _inproc;

public:

    static int ctx_vprintf(void *ctx, const char *format, va_list ap);

private:

    int vprintf(const char *format, va_list ap);

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
