#include"stable_checking.h"
#include "FileIO.h"
#include "CustomerSpec.h"

#define true    1
#define false   0

#define STDEV_THRESHOLD_ACC_X        5.0f
#define STDEV_THRESHOLD_ACC_Y        5.0f
#define STDEV_THRESHOLD_ACC_Z        8.5f
#define ACC_BUFFER_SIZE 16

static int acc_buf_pos = 0;
static int acc_buf_full = false;
static int acc_init_flag = false;
static float acc_buf[3][ACC_BUFFER_SIZE];

static double acc_x_stdev = 0.0f;
static double acc_y_stdev = 0.0f;
static double acc_z_stdev = 0.0f;
static void init_acc_variables(void)
{
	int ret = 0;
    acc_buf_pos = 0;
    acc_buf_full = false;
    acc_init_flag = true;
    ret = LoadAccSTDEV(ACC_STDEV_FILE,&acc_x_stdev,&acc_y_stdev,&acc_z_stdev);
    if(ret == 0)
    {
    	acc_x_stdev = 0.0f;
    	acc_y_stdev = 0.0f;
    	acc_z_stdev = 0.0f;
    }
    memset(acc_buf, 0, sizeof(acc_buf));
}

void update_acc_buf(const float gx, const float gy, const float gz)
{
    acc_buf[x][acc_buf_pos] = gx;
    acc_buf[y][acc_buf_pos] = gy;
    acc_buf[z][acc_buf_pos] = gz;

    acc_buf_pos++;

    if (acc_buf_pos >= ACC_BUFFER_SIZE) {
        acc_buf_pos = 0;
        acc_buf_full = true;
    }
}

int detect_phone_moving(void)
{
    float stdev_val_x = 0.0f;
    float stdev_val_y = 0.0f;
    float stdev_val_z = 0.0f;
    int data_len = 0;

    if (true == acc_buf_full) {
        data_len = ACC_BUFFER_SIZE;
    } else {
        data_len = acc_buf_pos;
    }

#if 0
    stdev_val_x = CalcStdFloat(acc_buf[x], data_len);
    //ALOGE("AKM Debug stdev_val_x, %f\n",stdev_val_x);
    if (stdev_val_x > (float)acc_x_stdev) {
        return 1;
    }

    stdev_val_y = CalcStdFloat(acc_buf[y], data_len);
    //ALOGE("AKM Debug stdev_val_y, %f\n",stdev_val_y);
    if (stdev_val_y > (float)acc_y_stdev) {
        return 1;
    }

    stdev_val_z = CalcStdFloat(acc_buf[z], data_len);
    if (stdev_val_z > (float)acc_z_stdev) {
    //ALOGE("AKM Debug stdev_val_z, %f\n",stdev_val_z);
        return 1;
    }
#else
    stdev_val_x = CalcStdFloat(acc_buf[x], data_len);
    stdev_val_y = CalcStdFloat(acc_buf[y], data_len);
    stdev_val_z = CalcStdFloat(acc_buf[z], data_len);

   // LoadAccSTDEV(ACC_STDEV_FILE,&acc_x_stdev,&acc_y_stdev,&acc_z_stdev);
    ALOGE("AKM Debug stdev_val_x, %f,%f,%f\n", stdev_val_x, stdev_val_y, stdev_val_z);
    
    //ALOGE("AKM Debug acc_x_stdev, %20lf,%20lf,%20lf\n", acc_x_stdev, acc_y_stdev, acc_z_stdev);

    if (stdev_val_x > (float)acc_x_stdev) {
   //     ALOGE("AKM Debug stdev_val_x, %f\n", stdev_val_x);
        return 1;
    }

    if (stdev_val_y > (float)acc_y_stdev) {
   //     ALOGE("AKM Debug stdev_val_y, %f\n", stdev_val_y);
        return 1;
    }

    if (stdev_val_z > (float)acc_z_stdev) {
    //    ALOGE("AKM Debug stdev_val_z, %f\n", stdev_val_z);
        return 1;
    }
#endif

    return 0;
}

int phone_move(const float gx, const float gy, const float gz)
{
    if (acc_init_flag == false) {
        init_acc_variables();
    }

    update_acc_buf(gx, gy, gz);

    if (detect_phone_moving()) {
        //ALOGE("AKM Debug phone moving====");
        return 1;
    } else {
        //ALOGE("AKM Debug device stable");
        return 0;
    }
}

