
#include "filter_common.h"

float CalcAveFloat(const float data[], const int size)
{
    int i = 0;
    float ave = 0.0f;
    float sum = 0.0f;

    for(i=0; i<size; i++)
    {
        sum += data[i];
    }
    ave = sum/size;

    return ave;
}

float CalcStdFloat(const float data[], const int size)
{
    int i = 0;
    float sum = 0.0f;
    float data_ave = 0.0f;
    float result = 0.0f;

    /*ALOGE("%f,%f,%f,%f,%f,%f,%f,%f,"
          "%f,%f,%f,%f,%f,%f,%f,%f\n", 
          data[0], data[1], data[2], data[3],
          data[4], data[5], data[6], data[7],
          data[8], data[9], data[10], data[11],
          data[12], data[13], data[14], data[15]);*/
    
    data_ave = CalcAveFloat(data, size);

    for (i=0; i<size; i++) {
        sum = sum + (data[i]-data_ave)*(data[i]-data_ave);
    }
    result = sqrt(sum/size);

    return result;
}

//bubble sort
void SortArrayInt(int data[], const int num)
{
    int i, j, tmp;

    for (i=0; i<num-1; i++) {
        for (j=0; j<num-1-i; j++) {
            if (data[j] > data[j+1]) {
                tmp = data[j];
                data[j] = data[j+1];
                data[j+1] = tmp;
            }
        }
    }
}

void SortArrayFloat(float data[], const int num)
{
    int i, j;
    float tmp;

    for (i=0; i<num-1; i++) {
        for (j=0; j<num-1-i; j++) {
            if (data[j] > data[j+1]) {
                tmp = data[j];
                data[j] = data[j+1];
                data[j+1] = tmp;
            }
        }
    }
}
