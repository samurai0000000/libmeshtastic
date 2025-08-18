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
#include <BaseNVM.hxx>

using namespace std;

class SimpleShell {

public:

    SimpleShell(shared_ptr<SimpleClient> client = NULL);
    ~SimpleShell();

    inline void setBanner(const string &banner) {
        _banner = banner;
    }
    inline void setVersion(const string &version) {
        _version = version;
    }
    inline void setBuilt(const string &built) {
        _built = built;
    }
    inline void setCopyright(const string &copyright) {
        _copyright = copyright;
    }

    inline const string &banner(void) const {
        return _banner;
    }
    inline const string &version(void) const {
        return _version;
    }
    inline const string &built(void) const {
        return _built;
    }
    inline const string &copyright(void) const {
        return _copyright;
    }

    virtual void setClient(shared_ptr<SimpleClient> client);
    virtual void setNVM(shared_ptr<BaseNVM> nvm);

    inline void setNoEcho(bool noEcho) {
        _noEcho = noEcho;
    }

    inline virtual void attach(void *ctx, bool showWelcome = false) {
        _ctx = ctx;
        if (showWelcome) {
            this->printf("\n\x1b[2K");
            this->printf("%s\n", _banner.c_str());
            this->printf("%s\n", _version.c_str());
            this->printf("%s\n", _built.c_str());
            this->printf("-------------------------------------------\n");
            this->printf("%s\n", _copyright.c_str());
            this->printf("> ");
        }
    }
    inline virtual void detach(void) {
        _ctx = NULL;
    }

    virtual int process(void);

protected:

    shared_ptr<SimpleClient> _client;
    shared_ptr<BaseNVM> _nvm;

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
    virtual int dm(int argc, char **argv);
    virtual int cm(int argc, char **argv);
    virtual int authchan(int argc, char **argv);
    virtual int admin(int argc, char **argv);
    virtual int mate(int argc, char **argv);
    virtual int nvm(int argc, char **argv);
    virtual int unknown_command(int argc, char **argv);

    time_t _since;

    string _banner;
    string _version;
    string _built;
    string _copyright;

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
