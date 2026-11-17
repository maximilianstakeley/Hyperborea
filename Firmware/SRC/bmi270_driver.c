#include "hb/bmi270_driver.h"
#include <string.h>

static BMI2_INTF_RETURN_TYPE bmi_read(uint8_t reg, uint8_t *data, uint32_t len,
                                      void *context) {
    hb_bmi270_bus_t *bus = context;
    return bus && bus->read ? bus->read(bus->context, reg, data, len) : BMI2_E_NULL_PTR;
}
static BMI2_INTF_RETURN_TYPE bmi_write(uint8_t reg, const uint8_t *data,
                                       uint32_t len, void *context) {
    hb_bmi270_bus_t *bus = context;
    return bus && bus->write ? bus->write(bus->context, reg, data, len) : BMI2_E_NULL_PTR;
}
static void bmi_delay(uint32_t period, void *context) {
    hb_bmi270_bus_t *bus = context;
    if (bus && bus->delay_us) bus->delay_us(bus->context, period);
}

hb_imu_result_t hb_bmi270_init(hb_bmi270_t *imu, const hb_bmi270_bus_t *bus) {
    if (!imu || !bus || !bus->read || !bus->write || !bus->delay_us || !bus->micros)
        return HB_IMU_ERR_ARGUMENT;
    memset(imu, 0, sizeof *imu);
    imu->bus = *bus;
    imu->device.intf = BMI2_I2C_INTF;
    imu->device.intf_ptr = &imu->bus;
    imu->device.read = bmi_read;
    imu->device.write = bmi_write;
    imu->device.delay_us = bmi_delay;
    imu->device.read_write_len = 30u;
    imu->device.resolution = 16u;
    if (bmi270_init(&imu->device) != BMI2_OK) return HB_IMU_ERR_VENDOR_API;

    struct bmi2_sens_config requested[2];
    memset(requested, 0, sizeof requested);
    requested[0].type = BMI2_ACCEL;
    requested[0].cfg.acc.odr = BMI2_ACC_ODR_200HZ;
    requested[0].cfg.acc.range = BMI2_ACC_RANGE_4G;
    requested[0].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
    requested[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
    requested[1].type = BMI2_GYRO;
    requested[1].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;
    requested[1].cfg.gyr.range = BMI2_GYR_RANGE_500;
    requested[1].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;
    requested[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;
    requested[1].cfg.gyr.noise_perf = BMI2_PERF_OPT_MODE;
    if (bmi270_set_sensor_config(requested, 2u, &imu->device) != BMI2_OK)
        return HB_IMU_ERR_VENDOR_API;

    struct bmi2_sens_config actual[2];
    memset(actual, 0, sizeof actual);
    actual[0].type = BMI2_ACCEL;
    actual[1].type = BMI2_GYRO;
    if (bmi270_get_sensor_config(actual, 2u, &imu->device) != BMI2_OK ||
        actual[0].cfg.acc.odr != BMI2_ACC_ODR_200HZ ||
        actual[0].cfg.acc.range != BMI2_ACC_RANGE_4G ||
        actual[1].cfg.gyr.odr != BMI2_GYR_ODR_200HZ ||
        actual[1].cfg.gyr.range != BMI2_GYR_RANGE_500)
        return HB_IMU_ERR_VENDOR_API;
    const uint8_t sensors[2] = {BMI2_ACCEL, BMI2_GYRO};
    if (bmi270_sensor_enable(sensors, 2u, &imu->device) != BMI2_OK)
        return HB_IMU_ERR_VENDOR_API;
    imu->initialized = true;
    return HB_IMU_OK;
}

hb_imu_result_t hb_bmi270_read(hb_bmi270_t *imu, hb_imu_sample_t *sample) {
    if (!imu || !sample) return HB_IMU_ERR_ARGUMENT;
    if (!imu->initialized) return HB_IMU_ERR_STATE;
    struct bmi2_sens_data data;
    memset(&data, 0, sizeof data);
    if (bmi2_get_sensor_data(&data, &imu->device) != BMI2_OK)
        return HB_IMU_ERR_VENDOR_API;
    sample->timestamp_us = imu->bus.micros(imu->bus.context);
    sample->sensor_time = data.sens_time;
    sample->accel_x = data.acc.x;
    sample->accel_y = data.acc.y;
    sample->accel_z = data.acc.z;
    sample->gyro_x = data.gyr.x;
    sample->gyro_y = data.gyr.y;
    sample->gyro_z = data.gyr.z;
    sample->status = data.status;
    return HB_IMU_OK;
}
