
#ifndef AKMD_INC_FILTER_COMMON_H
#define AKMD_INC_FILTER_COMMON_H

#include <math.h>
#include "AKCommon.h"

typedef enum _Axis
{
    x = 0,
    y = 1,
    z = 2
} Axis;

float CalcAveFloat(const float data[], const int size);
float CalcStdFloat(const float data[], const int size);
void SortArrayInt(int data[], const int size);
void SortArrayFloat(float data[], const int size);

#endif //AKMD_INC_FILTER_COMMON_H
