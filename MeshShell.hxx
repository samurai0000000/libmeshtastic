/*
 * MeshShell.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHSHELL_HXX
#define MESHSHELL_HXX

#include <termios.h>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <libmeshtastic.h>
#include <MeshClient.hxx>
#include <MeshNVM.hxx>

using namespace std;

class MeshShell {

public:

    MeshShell(shared_ptr<MeshClient> client = NULL);
    ~MeshShell();

    virtual void setClient(shared_ptr<MeshClient> client);
    virtual void setNVM(shared_ptr<MeshNVM> nvm);

    void setPassword(const string &password);
    bool attachStdio(void);
    bool attachFd(int fd);
    bool bindPort(uint16_t port = 16876);
    void detach(void);
    void join(void);

protected:

    shared_ptr<MeshClient> _client;
    shared_ptr<MeshNVM> _nvm;

    virtual int printf(const char *format, ...);
    virtual int exec(char *cmdline);
    virtual int exit(int argc, char **argv);
    virtual int help(int argc, char **argv);
    virtual int version(int argc, char **argv);
    virtual int system(int argc, char **argv);
    virtual int reboot(int argc, char **argv);
    virtual int status(int argc, char **argv);
    virtual int dm(int argc, char **argv);
    virtual int cm(int argc, char **argv);
    virtual int authchan(int argc, char **argv);
    virtual int admin(int argc, char **argv);
    virtual int mate(int argc, char **argv);
    virtual int nvm(int argc, char **argv);
    virtual int unknown_command(int argc, char **argv);

private:

    void stop(void);
    static void thread_function(MeshShell *ms);
    void run(void);

private:

    shared_ptr<thread> _thread;
    mutex _mutex;
    bool _isRunning;

    int _fd;
    int _outfd;
    struct termios _oldtty;
    uint16_t _port;
    string _password;

    vector<shared_ptr<MeshShell>> _children;

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
