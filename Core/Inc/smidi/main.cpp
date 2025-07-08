#include <stdio.h>
#include <fstream>
#include <filesystem>

extern "C" {
#include "solfege/solfege.h"
#include "smidi.h"
}

void callback(int pTrack)
{
    printf("event %d 0x%x [T%d]\n", gSmidiCurrentEvent.event >> 8, gSmidiCurrentEvent.event & 0xFF, pTrack);
}

int main(void)
{
    smidiParseData((u8*)gSmidiTestBuffer, SMIDI_TEST_BUFFER_SIZE, callback);
    return 0;
}
