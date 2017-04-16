
#include "yaw_average.h"

#define AKM_DATA_SIZE       32//16
#define YAW_SCALE           360
#define YAW_DIFF_THRESHODL  180
#define STD_THRESHOLD       0.1

static float akm_data_buffer[AKM_DATA_SIZE];
static int data_init_flag = 0;
static int store_save_pos = 0;
static int buffer_full_flag = 0;

static float pre_yaw = 0.0f;

static void akm_sensor_avg_init(float init_data)
{
	int i;
	for (i=0; i<AKM_DATA_SIZE; i++) {
		akm_data_buffer[i] = init_data;
	}

	store_save_pos = 0;
    buffer_full_flag = 0;

    pre_yaw = 0.0f;
}

void sortArray(float data[], int num)
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

void akm_sensor_deinit()
{
    data_init_flag = 0;
    akm_sensor_avg_init(0);
}

float akm_sensor_avg(float data)
{
	int i;
    int preData_index;
    int cur_pos;
	float data_sum = 0;
    float data_avg = 0;
    int ave_num = 0;
    float cur_data = 0.0f;
    float std_val = 0.0f;

    if (data_init_flag == 0) {
        akm_sensor_avg_init(data);
        data_init_flag = 1;
    }

    if (store_save_pos == 0) {
        preData_index = AKM_DATA_SIZE - 1;
        buffer_full_flag = 1;
    } else {
        preData_index = store_save_pos - 1;
    }
	
    if (fabs(data-akm_data_buffer[preData_index]) > (float)(YAW_DIFF_THRESHODL)) {
        cur_data = YAW_SCALE - data;
    } else {
        cur_data = data;
    }

    cur_pos = store_save_pos;
	akm_data_buffer[store_save_pos] = cur_data;
	store_save_pos++;

    if (store_save_pos >= AKM_DATA_SIZE) {
        store_save_pos = 0;
    }

    // calculate average yaw begin
    for (i=0; i<AKM_DATA_SIZE; i++) {
        data_sum += akm_data_buffer[i];
    }

    if (buffer_full_flag == 0) {
        ave_num = store_save_pos + 1;
    } else {
        ave_num = AKM_DATA_SIZE;
    }
    data_avg = data_sum / ave_num;
    // calculate average yaw end 

    //dump();
    // calculate standard deviation
    std_val = CalcStdFloat(akm_data_buffer, AKM_DATA_SIZE);

    if (std_val > STD_THRESHOLD) {
        akm_data_buffer[cur_pos] = pre_yaw; 
    } else {
        pre_yaw = data_avg;
    }

    return pre_yaw;
}

void dump()
{
    ALOGE("save_pos,%d,"
          "data,%f,%f,%f,%f,%f,%f,%f,%f,"
          "%f,%f,%f,%f,%f,%f,%f,%f\n",
          store_save_pos,
          akm_data_buffer[0], akm_data_buffer[1], akm_data_buffer[2], akm_data_buffer[3],
          akm_data_buffer[4], akm_data_buffer[5], akm_data_buffer[6], akm_data_buffer[7],
          akm_data_buffer[8], akm_data_buffer[9], akm_data_buffer[10], akm_data_buffer[11],
          akm_data_buffer[12], akm_data_buffer[13], akm_data_buffer[14], akm_data_buffer[15]
    );
}
