/**
 *   @defgroup  eMPL
 *   @brief     Embedded Motion Processing Library
 *
 *   @{
 *       @file      motion_driver_test.c
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "USB_eMPL/descriptors.h"

// #include "USB_API/USB_Common/device.h"
// #include "USB_API/USB_Common/types.h"
// #include "USB_API/USB_Common/usb.h"

// #include "F5xx_F6xx_Core_Lib/HAL_UCS.h"
// #include "F5xx_F6xx_Core_Lib/HAL_PMM.h"
// #include "F5xx_F6xx_Core_Lib/HAL_FLASH.h"

// #include "USB_API/USB_CDC_API/UsbCdc.h"
// #include "usbConstructs.h"

// #include "msp430.h"
// #include "msp430_clock.h"
// #include "msp430_i2c.h"
// #include "msp430_interrupt.h"

#include "driver/i2c.h"
#include "empl_driver.h"
#include "esp_system.h"
#include "mpu_dmp_driver.h"

#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"

/* Data requested by client. */
#define PRINT_ACCEL (0x01)
#define PRINT_GYRO (0x02)
#define PRINT_QUAT (0x04)

#define ACCEL_ON (0x01)
#define GYRO_ON (0x02)

#define MOTION (0)
#define NO_MOTION (1)

#define MPU9250_ADDR 0X68        // MPU6500的器件IIC地址
#define MPU_ACCEL_XOUTH_REG 0X3B // 加速度值,X轴高8位寄存器
#define MPU_GYRO_XOUTH_REG 0X43  // 陀螺仪值,X轴高8位寄存器
#define AK8963_ADDR 0X0C
#define AK8963_ID 0X48
//#define MAG_XOUT_L 0X03
#define MAG_CNTL1 0X0A
#define MAG_WIA 0x00

/* Starting sampling rate. */

#define FLASH_SIZE (512)
#define FLASH_MEM_START ((void *)0x1800)

struct rx_s {
    unsigned char header[3];
    unsigned char cmd;
};
struct hal_s {
    unsigned char sensors;
    unsigned char dmp_on;
    unsigned char wait_for_tap;
    volatile unsigned char new_gyro;
    unsigned short report;
    unsigned short dmp_features;
    unsigned char motion_int_mode;
    struct rx_s rx;
};
static struct hal_s hal = {0};

/* USB RX binary semaphore. Actually, it's just a flag. Not included in struct
 * because it's declared extern elsewhere.
 */
volatile unsigned char rx_new;

/* The sensors can be mounted onto the board in any orientation. The mounting
 * matrix seen below tells the MPL how to rotate the raw data from thei
 * driver(s).
 * TODO: The following matrices refer to the configuration on an internal test
 * board at Invensense. If needed, please modify the matrices to match the
 * chip-to-body matrix for your particular set up.
 */
static signed char gyro_orientation[9] = {-1, 0, 0,
                                          0, -1, 0,
                                          0, 0, 1};

// enum packet_type_e {
//     PACKET_TYPE_ACCEL,
//     PACKET_TYPE_GYRO,
//     PACKET_TYPE_QUAT,
//     PACKET_TYPE_TAP,
//     PACKET_TYPE_ANDROID_ORIENT,
//     PACKET_TYPE_PEDO,
//     PACKET_TYPE_MISC
// };

/* Send data to the Python client application.
 * Data is formatted as follows:
 * packet[0]    = $
 * packet[1]    = packet type (see packet_type_e)
 * packet[2+]   = data
 */
// void send_packet(char packet_type, void *data)
// {
// #define MAX_BUF_LENGTH  (18)
//     char buf[MAX_BUF_LENGTH], length;

//     memset(buf, 0, MAX_BUF_LENGTH);
//     buf[0] = '$';
//     buf[1] = packet_type;

