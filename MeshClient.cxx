/*
 * MeshClient.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <stdio.h>
#include <MeshClient.hxx>

#define DEFAULT_HEARTBEAT_SECONDS 30

MeshClient::MeshClient()
    : SimpleClient()
{
    _logStderr = false;
    _heartbeatSeconds = DEFAULT_HEARTBEAT_SECONDS;
    _mtc.logger = logEvent;
    _thread = NULL;
    _isRunning = false;
}

MeshClient::~MeshClient()
{
    stop();
}

bool MeshClient::attachSerial(string device)
{
    bool result = false;

    if (mt_serial_attach(&_mtc, device.c_str()) != 0) {
        goto done;
    }

    _isRunning = true;
    _thread = make_shared<thread>(thread_function, this);

    result = true;

done:

    return result;
}

void MeshClient::detach(void)
{
    stop();
}

void MeshClient::join(void)
{
    if (_thread != NULL) {
        if (_thread->joinable()) {
            _thread->join();
        }
    }
}

bool MeshClient::logStderr(void) const
{
    return _logStderr;
}

void MeshClient::enableLogStderr(bool enable)
{
    _logStderr = enable;
}

unsigned int MeshClient::heartbeatSeconds(void) const
{
    return _heartbeatSeconds;
}

void MeshClient::setHeartbeatSeconds(unsigned int seconds)
{
    _heartbeatSeconds = seconds;
}

void MeshClient::logEvent(struct mt_client *mtc, const char *msg, size_t size)
{
    MeshClient *client = (MeshClient *) mtc->ctx;
    if (client->_logStderr) {
        fwrite(msg, 1, size, stderr);
        fflush(stderr);
    }
}

void MeshClient::stop(void)
{
    _isRunning = false;
}

void MeshClient::thread_function(MeshClient *mtc)
{
    mtc->run();
}

void MeshClient::run(void)
{
    int ret = 0;
    uint32_t timeout_ms = 1000;
    time_t last_heartbeat, last_want_config, now;
    time_t currentTime;
    tm *localTime;
    int last_min = -1;

    now = time(NULL);
    last_heartbeat = now;
    last_want_config = 0;

    sendDisconnect();

    while (_isRunning) {
        now = time(NULL);
        if (!isConnected() && ((now - last_want_config) >= 5)) {
            ret = sendWantConfig();
            if (ret != true) {
                _isRunning = false;
                continue;
            }

            last_want_config = now;
        } else if (isConnected()) {
            last_want_config = now;
        }

        if (_heartbeatSeconds > 0) {
            if (isConnected() &&
                ((now - last_heartbeat) >= (time_t) _heartbeatSeconds)) {
                if (sendHeartbeat() != true) {
                    _isRunning = false;
                    break;
                }

                last_heartbeat = now;
            }
        }

        do {
            ret = mt_serial_process(&_mtc, timeout_ms);
            if (ret != 0) {
                _isRunning = false;
                continue;
            }
        } while (ret > 0);

        currentTime = time(NULL);
        localTime = localtime(&currentTime);
        if (last_min != localTime->tm_min) {
            last_min = localTime->tm_min;
            crontab(localTime);
        }

        loop();
    }

    sendDisconnect();
    mt_serial_detach(&_mtc);

    return;
}

void MeshClient::crontab(const struct tm *now)
{
    (void)(now);
    houseKeeping();
}

void MeshClient::loop(void)
{

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
