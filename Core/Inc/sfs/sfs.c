//
// Created by Made on 23/03/2025.
//

#include "sfs/sfs.h"

#include <serrno.h>
#include <stdlib.h>

u8*        gSfsFirstBlockData = NULL;
sfsHeader* gSfsHeader         = NULL;

synthErrno sfsInit()
{
    gSfsFirstBlockData = malloc(BLOCK_SIZE);
    synthErrno ret     = sfsReadBlocks(gSfsFirstBlockData, 0, 1);
    if (ret != SERR_OK)
    {
        return ret;
    }
    gSfsHeader = (sfsHeader*)gSfsFirstBlockData;

    return SERR_OK;
}

synthErrno sfsDeinit()
{
    free(gSfsFirstBlockData);

    return SERR_OK;
}