//     if (packet_type == PACKET_TYPE_ACCEL || packet_type == PACKET_TYPE_GYRO) {
//         short *sdata = (short*)data;
//         buf[2] = (char)(sdata[0] >> 8);
//         buf[3] = (char)sdata[0];
//         buf[4] = (char)(sdata[1] >> 8);
//         buf[5] = (char)sdata[1];
//         buf[6] = (char)(sdata[2] >> 8);
//         buf[7] = (char)sdata[2];
//         length = 8;
//     } else if (packet_type == PACKET_TYPE_QUAT) {
//         long *ldata = (long*)data;
//         buf[2] = (char)(ldata[0] >> 24);
//         buf[3] = (char)(ldata[0] >> 16);
//         buf[4] = (char)(ldata[0] >> 8);
//         buf[5] = (char)ldata[0];
//         buf[6] = (char)(ldata[1] >> 24);
//         buf[7] = (char)(ldata[1] >> 16);
//         buf[8] = (char)(ldata[1] >> 8);
//         buf[9] = (char)ldata[1];
//         buf[10] = (char)(ldata[2] >> 24);
//         buf[11] = (char)(ldata[2] >> 16);
//         buf[12] = (char)(ldata[2] >> 8);
//         buf[13] = (char)ldata[2];
//         buf[14] = (char)(ldata[3] >> 24);
//         buf[15] = (char)(ldata[3] >> 16);
//         buf[16] = (char)(ldata[3] >> 8);
//         buf[17] = (char)ldata[3];
//         length = 18;
//     } else if (packet_type == PACKET_TYPE_TAP) {
//         buf[2] = ((char*)data)[0];
//         buf[3] = ((char*)data)[1];
//         length = 4;
//     } else if (packet_type == PACKET_TYPE_ANDROID_ORIENT) {
//         buf[2] = ((char*)data)[0];
//         length = 3;
//     } else if (packet_type == PACKET_TYPE_PEDO) {
//         long *ldata = (long*)data;
//         buf[2] = (char)(ldata[0] >> 24);
//         buf[3] = (char)(ldata[0] >> 16);
//         buf[4] = (char)(ldata[0] >> 8);
//         buf[5] = (char)ldata[0];
//         buf[6] = (char)(ldata[1] >> 24);
//         buf[7] = (char)(ldata[1] >> 16);
//         buf[8] = (char)(ldata[1] >> 8);
//         buf[9] = (char)ldata[1];
//         length = 10;
//     } else if (packet_type == PACKET_TYPE_MISC) {
//         buf[2] = ((char*)data)[0];
//         buf[3] = ((char*)data)[1];
//         buf[4] = ((char*)data)[2];
//         buf[5] = ((char*)data)[3];
//         length = 6;
//     }
//     cdcSendDataWaitTilDone((BYTE*)buf, length, CDC0_INTFNUM, 100);
// }

/* These next two functions converts the orientation matrix (see
 * gyro_orientation) to a scalar representation for use by the DMP.
 * NOTE: These functions are borrowed from Invensense's MPL.
 */
static inline unsigned short inv_row_2_scale(const signed char *row)
{
    unsigned short b;

    if (row[0] > 0)
        b = 0;
    else if (row[0] < 0)
        b = 4;
    else if (row[1] > 0)
        b = 1;
    else if (row[1] < 0)
        b = 5;
    else if (row[2] > 0)
        b = 2;
    else if (row[2] < 0)
        b = 6;
    else
        b = 7; // error
    return b;
}

static inline unsigned short inv_orientation_matrix_to_scalar(
    const signed char *mtx)
{
    unsigned short scalar;

    /*
       XYZ  010_001_000 Identity Matrix
       XZY  001_010_000
       YXZ  010_000_001
       YZX  000_010_001
       ZXY  001_000_010
       ZYX  000_001_010
     */

    scalar = inv_row_2_scale(mtx);
    scalar |= inv_row_2_scale(mtx + 3) << 3;
    scalar |= inv_row_2_scale(mtx + 6) << 6;

    return scalar;
}

/* Handle sensor on/off combinations. */
// static void setup_gyro(void)
// {
//     unsigned char mask = 0;
//     if (hal.sensors & ACCEL_ON)
//         mask |= INV_XYZ_ACCEL;
//     if (hal.sensors & GYRO_ON)
//         mask |= INV_XYZ_GYRO;
//     /* If you need a power transition, this function should be called with a
//      * mask of the sensors still enabled. The driver turns off any sensors
//      * excluded from this mask.
//      */
//     mpu_set_sensors(mask);
//     if (!hal.dmp_on)
//         mpu_configure_fifo(mask);
// }

// 手势回调函数
// static void tap_cb(unsigned char direction, unsigned char count)
// {
//     char data[2];
//     data[0] = (char)direction;
//     data[1] = (char)count;
//     send_packet(PACKET_TYPE_TAP, data);
// }

// static void android_orient_cb(unsigned char orientation)
// {
//     send_packet(PACKET_TYPE_ANDROID_ORIENT, &orientation);
// }

// static inline void msp430_reset(void)
// {
//     PMMCTL0 |= PMMSWPOR;
// }

// 重启系统
void system_reset(void)
{
    esp_restart();
}

static inline void run_self_test(void)
{
    int result;
    // char test_packet[4] = {0};
    long gyro[3], accel[3];
    unsigned char i = 0;

#if defined(MPU6500) || defined(MPU9250)
    result = mpu_run_6500_self_test(gyro, accel, 0);
#elif defined(MPU6050) || defined(MPU9150)
    result = mpu_run_self_test(gyro, accel);
#endif
    if (result == 0x3) { // 六轴不是九轴
        /* Test passed. We can trust the gyro data here, so let's push it down
         * to the DMP.
         */
        for (i = 0; i < 3; i++) {
            gyro[i] = (long)(gyro[i] * 32.8f); // convert to +-1000dps
            accel[i] *= 2048.f;                // convert to +-16G
            accel[i] = accel[i] >> 16;
            gyro[i] = (long)(gyro[i] >> 16);
        }

        mpu_set_gyro_bias_reg(gyro);

#if defined(MPU6500) || defined(MPU9250)
        mpu_set_accel_bias_6500_reg(accel);
#elif defined(MPU6050) || defined(MPU9150)
        mpu_set_accel_bias_6050_reg(accel);
#endif
    }

    /* Report results. */
    // test_packet[0] = 't';
    // test_packet[1] = result;
    // send_packet(PACKET_TYPE_MISC, test_packet);
}

