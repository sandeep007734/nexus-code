#include <iostream>
#include <iomanip>
#include <cstring>

#include "utils.h"

using namespace std;

// http://stackoverflow.com/questions/16804251/how-would-i-create-a-hex-dump-utility-in-c
void hexdump(uint8_t * buf, uint32_t len)
{
    unsigned long address = 0;
    char c;

    cout << hex << setfill('0');
    while (len) {
        int nread = len > 16 ? 16 : len;

        // Show the address
        cout << setw(8) << address;

        // Show the hex codes
        for (int i = 0; i < 16; i++) {
            if (i % 8 == 0)
                cout << ' ';
            if (i < nread)
                cout << ' ' << setw(2) << (unsigned)buf[i];
            else
                cout << "   ";
        }

        // Show printable characters
        cout << "  ";
        for (int i = 0; i < nread; i++) {
            if (buf[i] < 32)
                cout << '.';
            else
                cout << buf[i];
        }

        cout << "\n";
        address += 16;

        buf += nread;
        len -= nread;
    }
}

char * do_get_fname(const char * fpath)
{
    size_t i = 0;
    char * rv = nullptr;
    const char * result = fpath + strlen(fpath);

    while (*result != '/' && result != fpath) {
        i++;
        result--;
    }

    if (result != fpath) {
        rv = (char *)operator new(i + 1);
        memcpy(rv, result + 1, i);
        rv[i] = '\0';
    }
    return rv;
}
