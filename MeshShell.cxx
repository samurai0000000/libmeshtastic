/*
 * MeshShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <iostream>
#include <HomeChat.hxx>
#include <MeshShell.hxx>

MeshShell::MeshShell(shared_ptr<MeshClient> client)
{
    _since = time(NULL);
    setClient(client);
    _fd = -1;
    _port = 0;
    _help_list.push_back("exit");
}

MeshShell::~MeshShell()
{
    if (_client != NULL) {
        HomeChat *hc = _client->getHomeChat();
        if (hc) {
            struct vprintf_callback cb = {
                .ctx = this,
                .vprintf = ctx_vprintf,
            };
            hc->delPrintfCallback(cb);
        }
    }
}

void MeshShell::setClient(shared_ptr<MeshClient> client)
{
    SimpleShell::setClient(dynamic_pointer_cast<SimpleClient>(client));
    _client = client;
    if (_client != NULL) {
        HomeChat *hc = _client->getHomeChat();
        if (hc) {
            struct vprintf_callback cb = {
                .ctx = this,
                .vprintf = ctx_vprintf,
            };
            hc->addPrintfCallback(cb);
        }
    }
}

void MeshShell::setNVM(shared_ptr<MeshNVM > nvm)
{
    SimpleShell::setNVM(dynamic_pointer_cast<BaseNVM>(nvm));
    _nvm = nvm;
}

void MeshShell::setPassword(const string &password)
{
    _password = password;
}

bool MeshShell::attachStdio(void)
{
    bool result = false;
    int ret;
    struct termios tty;
    int terminal_fd;

    ret = tcgetattr(STDIN_FILENO, &tty);
    if (ret != 0) {
        fprintf(stderr, "tcgetattr: %s\n", strerror(errno));
        result = false;
        goto done;
    }

    memcpy(&_oldtty, &tty, sizeof(tty));

    tty.c_lflag &= ~(ICANON | ECHO | ISIG);
    ret = tcsetattr(STDIN_FILENO, TCSANOW, &tty);
    if (ret != 0) {
        fprintf(stderr, "tcsetattr: %s\n", strerror(errno));
        result = false;
        goto done;
    }

    terminal_fd = open("/dev/tty", O_RDWR | O_NOCTTY);
    if (terminal_fd == -1) {
        fprintf(stderr, "/dev/tty: %s\n", strerror(errno));
        result = false;
        goto done;
    }

    ret = dup2(terminal_fd, STDIN_FILENO);
    if (ret == -1) {
        fprintf(stderr, "dup2: %s\n", strerror(errno));
        close(terminal_fd);
        result = false;
        goto done;
    }

    close(terminal_fd);
    _fd = STDIN_FILENO;

    _isRunning = true;
    _thread = make_shared<thread>(thread_function, this);
    result = true;

    if (_client != NULL) {
        HomeChat *hc = _client->getHomeChat();
        if (hc) {
            struct vprintf_callback cb = {
                .ctx = this,
                .vprintf = ctx_vprintf,
            };
            hc->delPrintfCallback(cb);
        }
    }

done:

    return result;
}

bool MeshShell::attachFd(int fd)
{
    bool result = false;

    if (_fd != -1) {
        result = false;
        goto done;
    }

    _fd = fd;
    _isRunning = true;
    _thread = make_shared<thread>(thread_function, this);
    result = true;

done:

    return result;
}

bool MeshShell::bindPort(uint16_t port)
{
    bool result = false;
    struct sockaddr_in serveraddr;
    int ret, val;

    if (_fd != -1) {
        result = false;
        goto done;
    }

    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == -1) {
        cerr << "socket: " << strerror(errno) << endl;
        result = false;
        goto done;
    }

    val = 1;
    setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    ret = bind(_fd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
    if (ret == -1) {
        cerr << "bind: " << strerror(errno) << endl;
        result = false;
        close(_fd);
        _fd = -1;
        goto done;
    }

    ret = listen(_fd, 5);
    if (ret == -1) {
        cerr << "listen: " << strerror(errno) << endl;
        result = false;
        close(_fd);
        _fd = -1;
        goto done;
    }

    cout << "Listening to port " << port << endl;

    _port = port;
    _isRunning = true;
    _thread = make_shared<thread>(thread_function, this);
    result = true;

done:

    return result;
}

void MeshShell::detach(void)
{
    int ret;

    stop();

    if (_fd == STDIN_FILENO) {
        ret = tcsetattr(STDIN_FILENO, TCSANOW, &_oldtty);
        if (ret != 0) {
            fprintf(stderr, "tcsetattr: %s\n", strerror(errno));
        }
    }
}

void MeshShell::join(void)
{
    if (_thread != NULL) {
        if (_thread->joinable()) {
            _thread->join();
        }
    }

    for (vector<shared_ptr<MeshShell>>::iterator it = _children.begin();
         it != _children.end(); it++) {
        (*it)->join();
    }
}

void MeshShell::stop(void)
{
    _isRunning = false;
}

void MeshShell::thread_function(MeshShell *ms)
{
    ms->run();
}

void MeshShell::run(void)
{
    bool binder = false;
    bool netClient = false;
    bool nvmSet = false;
    struct sockaddr_in in_addr;
    socklen_t len;
    char cmdline[256];
    unsigned int i = 0;

    if (_port != 0) {
        binder = true;
    } else if (_fd != STDIN_FILENO) {
        netClient = true;
    }

    if (netClient) {
#if 0
        static const uint8_t suppress_local_echo[3] = { 0xff, 0xfb, 0x01, };
        write(_fd, suppress_local_echo, sizeof(suppress_local_echo));
#endif
    }

    if (!binder) {
        this->printf("%s\n", _banner.c_str());
        this->printf("%s\n", _version.c_str());
        this->printf("----------------------------------------------------\n");
        this->printf("%s\n", _copyright.c_str());
        this->printf("> ");
    }

    while (_isRunning) {
        int ret;
        fd_set rfds;
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 500000,
        };

        if ((nvmSet == false) && (_client != NULL) && (_nvm != NULL) &&
            _client->isConnected()) {
            if (_nvm->setupFor(_client->whoami()) == true) {
                _nvm->loadNvm();
                nvmSet = true;
            }
        }

        if (nvmSet && _nvm->hasNvmChanged() &&
            (_client->getHomeChat() != NULL)) {
            _client->getHomeChat()->clearAuthchansAdminsMates();
            for (vector<struct nvm_authchan_entry>::const_iterator it =
                     _nvm->nvmAuthchans().begin();
                 it != _nvm->nvmAuthchans().end(); it++) {
                _client->getHomeChat()->addAuthChannel(it->name, it->psk);
            }

            for (vector<struct nvm_admin_entry>::const_iterator it =
                     _nvm->nvmAdmins().begin();
                 it != _nvm->nvmAdmins().end(); it++) {
                _client->getHomeChat()->addAdmin(it->node_num, it->pubkey);
            }
            for (vector<struct nvm_mate_entry>::const_iterator it =
                     _nvm->nvmMates().begin();
                 it != _nvm->nvmMates().end(); it++) {
                _client->getHomeChat()->addMate(it->node_num, it->pubkey);
            }

            _nvm->clearNvmChanged();
        }

        FD_ZERO(&rfds);
        FD_SET(_fd, &rfds);

        ret = select(_fd + 1, &rfds, NULL, NULL, &timeout);

        if (ret == -1) {
            _isRunning = false;
            break;
        } else if (ret == 0) {
            continue;
        }

        if (binder) {
            shared_ptr<MeshShell> clientShell;
            int client_fd;

            len = sizeof(in_addr);
            client_fd = accept(_fd, (struct sockaddr *) &in_addr, &len);
            if (client_fd == -1) {
                continue;
            }

            clientShell = make_shared<MeshShell>();
            clientShell->setClient(_client);
            clientShell->setNVM(_nvm);
            clientShell->setBanner(_banner);
            clientShell->setVersion(_version);
            clientShell->setBuilt(_built);
            clientShell->setCopyright(_copyright);
            clientShell->attachFd(client_fd);

            _children.push_back(clientShell);
        } else {
            char c;

            ret = read(_fd, &c, 1);
            if (ret == -1) {
                _isRunning = false;
                break;  // File descriptor error
            } else if (ret == -1) {
                _isRunning = false;
                break;  // EOF
            }

            if (netClient & (c == 0xff)) {  // IAC received
                static const uint8_t iac_do_tm[3] = { 0xff, 0xfd, 0x06};
                static const uint8_t iac_will_tm[3] = { 0xff, 0xfb, 0x06};
                char iac2;

                ret = read(_fd, &iac2, 1);
                if (ret == 1) {
                    switch (iac2) {
                    case 0xf4:  // IAC IP (interrupt process)
                        ret = write(_fd, iac_do_tm, sizeof(iac_do_tm));
                        ret = write(_fd, iac_will_tm, sizeof(iac_will_tm));
                        this->printf("\n> ");
                        i = 0;
                        break;
                    default:
                        break;
                    }
                }

                continue;
            }

            if (c == '\n') {
                cmdline[i] = '\0';
                if (!netClient) {
                    this->printf("\n");
                }
                ret = exec(cmdline);
                (void)(ret);
                this->printf("> ");
                i = 0;
                cmdline[0] = '\0';
            } else if ((c == '\x7f') || (c == '\x08')) {
                if (i > 0) {
                    this->printf("\b \b");
                    i--;
                }
            } else if ((c == '\x03') && !netClient) {
                this->printf("^C\n> ");
                i = 0;
            } else if ((c != '\n') && isprint(c)) {
                if (i < (sizeof(cmdline) - 1)) {
                    if (!netClient) {
                        this->printf("%c", c);
                    }
                    cmdline[i] = c;
                    i++;
                }
            }
        }
    }

    for (vector<shared_ptr<MeshShell>>::iterator it = _children.begin();
         it != _children.end(); it++) {
        (*it)->detach();
    }
}

int MeshShell::ctx_vprintf(void *ctx, const char *format, va_list ap)
{
    MeshShell *ms = (MeshShell *) ctx;

    if (ms == NULL) {
        return -1;
    }

    return ms->vprintf(format, ap);
}

int MeshShell::vprintf(const char *format, va_list ap)
{
    int ret = 0;
    size_t len = 0;
    char pbuf[1024];

    if (format == NULL) {
        goto done;
    }

    ret = vsnprintf(pbuf, sizeof(pbuf) - 1, format, ap);
    if (ret <= 0) {
        goto done;
    }

    len = (size_t) ret;

    while (len > 0) {
        ret = write(_fd, pbuf, len);
        if (ret == -1) {
            break;
        }

        len -= (size_t) ret;
    }

done:

    return ret;
}

int MeshShell::printf(const char *format, ...)
{
    int ret = 0;
    va_list ap;

    va_start(ap, format);
    ret = this->vprintf(format, ap);
    va_end(ap);

    return ret;
}

int MeshShell::exit(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    if (_fd == STDIN_FILENO) {
        raise(SIGINT);
    } else {
        close(_fd);
    }

    return 0;
}

int MeshShell::unknown_command(int argc, char **argv)
{
    int ret = 0;

    if (strcmp(argv[0], "exit") == 0) {
        ret = this->exit(argc, argv);
    } else {
        this->printf("Unknown command '%s'!\n", argv[0]);
        ret = -1;
    }

    return ret;
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