// static void handle_input(void)
// {
//     char c;
//     const unsigned char header[3] = "inv";
//     unsigned long pedo_packet[2];

//     /* Read incoming byte and check for header.
//      * Technically, the MSP430 USB stack can handle more than one byte at a
//      * time. This example allows for easily switching to UART if porting to a
//      * different microcontroller.
//      */
//     rx_new = 0;
//     cdcReceiveDataInBuffer((BYTE*)&c, 1, CDC0_INTFNUM);
//     if (hal.rx.header[0] == header[0]) {
//         if (hal.rx.header[1] == header[1]) {
//             if (hal.rx.header[2] == header[2]) {
//                 memset(&hal.rx.header, 0, sizeof(hal.rx.header));
//                 hal.rx.cmd = c;
//             } else if (c == header[2])
//                 hal.rx.header[2] = c;
//             else
//                 memset(&hal.rx.header, 0, sizeof(hal.rx.header));
//         } else if (c == header[1])
//             hal.rx.header[1] = c;
//         else
//             memset(&hal.rx.header, 0, sizeof(hal.rx.header));
//     } else if (c == header[0])
//         hal.rx.header[0] = header[0];
//     if (!hal.rx.cmd)
//         return;

//     switch (hal.rx.cmd) {
//     /* These commands turn the hardware sensors on/off. */
//     case '8':
//         if (!hal.dmp_on) {
//             /* Accel and gyro need to be on for the DMP features to work
//              * properly.
//              */
//             hal.sensors ^= ACCEL_ON;
//             setup_gyro();
//         }
//         break;
//     case '9':
//         if (!hal.dmp_on) {
//             hal.sensors ^= GYRO_ON;
//             setup_gyro();
//         }
//         break;
//     /* The commands start/stop sending data to the client. */
//     case 'a':
//         hal.report ^= PRINT_ACCEL;
//         break;
//     case 'g':
//         hal.report ^= PRINT_GYRO;
//         break;
//     case 'q':
//         hal.report ^= PRINT_QUAT;
//         break;
//     /* The hardware self test can be run without any interaction with the
//      * MPL since it's completely localized in the gyro driver. Logging is
//      * assumed to be enabled; otherwise, a couple LEDs could probably be used
//      * here to display the test results.
//      */
//     case 't':
//         run_self_test();
//         break;
//     /* Depending on your application, sensor data may be needed at a faster or
//      * slower rate. These commands can speed up or slow down the rate at which
//      * the sensor data is pushed to the MPL.
//      *
//      * In this example, the compass rate is never changed.
//      */
//     case '1':
//         if (hal.dmp_on)
//             dmp_set_fifo_rate(10);
//         else
//             mpu_set_sample_rate(10);
//         break;
//     case '2':
//         if (hal.dmp_on)
//             dmp_set_fifo_rate(20);
//         else
//             mpu_set_sample_rate(20);
//         break;
//     case '3':
//         if (hal.dmp_on)
//             dmp_set_fifo_rate(40);
//         else
//             mpu_set_sample_rate(40);
//         break;
//     case '4':
//         if (hal.dmp_on)
//             dmp_set_fifo_rate(50);
//         else
//             mpu_set_sample_rate(50);
//         break;
//     case '5':
//         if (hal.dmp_on)
//             dmp_set_fifo_rate(100);
//         else
//             mpu_set_sample_rate(100);
//         break;
//     case '6':
//         if (hal.dmp_on)
//             dmp_set_fifo_rate(200);
//         else
//             mpu_set_sample_rate(200);
//         break;
// 	case ',':
//         /* Set hardware to interrupt on gesture event only. This feature is
//          * useful for keeping the MCU asleep until the DMP detects as a tap or
//          * orientation event.
//          */
//         dmp_set_interrupt_mode(DMP_INT_GESTURE);
//         break;
//     case '.':
//         /* Set hardware to interrupt periodically. */
//         dmp_set_interrupt_mode(DMP_INT_CONTINUOUS);
//         break;
//     case '7':
//         /* Reset pedometer. */
//         dmp_set_pedometer_step_count(0);
//         dmp_set_pedometer_walk_time(0);
//         break;
//     case 'f':
//         /* Toggle DMP. */
//         if (hal.dmp_on) {
//             unsigned short dmp_rate;
//             hal.dmp_on = 0;
//             mpu_set_dmp_state(0);
//             /* Restore FIFO settings. */
//             mpu_configure_fifo(INV_XYZ_ACCEL | INV_XYZ_GYRO);
//             /* When the DMP is used, the hardware sampling rate is fixed at
//              * 200Hz, and the DMP is configured to downsample the FIFO output
//              * using the function dmp_set_fifo_rate. However, when the DMP is
//              * turned off, the sampling rate remains at 200Hz. This could be
//              * handled in inv_mpu.c, but it would need to know that
//              * inv_mpu_dmp_motion_driver.c exists. To avoid this, we'll just
//              * put the extra logic in the application layer.
//              */
//             dmp_get_fifo_rate(&dmp_rate);
//             mpu_set_sample_rate(dmp_rate);
//         } else {
//             unsigned short sample_rate;
//             hal.dmp_on = 1;
//             /* Both gyro and accel must be on. */
//             hal.sensors |= ACCEL_ON | GYRO_ON;
//             mpu_set_sensors(INV_XYZ_ACCEL | INV_XYZ_GYRO);
//             mpu_configure_fifo(INV_XYZ_ACCEL | INV_XYZ_GYRO);
//             /* Preserve current FIFO rate. */
//             mpu_get_sample_rate(&sample_rate);
//             dmp_set_fifo_rate(sample_rate);
//             mpu_set_dmp_state(1);
//         }
//         break;
//     case 'm':
//         /* Test the motion interrupt hardware feature. */
// 		#ifndef MPU6050 // not enabled for 6050 product
// 		hal.motion_int_mode = 1;
// 		#endif
//         break;
//     case 'p':
//         /* Read current pedometer count. */
//         dmp_get_pedometer_step_count(pedo_packet);
//         dmp_get_pedometer_walk_time(pedo_packet + 1);
//         send_packet(PACKET_TYPE_PEDO, pedo_packet);
//         break;
//     case 'x':
//         msp430_reset();
//         break;
//     case 'v':
//         /* Toggle LP quaternion.
//          * The DMP features can be enabled/disabled at runtime. Use this same
//          * approach for other features.
//          */
//         hal.dmp_features ^= DMP_FEATURE_6X_LP_QUAT;
//         dmp_enable_feature(hal.dmp_features);
//         break;
//     default:
//         break;
//     }
//     hal.rx.cmd = 0;
// }

