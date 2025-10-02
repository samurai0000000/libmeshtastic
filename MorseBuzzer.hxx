/*
 * MorseBuzzer.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MORSEBUZZER_HXX
#define MORSEBUZZER_HXX

#include <string>

using namespace std;

class MorseBuzzer {

public:

    MorseBuzzer();
    ~MorseBuzzer();

    void clearMorseText(void);
    void addMorseText(const string &text);
    inline bool isMorseEmpty(void) const {
        return _text.empty();
    }

    void runMorseThread(void);
    void stopMorseThread(void);

protected:

    virtual void sleepForMs(unsigned int ms) = 0;
    virtual void toggleBuzzer(bool onOff) = 0;

private:

    void dot(bool intraC = true);
    void dash(bool intraC = true);
    void intraChar(void);
    void interLetter(void);
    void interWord(void);

    bool _isRunning;
    unsigned int _unitTimeMs;
    string _text;

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
