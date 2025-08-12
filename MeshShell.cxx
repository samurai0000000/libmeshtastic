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
    setClient(client);
    _fd = -1;
    _port = 0;
}

MeshShell::~MeshShell()
{

}

void MeshShell::setClient(shared_ptr<MeshClient> client)
{
    _client = client;
}

void MeshShell::setNVM(shared_ptr<MeshNVM > nvm)
{
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
        result = false;
        close(_fd);
        _fd = -1;
        goto done;
    }

    ret = listen(_fd, 5);
    if (ret == -1) {
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

int MeshShell::printf(const char *format, ...)
{
    int ret = 0;
    va_list ap;
    char pbuf[1024];

    va_start(ap, format);
    ret = vsnprintf(pbuf, sizeof(pbuf) - 1, format, ap);
    va_end(ap);

    ret = write(_fd, pbuf, (size_t) ret);

    return ret;
}

int MeshShell::exec(char *cmdline)
{
    int ret = 0;
    int argc = 0;
    char *argv[32];

    if (cmdline == NULL) {
        ret = -1;
        goto done;
    }

    bzero(argv, sizeof(argv));

    // Tokenize cmdline into argv[]
    while (*cmdline != '\0' && (argc < 32)) {
        while ((*cmdline != '\0') && isspace((int) *cmdline)) {
            cmdline++;
        }

        if (*cmdline == '\0') {
            break;
        }

        argv[argc] = cmdline;
        argc++;

        while ((*cmdline != '\0') && !isspace((int) *cmdline)) {
            cmdline++;
        }

        if (*cmdline == '\0') {
            break;
        }

        *cmdline = '\0';
        cmdline++;
    }

    if (argc < 1) {
        ret = -1;
        goto done;
    }

    if (strcmp(argv[0], "exit") == 0) {
        ret = this->exit(argc, argv);
    } else if (strcmp(argv[0], "help") == 0) {
        ret = this->help(argc, argv);
    } else if (strcmp(argv[0], "version") == 0) {
        ret = this->version(argc, argv);
    } else if (strcmp(argv[0], "system") == 0) {
        ret = this->system(argc, argv);
    } else if (strcmp(argv[0], "reboot") == 0) {
        ret = this->reboot(argc, argv);
    } else if (strcmp(argv[0], "status") == 0) {
        ret = this->status(argc, argv);
    } else if (strcmp(argv[0], "dm") == 0) {
        ret = this->dm(argc, argv);
    } else if (strcmp(argv[0], "cm") == 0) {
        ret = this->cm(argc, argv);
    } else if (strcmp(argv[0], "authchan") == 0) {
        ret = this->authchan(argc, argv);
    } else if (strcmp(argv[0], "admin") == 0) {
        ret = this->admin(argc, argv);
    } else if (strcmp(argv[0], "mate") == 0) {
        ret = this->mate(argc, argv);
    } else if (strcmp(argv[0], "nvm") == 0) {
        ret = this->nvm(argc, argv);
    } else {
        ret = this->unknown_command(argc, argv);
    }

done:

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

int MeshShell::help(int argc, char **argv)
{
    static const char *this_class_commands[] = {
        "help",
        "version",
        "system",
        "reboot",
        "status",
        "dm",
        "cm",
        "authchan",
        "admin",
        "mate",
        "nvm",
        NULL,
    };
    int ret = 0;
    unsigned int i = 0;

    (void)(argc);
    (void)(argv);

    this->printf("Available commands:\n");

    for (i = 0; this_class_commands[i]; i++) {
        if ((i % 4) == 0) {
            this->printf("\t");
        }

        this->printf("%s\t", this_class_commands[i]);

        if ((i % 4) == 3) {
            this->printf("\n");
        }
    }

    if ((i % 4) != 0) {
        this->printf("\n");
    }

    return ret;
}

int MeshShell::version(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);
    this->printf("not implemented\n");

    return ret;
}

int MeshShell::system(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);
    this->printf("not implemented\n");

    return ret;
}

int MeshShell::reboot(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);
    this->printf("not implemented\n");

    return ret;
}