/* Every time new gyro data is available, this function is called in an
 * ISR context. In this example, it sets a flag protecting the FIFO read
 * function.
 */
void gyro_data_ready_cb(void)
{
    hal.new_gyro = 1;
}

/* Set up MSP430 peripherals. */
// static inline void platform_init(void)
// {
// 	WDTCTL = WDTPW | WDTHOLD;
//     SetVCore(2);
//     msp430_clock_init(12000000L, 2);
//     if (USB_init() != kUSB_succeed)
//         msp430_reset();
//     msp430_i2c_enable();
//     msp430_int_init();

//     USB_setEnabledEvents(kUSB_allUsbEvents);
//     if (USB_connectionInfo() & kUSB_vbusPresent){
//         if (USB_enable() == kUSB_succeed){
//             USB_reset();
//             USB_connect();
//         } else
//             msp430_reset();
//     }
// }

uint8_t mpu_init_i2c(void)
{
    esp_err_t esp_err;
    static i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MPU_I2C_SDA, // select GPIO specific to your project
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = MPU_I2C_SCL, // select GPIO specific to your project
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 800000, // select frequency specific to your project
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    esp_err = i2c_param_config(0, &conf);
    printf("i2c_param_config: %d \n", esp_err);

    esp_err = i2c_driver_install(0, I2C_MODE_MASTER, 0, 0, 0);
    printf("i2c_driver_install: %d \n", esp_err);

    return esp_err;
}

