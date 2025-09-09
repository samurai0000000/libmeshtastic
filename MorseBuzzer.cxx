/*
 * MorseBuzzer.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <MorseBuzzer.hxx>

MorseBuzzer::MorseBuzzer()
    : _isRunning(false),
      _unitTimeMs(75)
{

}

MorseBuzzer::~MorseBuzzer()
{
    stopMorseThread();
}

void MorseBuzzer::clearMorseText(void)
{
    _text = string();
}

void MorseBuzzer::addMorseText(const string &text)
{
    _text += text;
}

void MorseBuzzer::runMorseThread(void)
{
    char c, p = '\0';

    if (_isRunning) {
        goto done;
    }

    _isRunning = true;
    while (_isRunning) {
        if (_text.empty()) {
            sleepForMs(1000);
            continue;
        }

        c = _text[0];
        _text.erase(0, 1);

        if (p != '\0') {
            if (isspace(p)) {
                interWord();
            } else {
                interLetter();
            }
        }

        switch (c) {
        case 'a':
        case 'A': dot(); dash(false); break;
        case 'b':
        case 'B': dash(); dot(); dot(); dot(false); break;
        case 'c':
        case 'C': dash(); dot(); dash(); dot(false); break;
        case 'd':
        case 'D': dash(); dot(); dot(false); break;
        case 'e':
        case 'E': dot(false); break;
        case 'f':
        case 'F': dot(); dot(); dash(); dot(false); break;
        case 'g':
        case 'G': dash(); dash(); dot(false); break;
        case 'h':
        case 'H': dot(); dot(); dot(); dot(false); break;
        case 'i':
        case 'I': dot(); dot(false); break;
        case 'j':
        case 'J': dot(); dash(); dash(); dash(false); break;
        case 'k':
        case 'K': dash(); dot(); dash(false); break;
        case 'l':
        case 'L': dot(); dash(); dot(); dot(false); break;
        case 'm':
        case 'M': dash(); dash(false); break;
        case 'n':
        case 'N': dash(); dot(); break;
        case 'o':
        case 'O': dash(); dash(); dash(false); break;
        case 'p':
        case 'P': dot(); dash(); dash(); dot(false); break;
        case 'q':
        case 'Q': dash(); dash(); dot(); dash(false); break;
        case 'r':
        case 'R': dot(); dash(); dot(false); break;
        case 's':
        case 'S': dot(); dot(); dot(false); break;
        case 't':
        case 'T': dash(false); break;
        case 'u':
        case 'U': dot(); dot(); dash(false); break;
        case 'v':
        case 'V': dot(); dot(); dot(); dash(false); break;
        case 'w':
        case 'W': dot(); dash(); dash(false); break;
        case 'x':
        case 'X': dash(); dot(); dot(); dash(false); break;
        case 'y':
        case 'Y': dash(); dot(); dash(); dash(false); break;
        case 'z':
        case 'Z': dash(); dash(); dot(); dot(false); break;
        case '1': dot(); dash(); dash(); dash(false); break;
        case '2': dot(); dot(); dash(); dash(); dash(false); break;
        case '3': dot(); dot(); dot(); dash(); dash(false); break;
        case '4': dot(); dot(); dot(); dot(); dash(false); break;
        case '5': dot(); dot(); dot(); dot(); dot(false); break;
        case '6': dash(); dot(); dot(); dot(); dot(false); break;
        case '7': dash(); dash(); dot(); dot(); dot(false); break;
        case '8': dash(); dash(); dash(); dot(); dot(false); break;
        case '9': dash(); dash(); dash(); dash(); dot(false); break;
        case '0': dash(); dash(); dash(); dash(); dash(false); break;
        default:  interWord(); break;
        }

        p = c;
    }

done:

    return;
}

void MorseBuzzer::stopMorseThread(void)
{
    _isRunning = false;
}

void MorseBuzzer::dot(bool intraC)
{
    toggleBuzzer(true);
    sleepForMs(_unitTimeMs);
    toggleBuzzer(false);
    if (intraC) {
        intraChar();
    }
}

void MorseBuzzer::dash(bool intraC)
{
    toggleBuzzer(true);
    sleepForMs(3 * _unitTimeMs);
    toggleBuzzer(false);
    if (intraC) {
        intraChar();
    }
}

void MorseBuzzer::intraChar(void)
{
    sleepForMs(_unitTimeMs);
}

void MorseBuzzer::interLetter(void)
{
    sleepForMs(3 * _unitTimeMs);
}

void MorseBuzzer::interWord(void)
{
    sleepForMs(7 * _unitTimeMs);
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
