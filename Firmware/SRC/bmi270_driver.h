#ifndef HB_BMI270_DRIVER_H
#define HB_BMI270_DRIVER_H
#include <stdbool.h>
#include <stdint.h>
#include "bmi270.h"
#ifdef __cplusplus
extern "C" {
#endif

#define HB_BMI270_ODR_HZ 200u
typedef enum {
    HB_IMU_OK = 0,
    HB_IMU_ERR_ARGUMENT,
    HB_IMU_ERR_VENDOR_API,
    HB_IMU_ERR_STATE
} hb_imu_result_t;
typedef struct {
    void *context;
    int8_t (*read)(void *context, uint8_t reg, uint8_t *data, uint32_t length);
    int8_t (*write)(void *context, uint8_t reg, const uint8_t *data, uint32_t length);
    void (*delay_us)(void *context, uint32_t microseconds);
    uint32_t (*micros)(void *context);
} hb_bmi270_bus_t;
typedef struct {
    uint32_t timestamp_us;
    uint32_t sensor_time;
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    uint8_t status;
} hb_imu_sample_t;
typedef struct {
    struct bmi2_dev device;
    hb_bmi270_bus_t bus;
    bool initialized;
} hb_bmi270_t;

hb_imu_result_t hb_bmi270_init(hb_bmi270_t *imu, const hb_bmi270_bus_t *bus);
hb_imu_result_t hb_bmi270_read(hb_bmi270_t *imu, hb_imu_sample_t *sample);
#ifdef __cplusplus
}
#endif
#endif