// 初始化mpu6050
uint8_t mpu_dmp_init(void)
{
    if (mpu_init_i2c() != 0)
        return 1;
    int result = 0;
    // unsigned char accel_fsr;
    // unsigned short gyro_rate, gyro_fsr;
    // unsigned long timestamp;
    // struct int_param_s int_param;

    // /* Set up MSP430 hardware. */
    // platform_init();

    /* Set up gyro.
     * Every function preceded by mpu_ is a driver function and can be found
     * in inv_mpu.h.
     */
    // int_param.cb = gyro_data_ready_cb;
    // int_param.pin = INT_PIN_P20;
    // int_param.lp_exit = INT_EXIT_LPM0;
    // int_param.active_low = 1;

    // result = mpu_init(&int_param);
    result = mpu_init();
    printf("mpu_init: %d\n", result);

    if (result) {
        // msp430_reset();
        system_reset();
        return 1;
    };
    /* If you're not using an MPU9150 AND you're not using DMP features, this
     * function will place all slaves on the primary bus.
     * mpu_set_bypass(1);
     */

    /* Get/set hardware configuration. Start gyro. */
    /* Wake up all sensors. */
    if (mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL)) {
        return 2;
    }

    /* Push both gyro and accel data into the FIFO. */
    if (mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL)) {
        return 3;
    }
    if (mpu_set_sample_rate(DEFAULT_MPU_HZ)) {
        return 4;
    }
    /* Read back configuration in case it was set improperly. */
    // mpu_get_sample_rate(&gyro_rate);
    // mpu_get_gyro_fsr(&gyro_fsr);
    // mpu_get_accel_fsr(&accel_fsr);

    /* Initialize HAL state variables. */
    memset(&hal, 0, sizeof(hal));
    hal.sensors = ACCEL_ON | GYRO_ON;
    // hal.report = PRINT_QUAT;

    /* To initialize the DMP:
     * 1. Call dmp_load_motion_driver_firmware(). This pushes the DMP image in
     *    inv_mpu_dmp_motion_driver.h into the MPU memory.
     * 2. Push the gyro and accel orientation matrix to the DMP.
     * 3. Register gesture callbacks. Don't worry, these callbacks won't be
     *    executed unless the corresponding feature is enabled.
     * 4. Call dmp_enable_feature(mask) to enable different features.
     * 5. Call dmp_set_fifo_rate(freq) to select a DMP output rate.
     * 6. Call any feature-specific control functions.
     *
     * To enable the DMP, just call mpu_set_dmp_state(1). This function can
     * be called repeatedly to enable and disable the DMP at runtime.
     *
     * The following is a short summary of the features supported in the DMP
     * image provided in inv_mpu_dmp_motion_driver.c:
     * DMP_FEATURE_LP_QUAT: Generate a gyro-only quaternion on the DMP at
     * 200Hz. Integrating the gyro data at higher rates reduces numerical
     * errors (compared to integration on the MCU at a lower sampling rate).
     * DMP_FEATURE_6X_LP_QUAT: Generate a gyro/accel quaternion on the DMP at
     * 200Hz. Cannot be used in combination with DMP_FEATURE_LP_QUAT.
     * DMP_FEATURE_TAP: Detect taps along the X, Y, and Z axes.
     * DMP_FEATURE_ANDROID_ORIENT: Google's screen rotation algorithm. Triggers
     * an event at the four orientations where the screen should rotate.
     * DMP_FEATURE_GYRO_CAL: Calibrates the gyro data after eight seconds of
     * no motion.
     * DMP_FEATURE_SEND_RAW_ACCEL: Add raw accelerometer data to the FIFO.
     * DMP_FEATURE_SEND_RAW_GYRO: Add raw gyro data to the FIFO.
     * DMP_FEATURE_SEND_CAL_GYRO: Add calibrated gyro data to the FIFO. Cannot
     * be used in combination with DMP_FEATURE_SEND_RAW_GYRO.
     */
    if (dmp_load_motion_driver_firmware()) {
        return 5;
    }
    if (dmp_set_orientation(inv_orientation_matrix_to_scalar(gyro_orientation))) {
        return 6;
    }
    // dmp_register_tap_cb(tap_cb);
    // dmp_register_android_orient_cb(android_orient_cb);
    /*
     * Known Bug -
     * DMP when enabled will sample sensor data at 200Hz and output to FIFO at the rate
     * specified in the dmp_set_fifo_rate API. The DMP will then sent an interrupt once
     * a sample has been put into the FIFO. Therefore if the dmp_set_fifo_rate is at 25Hz
     * there will be a 25Hz interrupt from the MPU device.
     *
     * There is a known issue in which if you do not enable DMP_FEATURE_TAP
     * then the interrupts will be at 200Hz even if fifo rate
     * is set at a different rate. To avoid this issue include the DMP_FEATURE_TAP
     */
    hal.dmp_features = DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_TAP |
                       DMP_FEATURE_ANDROID_ORIENT | DMP_FEATURE_SEND_RAW_ACCEL | DMP_FEATURE_SEND_CAL_GYRO |
                       DMP_FEATURE_GYRO_CAL;
    if (dmp_enable_feature(hal.dmp_features)) {
        return 7;
    }
    if (dmp_set_fifo_rate(DEFAULT_MPU_HZ)) {
        return 8;
    }
    mpu_set_dmp_state(1);
    hal.dmp_on = 1;

    return 0;
    // __enable_interrupt();

    // /* Wait for enumeration. */
    // while (USB_connectionState() != ST_ENUM_ACTIVE);
}

// 得到加速度值(原始值)
// gx,gy,gz:陀螺仪x,y,z轴的原始读数(带符号)
// 返回值:0,成功
//     其他,错误代码
uint8_t MPU_Get_Accelerometer(short *ax, short *ay, short *az)
{
    uint8_t buf[6];
    uint8_t res;
    res = esp32_i2c_read(MPU9250_ADDR, MPU_ACCEL_XOUTH_REG, 6, buf);
    if (res == 0) {
        *ax = ((uint16_t)buf[0] << 8) | buf[1];
        *ay = ((uint16_t)buf[2] << 8) | buf[3];
        *az = ((uint16_t)buf[4] << 8) | buf[5];
    }
    return res;
}

// 得到陀螺仪值(原始值)
// gx,gy,gz:陀螺仪x,y,z轴的原始读数(带符号)
// 返回值:0,成功
//     其他,错误代码
uint8_t MPU_Get_Gyroscope(short *gx, short *gy, short *gz)
{
    uint8_t buf[6], res;
    res = esp32_i2c_read(MPU9250_ADDR, MPU_GYRO_XOUTH_REG, 6, buf);
    if (res == 0) {
        *gx = ((uint16_t)buf[0] << 8) | buf[1];
        *gy = ((uint16_t)buf[2] << 8) | buf[3];
        *gz = ((uint16_t)buf[4] << 8) | buf[5];
    }
    return res;
    ;
}