int MeshShell::status(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (!_client->isConnected()) {
        this->printf("Not connected\n");
    } else {
        unsigned int i;

        this->printf("Me: %s %s\n",
                     _client->getDisplayName(_client->whoami()).c_str(),
                     _client->lookupLongName(_client->whoami()).c_str());

        this->printf("Channels: %d\n",
                     _client->channels().size());
        for (map<uint8_t, meshtastic_Channel>::const_iterator it =
                 _client->channels().begin();
             it != _client->channels().end(); it++) {
            if (it->second.has_settings &&
                it->second.role != meshtastic_Channel_Role_DISABLED) {
                this->printf("chan#%u: %s\n",
                             (unsigned int) it->second.index,
                             it->second.settings.name);
            }
        }

        this->printf("Nodes: %d seen\n",
                     _client->nodeInfos().size());
        i = 0;
        for (map<uint32_t, meshtastic_NodeInfo>::const_iterator it =
                 _client->nodeInfos().begin();
             it != _client->nodeInfos().end(); it++, i++) {
            if ((i % 4) == 0) {
                this->printf("  ");
            }
            this->printf("%16s  ",
                         _client->getDisplayName(it->second.num).c_str());
            if ((i % 4) == 3) {
                this->printf("\n");
            }
        }
        if ((i % 4) != 0) {
            this->printf("\n");
        }

        map<uint32_t, meshtastic_DeviceMetrics>::const_iterator dev;
        dev = _client->deviceMetrics().find(_client->whoami());
        if (dev != _client->deviceMetrics().end()) {
            if (dev->second.has_channel_utilization) {
                this->printf("channel_utilization: %.2f\n",
                             dev->second.channel_utilization);
            }
            if (dev->second.has_air_util_tx) {
                this->printf("air_util_tx: %.2f\n",
                             dev->second.air_util_tx);
            }
        }

        map<uint32_t, meshtastic_EnvironmentMetrics>::const_iterator env;
        env = _client->environmentMetrics().find(_client->whoami());
        if (env != _client->environmentMetrics().end()) {
            if (env->second.has_temperature) {
                this->printf("temperature: %.2f\n",
                             env->second.temperature);
            }
            if (env->second.has_relative_humidity) {
                this->printf("relative_humidity: %.2f\n",
                             env->second.relative_humidity);
            }
            if (env->second.has_barometric_pressure) {
                this->printf("barometric_pressure: %.2f\n",
                             env->second.barometric_pressure);
            }
        }
    }

    return ret;
}

int MeshShell::dm(int argc, char **argv)
{
    int ret = 0;
    uint32_t dest = 0xffffffffU;
    string message;

    if (argc < 3) {
        this->printf("Usage: %s [name] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    dest = _client->getId(argv[1]);
    if ((dest == 0xffffffffU) || (dest == _client->whoami())) {
        this->printf("name '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (_client->textMessage(dest, 0x0U, message) != true) {
        this->printf("failed!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int MeshShell::cm(int argc, char **argv)
{
    int ret = 0;
    uint8_t channel = 0xffU;
    string message;

    if (argc < 3) {
        this->printf("Usage: %s [chan] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    channel = _client->getChannel(argv[1]);
    if (channel == 0xffU) {
        this->printf("channel '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (_client->textMessage(0xffffffffU, channel, message) != true) {
        this->printf("failed!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int MeshShell::authchan(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        this->printf("list of authchans:\n");
        for (unsigned int i = 0; i < _nvm->nvmAuthchans().size(); i++) {
            this->printf("  %s\n", _nvm->nvmAuthchans()[i].name);
        }
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        result = _nvm->addNvmAuthChannel(argv[2], *_client);
        if (result == false) {
            this->printf("addNvmAuthChannel failed!\n");
            ret = -1;
            goto done;
        }
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        this->printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        result = _nvm->delNvmAuthChannel(argv[2]);
        if (result == false) {
            this->printf("delNvmAuthChannel failed!\n");
            ret = -1;
            goto done;
        }
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        this->printf("ok\n");
    } else {
        this->printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int MeshShell::admin(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        unsigned int i;
        this->printf("list of admins:\n");
        for (i = 0; i < _nvm->nvmAdmins().size(); i++) {
            uint32_t node_num = _nvm->nvmAdmins()[i].node_num;
            if ((i % 4) == 0) {
                this->printf("  ");
            }
            this->printf("%16s  ",
                         _client->getDisplayName(node_num).c_str());
            if ((i % 4) == 3) {
                this->printf("\n");
            }
        }
        if ((i % 4) != 0) {
            this->printf("\n");
        }
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        result = _nvm->addNvmAdmin(argv[2], *_client);
        if (result == false) {
            this->printf("addNvmAdmin failed!\n");
            ret = -1;
            goto done;
        }
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        this->printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        result = _nvm->delNvmAdmin(argv[2], *_client);
        if (result == false) {
            this->printf("delNvmAdmin failed!\n");
            ret = -1;
            goto done;
        }
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        this->printf("ok\n");
    } else {
        this->printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int MeshShell::mate(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        unsigned int i;
        this->printf("list of mates:\n");
        for (i = 0; i < _nvm->nvmMates().size(); i++) {
            uint32_t node_num = _nvm->nvmMates()[i].node_num;
            if ((i % 4) == 0) {
                this->printf("  ");
            }
            this->printf("%16s  ",
                         _client->getDisplayName(node_num).c_str());
            if ((i % 4) == 3) {
                this->printf("\n");
            }
        }
        if ((i % 4) != 0) {
            this->printf("\n");
        }
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        result = _nvm->addNvmMate(argv[2], *_client);
        if (result == false) {
            this->printf("addNvmMate failed!\n");
            ret = -1;
            goto done;
        }
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        this->printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        result = _nvm->delNvmMate(argv[2], *_client);
        if (result == false) {
            this->printf("delNvmMate failed!\n");
            ret = -1;
            goto done;
        }
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        this->printf("ok\n");
    } else {
        this->printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int MeshShell::nvm(int argc, char **argv)
{
    if (argc != 1) {
        this->printf("syntax error!\n");
        return -1;
    }

    authchan(argc, argv);
    admin(argc, argv);
    mate(argc, argv);

    return 0;
}

int MeshShell::unknown_command(int argc, char **argv)
{
    (void)(argc);

    this->printf("Unknown command '%s'!\n", argv[0]);

    return -1;
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
