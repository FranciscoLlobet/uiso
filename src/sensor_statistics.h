/*
 * sensor_statistics.h
 *
 *  Created on: 4 dic 2022
 *      Author: Francisco
 */

#ifndef SENSOR_STATISTICS_H_
#define SENSOR_STATISTICS_H_

#include "bma2.h"

struct accelerometer_statistics_s
{
	struct{
		int16_t x;
		int16_t y;
		int16_t z;
	}mean;

	struct{
		int32_t x;
		int32_t y;
		int32_t z;
	}variance;
};

struct accelerometer_data_s
{
	float x;
	float y;
	float z;
};

void mean_and_variance(struct accelerometer_statistics_s * statistics, struct bma2_sensor_data * sensor_data, uint16_t sensor_data_len);

#endif /* SENSOR_STATISTICS_H_ */