// 得到磁力计值(原始值)
// mx,my,mz:磁力计x,y,z轴的原始读数(带符号)
// 返回值:0,成功
//     其他,错误代码
uint8_t MPU_Get_Magnetometer(short *mx, short *my, short *mz)
{
    uint8_t buf[6], res;
    uint8_t write[1];
    write[0] = 0x11;
    res = esp32_i2c_read(AK8963_ADDR, MAG_XOUT_L, 6, buf);
    if (res != 0) {
        printf("AK8963 read error\n");
    }
    if (res == 0) {
        *mx = ((uint16_t)buf[1] << 8) | buf[0];
        *my = ((uint16_t)buf[3] << 8) | buf[2];
        *mz = ((uint16_t)buf[5] << 8) | buf[4];
    }
    res = esp32_i2c_write(AK8963_ADDR, MAG_CNTL1, 1, write); // AK8963每次读完以后都需要重新设置为单次测量模式
    if (res != 0) {
        printf("AK8963 write error\n");
    }
    esp32_delay_ms(1);
    return res;
}

unsigned long sensor_timestamp;
short gyro[3], accel[3], sensors;
short aacx, aacy, aacz;
short gyrox, gyroy, gyroz; // 陀螺仪原始数据
unsigned char more;
long quat[4];
float norm;

#define q30 1073741824.0f
float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
float pitch, roll, yaw;
float pitch_deg, roll_deg, yaw_deg;
float Gx, Gy, Gz;
float ax, ay, az;

// 将角度转换为弧度
float degrees_to_radians(float degrees)
{
    return degrees * M_PI / 180.0;
}

#ifdef GET_LINEAR_ACC_AND_G

typedef struct {
    float x;
    float y;
    float z;
} vector3D;

typedef struct {
    float x;
    float y;
    float z;
    float Gx;
    float Gy;
    float Gz;
} vector3D_G;

typedef struct {
    float roll;
    float pitch;
    float yaw;
} eulerAngles;

vector3D linearAcc = {0, 0, 0};
vector3D gravity = {0.0, 0.0, -9.8};
eulerAngles angles = {0, 0, 0};

// 计算旋转矩阵
void calculate_rotation_matrix(eulerAngles angles, float rotationMatrix[3][3])
{
    float cosRoll = cos(degrees_to_radians(angles.roll));
    float sinRoll = sin(degrees_to_radians(angles.roll));
    float cosPitch = cos(degrees_to_radians(angles.pitch));
    float sinPitch = sin(degrees_to_radians(angles.pitch));
    float cosYaw = cos(degrees_to_radians(angles.yaw));
    float sinYaw = sin(degrees_to_radians(angles.yaw));

    // rotationMatrix[0][0] = cosPitch * cosYaw;
    // rotationMatrix[0][1] = cosPitch * sinYaw;
    // rotationMatrix[0][2] = -sinPitch;

    // rotationMatrix[1][0] = sinRoll * sinPitch * cosYaw - cosRoll * sinYaw;
    // rotationMatrix[1][1] = sinRoll * sinPitch * sinYaw + cosRoll * cosYaw;
    // rotationMatrix[1][2] = sinRoll * cosPitch;

    // rotationMatrix[2][0] = cosRoll * sinPitch * cosYaw + sinRoll * sinYaw;
    // rotationMatrix[2][1] = cosRoll * sinPitch * sinYaw - sinRoll * cosYaw;
    // rotationMatrix[2][2] = cosRoll * cosPitch;

    rotationMatrix[0][0] = cosYaw * cosPitch + sinYaw * sinRoll * sinPitch;
    rotationMatrix[0][1] = -cosYaw * sinPitch + sinYaw * sinRoll * sinPitch;
    rotationMatrix[0][2] = -sinYaw * cosRoll;

    rotationMatrix[1][0] = sinPitch * cosRoll;
    rotationMatrix[1][1] = cosPitch * cosRoll;
    rotationMatrix[1][2] = sinRoll;

    rotationMatrix[2][0] = sinYaw * sinPitch - cosYaw * sinRoll * sinPitch;
    rotationMatrix[2][1] = -sinYaw * sinPitch - cosYaw * sinRoll * cosPitch;
    rotationMatrix[2][2] = cosYaw * cosRoll;
}

