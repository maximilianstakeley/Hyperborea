#include "hb/bmi270_driver.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    hb_bmi270_t imu = {0};
    hb_imu_sample_t sample;
    hb_bmi270_bus_t bus = {0};
    assert(hb_bmi270_init(&imu, NULL) == HB_IMU_ERR_ARGUMENT);
    assert(hb_bmi270_init(&imu, &bus) == HB_IMU_ERR_ARGUMENT);
    assert(hb_bmi270_read(&imu, &sample) == HB_IMU_ERR_STATE);
    assert(hb_bmi270_read(NULL, &sample) == HB_IMU_ERR_ARGUMENT);
    assert(hb_bmi270_read(&imu, NULL) == HB_IMU_ERR_ARGUMENT);
    puts("BMI270 driver contract tests passed (native device emulation not included)");
    return 0;
}
