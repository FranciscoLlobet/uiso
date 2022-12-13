/*
 * statistics.c
 *
 *  Created on: 4 dic 2022
 *      Author: Francisco
 */

#include "sensor_statistics.h"



void mean_and_variance(struct accelerometer_statistics_s * statistics, struct bma2_sensor_data * sensor_data, uint16_t sensor_data_len)
{
	uint32_t i = 0;

	int32_t sum_x = 0;
	int32_t sum_y = 0;
	int32_t sum_z = 0;

	for(i = 0; i < (uint32_t)sensor_data_len; i++)
	{
		sum_x = sum_x + (int32_t)sensor_data[i].x;
		sum_y = sum_y + (int32_t)sensor_data[i].y;
		sum_z = sum_z + (int32_t)sensor_data[i].z;
	}

	statistics->mean.x = (sum_x / (int32_t)sensor_data_len);
	statistics->mean.y = (sum_y / (int32_t)sensor_data_len);
	statistics->mean.z = (sum_z / (int32_t)sensor_data_len);

}