// 剔除重力加速度
vector3D_G get_result(vector3D linearAcc, vector3D gravity, eulerAngles angles)
{
    float rotationMatrix[3][3];
    calculate_rotation_matrix(angles, rotationMatrix);

    // 将重力加速度旋转到局部坐标系
    float localGravity[3];
    localGravity[1] = rotationMatrix[0][0] * gravity.x + rotationMatrix[0][1] * gravity.y + rotationMatrix[0][2] * gravity.z;
    localGravity[0] = rotationMatrix[1][0] * gravity.x + rotationMatrix[1][1] * gravity.y + rotationMatrix[1][2] * gravity.z;
    localGravity[2] = rotationMatrix[2][0] * gravity.x + rotationMatrix[2][1] * gravity.y + rotationMatrix[2][2] * gravity.z;

    // 剔除重力加速度
    vector3D_G linearAccWithoutGravity;
    linearAccWithoutGravity.Gy = localGravity[1];
    linearAccWithoutGravity.Gx = localGravity[0];
    linearAccWithoutGravity.Gz = localGravity[2];
    linearAccWithoutGravity.x = linearAcc.x - localGravity[0];
    linearAccWithoutGravity.y = linearAcc.y - localGravity[1];
    linearAccWithoutGravity.z = linearAcc.z + localGravity[2];

    return linearAccWithoutGravity;
}
#endif // GET_LINEAR_ACC_AND_G

// void main(void)
// {
//     int result;
//     unsigned char accel_fsr;
//     unsigned short gyro_rate, gyro_fsr;
//     unsigned long timestamp;
//     struct int_param_s int_param;

//     /* Set up MSP430 hardware. */
//     platform_init();

//     /* Set up gyro.
//      * Every function preceded by mpu_ is a driver function and can be found
//      * in inv_mpu.h.
//      */
//     int_param.cb = gyro_data_ready_cb;
//     int_param.pin = INT_PIN_P20;
//     int_param.lp_exit = INT_EXIT_LPM0;
//     int_param.active_low = 1;
//     result = mpu_init(&int_param);
//     if (result)
//         msp430_reset();

//     /* If you're not using an MPU9150 AND you're not using DMP features, this
//      * function will place all slaves on the primary bus.
//      * mpu_set_bypass(1);
//      */

//     /* Get/set hardware configuration. Start gyro. */
//     /* Wake up all sensors. */
//     mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL);
//     /* Push both gyro and accel data into the FIFO. */
//     mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL);
//     mpu_set_sample_rate(DEFAULT_MPU_HZ);
//     /* Read back configuration in case it was set improperly. */
//     mpu_get_sample_rate(&gyro_rate);
//     mpu_get_gyro_fsr(&gyro_fsr);
//     mpu_get_accel_fsr(&accel_fsr);

//     /* Initialize HAL state variables. */
//     memset(&hal, 0, sizeof(hal));
//     hal.sensors = ACCEL_ON | GYRO_ON;
//     hal.report = PRINT_QUAT;

//     /* To initialize the DMP:
//      * 1. Call dmp_load_motion_driver_firmware(). This pushes the DMP image in
//      *    inv_mpu_dmp_motion_driver.h into the MPU memory.
//      * 2. Push the gyro and accel orientation matrix to the DMP.
//      * 3. Register gesture callbacks. Don't worry, these callbacks won't be
//      *    executed unless the corresponding feature is enabled.
//      * 4. Call dmp_enable_feature(mask) to enable different features.
//      * 5. Call dmp_set_fifo_rate(freq) to select a DMP output rate.
//      * 6. Call any feature-specific control functions.
//      *
//      * To enable the DMP, just call mpu_set_dmp_state(1). This function can
//      * be called repeatedly to enable and disable the DMP at runtime.
//      *
//      * The following is a short summary of the features supported in the DMP
//      * image provided in inv_mpu_dmp_motion_driver.c:
//      * DMP_FEATURE_LP_QUAT: Generate a gyro-only quaternion on the DMP at
//      * 200Hz. Integrating the gyro data at higher rates reduces numerical
//      * errors (compared to integration on the MCU at a lower sampling rate).
//      * DMP_FEATURE_6X_LP_QUAT: Generate a gyro/accel quaternion on the DMP at
//      * 200Hz. Cannot be used in combination with DMP_FEATURE_LP_QUAT.
//      * DMP_FEATURE_TAP: Detect taps along the X, Y, and Z axes.
//      * DMP_FEATURE_ANDROID_ORIENT: Google's screen rotation algorithm. Triggers
//      * an event at the four orientations where the screen should rotate.
//      * DMP_FEATURE_GYRO_CAL: Calibrates the gyro data after eight seconds of
//      * no motion.
//      * DMP_FEATURE_SEND_RAW_ACCEL: Add raw accelerometer data to the FIFO.
//      * DMP_FEATURE_SEND_RAW_GYRO: Add raw gyro data to the FIFO.
//      * DMP_FEATURE_SEND_CAL_GYRO: Add calibrated gyro data to the FIFO. Cannot
//      * be used in combination with DMP_FEATURE_SEND_RAW_GYRO.
//      */
//     dmp_load_motion_driver_firmware();
//     dmp_set_orientation(
//         inv_orientation_matrix_to_scalar(gyro_orientation));
//     dmp_register_tap_cb(tap_cb);
//     dmp_register_android_orient_cb(android_orient_cb);
//     /*
//      * Known Bug -
//      * DMP when enabled will sample sensor data at 200Hz and output to FIFO at the rate
//      * specified in the dmp_set_fifo_rate API. The DMP will then sent an interrupt once
//      * a sample has been put into the FIFO. Therefore if the dmp_set_fifo_rate is at 25Hz
//      * there will be a 25Hz interrupt from the MPU device.
//      *
//      * There is a known issue in which if you do not enable DMP_FEATURE_TAP
//      * then the interrupts will be at 200Hz even if fifo rate
//      * is set at a different rate. To avoid this issue include the DMP_FEATURE_TAP
//      */
//     hal.dmp_features = DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_TAP |
//         DMP_FEATURE_ANDROID_ORIENT | DMP_FEATURE_SEND_RAW_ACCEL | DMP_FEATURE_SEND_CAL_GYRO |
//         DMP_FEATURE_GYRO_CAL;
//     dmp_enable_feature(hal.dmp_features);
//     dmp_set_fifo_rate(DEFAULT_MPU_HZ);
//     mpu_set_dmp_state(1);
//     hal.dmp_on = 1;

