//
// Created by Made on 07/05/2025.
//

#include "serrno.h"
#include <stdio.h>
#include <stdlib.h>

void synthPanic(synthErrno pCode)
{
    printf("Synth panic %x\n", pCode);
    exit(pCode);
}