/*
 * SimpleShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <SimpleShell.hxx>

SimpleShell::SimpleShell(shared_ptr<SimpleClient> client)
{
    setClient(client);
    _nvm = NULL;
    _noEcho = false;
    _ctx = NULL;
    _inproc.i = 0;
    _since = time(NULL);
    _help_list.push_back("help");
    _help_list.push_back("version");
    _help_list.push_back("system");
    _help_list.push_back("reboot");
    _help_list.push_back("status");
    _help_list.push_back("wcfg");
    _help_list.push_back("disc");
    _help_list.push_back("hb");
    _help_list.push_back("dm");
    _help_list.push_back("cm");
    _help_list.push_back("authchan");
    _help_list.push_back("admin");
    _help_list.push_back("mate");
    _help_list.push_back("nvm");
}

SimpleShell::~SimpleShell()
{

}

void SimpleShell::setClient(shared_ptr<SimpleClient> client)
{
    _client = client;
}

void SimpleShell::setNvm(shared_ptr<BaseNvm> nvm)
{
    _nvm = nvm;
}

int SimpleShell::process(void)
{
    int ret = 0;
    int rx;
    char c;

    while (this->rx_ready() > 0) {
        rx = this->rx_read((uint8_t *) &c, 1);
        if (rx < 0) {
            ret = rx;
            break;
        } else if (rx == 0) {
            break;
        }

        ret += rx;

        if (c == 0xff) {  // IAC received
            static const uint8_t iac_do_tm[3] = { 0xff, 0xfd, 0x06};
            static const uint8_t iac_will_tm[3] = { 0xff, 0xfb, 0x06};
            char iac2;

            ret = this->rx_read((uint8_t *) &iac2, 1);
            if (ret == 1) {
                switch (iac2) {
                case 0xf4:  // IAC IP (interrupt process)
                    ret = this->tx_write(iac_do_tm, sizeof(iac_do_tm));
                    ret = this->tx_write(iac_will_tm, sizeof(iac_will_tm));
                    this->printf("\n> ");
                    _inproc.i = 0;
                    break;
                default:
                    break;
                }
            }

            continue;
        }

        if (c == '\r') {
            _inproc.cmdline[_inproc.i] = '\0';
            if (!_noEcho) {
                this->printf("\n");
            }
            this->exec(_inproc.cmdline);
            this->printf("> ");
            _inproc.i = 0;
            _inproc.cmdline[0] = '\0';
        } else if ((c == '\x7f') || (c == '\x08')) {
            if (_inproc.i > 0) {
                this->printf("\b \b");
                _inproc.i--;
            }
        } else if (c == '\x03') {
            this->printf("^C\n> ");
            _inproc.i = 0;
        } else if ((c != '\n') && isprint(c)) {
            if (_inproc.i < (CMDLINE_SIZE - 1)) {
                if (!_noEcho) {
                    this->printf("%c", c);
                }
                _inproc.cmdline[_inproc.i] = c;
                _inproc.i++;
            }
        }
    }

    return ret;
}

int SimpleShell::tx_write(const uint8_t *buf, size_t size)
{
    (void)(buf);
    (void)(size);
    return 0;
}

int SimpleShell::printf(const char *format, ...)
{
    (void)(format);
    return 0;
}

int SimpleShell::rx_ready(void) const
{
    return 0;
}

int SimpleShell::rx_read(uint8_t *buf, size_t size)
{
    (void)(buf);
    (void)(size);

    return 0;
}

int SimpleShell::exec(char *cmdline)
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

    if (strcmp(argv[0], "help") == 0) {
        ret = this->help(argc, argv);
    } else if (strcmp(argv[0], "version") == 0) {
        ret = this->version(argc, argv);
    } else if (strcmp(argv[0], "system") == 0) {
        ret = this->system(argc, argv);
    } else if (strcmp(argv[0], "reboot") == 0) {
        ret = this->reboot(argc, argv);
    } else if (strcmp(argv[0], "status") == 0) {
        ret = this->status(argc, argv);
    } else if (strcmp(argv[0], "wcfg") == 0) {
        ret = this->wcfg(argc, argv);
    } else if (strcmp(argv[0], "disc") == 0) {
        ret = this->disc(argc, argv);
    } else if (strcmp(argv[0], "hb") == 0) {
        ret = this->hb(argc, argv);
    } else if (strcmp(argv[0], "zerohops") == 0) {
        ret = this->zerohops(argc, argv);
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

int SimpleShell::help(int argc, char **argv)
{
    int ret = 0;
    unsigned int i;

    (void)(argc);
    (void)(argv);

    this->printf("Available commands:\n");

    i = 0;
    for (vector<string>::const_iterator it = _help_list.begin();
         it != _help_list.end(); it++, i++) {
        if ((i % 4) == 0) {
            this->printf("\t");
        }

        this->printf("%s\t", it->c_str());

        if ((i % 4) == 3) {
            this->printf("\n");
        }
    }

    if ((i % 4) != 0) {
        this->printf("\n");
    }

    return ret;
}

int SimpleShell::version(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    this->printf("%s\n", _banner.c_str());
    this->printf("%s\n", _version.c_str());
    this->printf("%s\n", _built.c_str());
    this->printf("-------------------------------------------\n");
    this->printf("%s\n", _copyright.c_str());

    return ret;
}

int SimpleShell::system(int argc, char **argv)
{
    int ret = 0;
    time_t now;
    unsigned int uptime, days, hour, min, sec;

    (void)(argc);
    (void)(argv);

    now = time(NULL);
    uptime = now - _since;
    sec = (uptime % 60);
    min = (uptime / 60) % 60;
    hour = (uptime / 3600) % 24;
    days = (uptime) / 86400;
    if (days == 0) {
        this->printf("   Up-time: %.2u:%.2u:%.2u\n", hour, min, sec);
    } else {
        this->printf("   Up-time: %ud %.2u:%.2u:%.2u\n", days, hour, min, sec);
    }

    return ret;
}

int SimpleShell::reboot(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);
    this->printf("not implemented\n");

    return ret;
}

int SimpleShell::status(int argc, char **argv)
{
    int ret = 0;
    unsigned int i;
    map<uint32_t, meshtastic_DeviceMetrics>::const_iterator dev;
    map<uint32_t, meshtastic_EnvironmentMetrics>::const_iterator env;

    (void)(argc);
    (void)(argv);

    if (!_client->isConnected()) {
        this->printf("Not connected\n");
        goto done;
    }

    this->printf("Me: %s %s\n",
                 _client->getDisplayName(_client->whoami(), true).c_str(),
                 _client->lookupLongName(_client->whoami(), true).c_str());

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
                     _client->getDisplayName(it->second.num, true).c_str());
        if ((i % 4) == 3) {
            this->printf("\n");
        }
    }
    if ((i % 4) != 0) {
        this->printf("\n");
    }

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

    this->printf("mesh bytes (rx/tx): %u/%u\n",
                 _client->meshDeviceBytesReceived(),
                 _client->meshDeviceBytesSent());
    this->printf("mesh packets (rx/tx): %u/%u\n",
                 _client->meshDevicePacketsReceived(),
                 _client->meshDevicePacketsSent());
    this->printf("last mesh packet: %us ago\n",
                 _client->meshDeviceLastRecivedSecondsAgo());

done:

    return ret;
}

int SimpleShell::wcfg(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (_client->sendWantConfig() != true) {
        this->printf("failed!\n");
        ret = -1;
    }

    return ret;
}

int SimpleShell::disc(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (_client->sendDisconnect() != true) {
        this->printf("failed!\n");
        ret = -1;
    }

    return ret;
}

int SimpleShell::hb(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (_client->sendHeartbeat() != true) {
        this->printf("failed!\n");
        ret = -1;
    }

    return ret;
}

int SimpleShell::zerohops(int argc, char **argv)
{
   int ret = 0;
   map<uint32_t, meshtastic_NodeInfo>::const_iterator it;

    (void)(argc);
    (void)(argv);

    this->printf("my zero-hop neighbors:\n");
    for (it = _client->nodeInfos().begin();
         it != _client->nodeInfos().end();
         it++) {
        if (it->second.num == _client->whoami()) {
            continue;
        }

        if (!(it->second.has_hops_away) || (it->second.hops_away > 0)) {
            continue;
        }

        this->printf("%s\n",
                     _client->getDisplayName(it->second.num, true).c_str());
    }


    return ret;

}

int SimpleShell::dm(int argc, char **argv)
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

int SimpleShell::cm(int argc, char **argv)
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

int SimpleShell::authchan(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        for (unsigned int i = 0; i < _nvm->nvmAuthchans().size(); i++) {
            this->printf("%s\n", _nvm->nvmAuthchans()[i].name);
        }
    } else if ((argc == 3) && (strcmp(argv[1], "add") == 0)) {
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
    } else if ((argc == 3) && (strcmp(argv[1], "del") == 0)) {
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
    } else if ((argc == 2) && (strcmp(argv[1], "clear") == 0)) {
        _nvm->clearNvmAuthChannels();
        this->printf("ok\n");
    } else if ((argc > 3) && (strcmp(argv[1], "set") == 0)) {
        int i, pass = 0, fail = 0;

        _nvm->clearNvmAuthChannels();
        for (i = 2; i < argc; i++) {
            result = _nvm->addNvmAuthChannel(argv[i], *_client);
            if (result) {
                this->printf("%s - pass\n", argv[i]);
                pass++;
            } else {
                this->printf("%s - failok\n", argv[i]);
                fail++;
            }
        }
        this->printf("added %u channels, %u failed to add\n", pass, fail);
    } else {
        this->printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int SimpleShell::admin(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        unsigned int i;
        for (i = 0; i < _nvm->nvmAdmins().size(); i++) {
            uint32_t node_num = _nvm->nvmAdmins()[i].node_num;
            this->printf("%16s ",
                         _client->getDisplayName(node_num, true).c_str());
            if ((i % 4) == 3) {
                this->printf("\n");
            }
        }
        if ((i % 4) != 0) {
            this->printf("\n");
        }
    } else if ((argc == 3) && (strcmp(argv[1], "add") == 0)) {
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
    } else if ((argc == 3) && (strcmp(argv[1], "del") == 0)) {
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
    } else if ((argc == 2) && (strcmp(argv[1], "clear") == 0)) {
        _nvm->clearNvmAdmins();
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        this->printf("ok\n");
    } else if ((argc > 3) && (strcmp(argv[1], "set") == 0)) {
        int i, pass = 0, fail = 0;

        _nvm->clearNvmAdmins();
        for (i = 2; i < argc; i++) {
            result = _nvm->addNvmAdmin(argv[i], *_client);
            if (result) {
                this->printf("%s - pass\n", argv[i]);
                pass++;
            } else {
                this->printf("%s - failok\n", argv[i]);
                fail++;
            }
        }
        this->printf("added %u admins, %u failed to add\n", pass, fail);
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
    } else {
        this->printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int SimpleShell::mate(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        unsigned int i;
        for (i = 0; i < _nvm->nvmMates().size(); i++) {
            uint32_t node_num = _nvm->nvmMates()[i].node_num;
            this->printf("%16s ",
                         _client->getDisplayName(node_num, true).c_str());
            if ((i % 4) == 3) {
                this->printf("\n");
            }
        }
        if ((i % 4) != 0) {
            this->printf("\n");
        }
    } else if ((argc == 3) && (strcmp(argv[1], "add") == 0)) {
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
    } else if ((argc == 3) && (strcmp(argv[1], "del") == 0)) {
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
    } else if ((argc == 2) && (strcmp(argv[1], "clear") == 0)) {
        _nvm->clearNvmMates();
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        this->printf("ok\n");
    } else if ((argc > 3) && (strcmp(argv[1], "set") == 0)) {
        int i, pass = 0, fail = 0;

        _nvm->clearNvmMates();
        for (i = 2; i < argc; i++) {
            result = _nvm->addNvmMate(argv[i], *_client);
            if (result) {
                this->printf("%s - pass\n", argv[i]);
                pass++;
            } else {
                this->printf("%s - failok\n", argv[i]);
                fail++;
            }
        }
        this->printf("added %u mates, %u failed to add\n", pass, fail);
        result = _nvm->saveNvm();
        if (result == false) {
            this->printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
    } else {
        this->printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

int SimpleShell::nvm(int argc, char **argv)
{
    if (argc != 1) {
        this->printf("syntax error!\n");
        return -1;
    }

    this->printf("authchans:\n");
    authchan(argc, argv);
    this->printf("admins:\n");
    admin(argc, argv);
    this->printf("mates:\n");
    mate(argc, argv);

    return 0;
}

int SimpleShell::unknown_command(int argc, char **argv)
{
    (void)(argc);

    this->printf("Unknown command '%s'!\n", argv[0]);

    return -1;
}

int SimpleShell::ctx_vprintf(void *ctx, const char *format, va_list ap)
{
    SimpleShell *ss = (SimpleShell *) ctx;

    if (ss == NULL) {
        return -1;
    }

    return ss->vprintf(format, ap);
}

int SimpleShell::vprintf(const char *format, va_list ap)
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
        ret = tx_write((const uint8_t *) pbuf, len);
        if (ret == -1) {
            break;
        }

        len -= (size_t) ret;
    }

done:

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