//     __enable_interrupt();

//     /* Wait for enumeration. */
//     while (USB_connectionState() != ST_ENUM_ACTIVE);

//     while (1) {
//         unsigned long sensor_timestamp;
//         if (rx_new)
//             /* A byte has been received via USB. See handle_input for a list of
//              * valid commands.
//              */
//             handle_input();
//         msp430_get_clock_ms(&timestamp);

//         if (hal.motion_int_mode) {
//             /* Enable motion interrupt. */
// 			mpu_lp_motion_interrupt(500, 1, 5);
//             hal.new_gyro = 0;
//             /* Wait for the MPU interrupt. */
//             while (!hal.new_gyro)
//                 __bis_SR_register(LPM0_bits + GIE);
//             /* Restore the previous sensor configuration. */
//             mpu_lp_motion_interrupt(0, 0, 0);
//             hal.motion_int_mode = 0;
//         }

//         if (!hal.sensors || !hal.new_gyro) {
//             /* Put the MSP430 to sleep until a timer interrupt or data ready
//              * interrupt is detected.
//              */
//             __bis_SR_register(LPM0_bits + GIE);
//             continue;
//         }

//         if (hal.new_gyro && hal.dmp_on) {
//             short gyro[3], accel[3], sensors;
//             unsigned char more;
//             long quat[4];
//             /* This function gets new data from the FIFO when the DMP is in
//              * use. The FIFO can contain any combination of gyro, accel,
//              * quaternion, and gesture data. The sensors parameter tells the
//              * caller which data fields were actually populated with new data.
//              * For example, if sensors == (INV_XYZ_GYRO | INV_WXYZ_QUAT), then
//              * the FIFO isn't being filled with accel data.
//              * The driver parses the gesture data to determine if a gesture
//              * event has occurred; on an event, the application will be notified
//              * via a callback (assuming that a callback function was properly
//              * registered). The more parameter is non-zero if there are
//              * leftover packets in the FIFO.
//              */
//             dmp_read_fifo(gyro, accel, quat, &sensor_timestamp, &sensors,
//                 &more);
//             if (!more)
//                 hal.new_gyro = 0;
//             /* Gyro and accel data are written to the FIFO by the DMP in chip
//              * frame and hardware units. This behavior is convenient because it
//              * keeps the gyro and accel outputs of dmp_read_fifo and
//              * mpu_read_fifo consistent.
//              */
//             if (sensors & INV_XYZ_GYRO && hal.report & PRINT_GYRO)
//                 send_packet(PACKET_TYPE_GYRO, gyro);
//             if (sensors & INV_XYZ_ACCEL && hal.report & PRINT_ACCEL)
//                 send_packet(PACKET_TYPE_ACCEL, accel);
//             /* Unlike gyro and accel, quaternions are written to the FIFO in
//              * the body frame, q30. The orientation is set by the scalar passed
//              * to dmp_set_orientation during initialization.
//              */
//             if (sensors & INV_WXYZ_QUAT && hal.report & PRINT_QUAT)
//                 send_packet(PACKET_TYPE_QUAT, quat);
//         } else if (hal.new_gyro) {
//             short gyro[3], accel[3];
//             unsigned char sensors, more;
//             /* This function gets new data from the FIFO. The FIFO can contain
//              * gyro, accel, both, or neither. The sensors parameter tells the
//              * caller which data fields were actually populated with new data.
//              * For example, if sensors == INV_XYZ_GYRO, then the FIFO isn't
//              * being filled with accel data. The more parameter is non-zero if
//              * there are leftover packets in the FIFO.
//              */
//             mpu_read_fifo(gyro, accel, &sensor_timestamp, &sensors, &more);
//             if (!more)
//                 hal.new_gyro = 0;
//             if (sensors & INV_XYZ_GYRO && hal.report & PRINT_GYRO)
//                 send_packet(PACKET_TYPE_GYRO, gyro);
//             if (sensors & INV_XYZ_ACCEL && hal.report & PRINT_ACCEL)
//                 send_packet(PACKET_TYPE_ACCEL, accel);
//         }
//     }
// }
