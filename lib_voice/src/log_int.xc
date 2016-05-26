#include "xclib.h"
#include "log_int.h"

static int lookup[32] = {
    0,    11,  22,  33,  43,  53,  63,  73,
    82,   91, 100, 109, 117, 125, 134, 141,
    149, 157, 164, 172, 179, 186, 193, 200,
    206, 213, 219, 225, 232, 238, 244, 250,
};

int log_int(unsigned int x) {
    if (x == 0) {
        return 0;
    }
    int bits = 32 - clz(x);
    if (bits <= 6) {
        return lookup[(x << (6 - bits))-32] + bits * 256;
    } else {
        return lookup[(x >> (bits - 6))-32] + bits * 256;
    }
}
