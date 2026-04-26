#ifndef __ATK_MS6050_H
#define __ATK_MS6050_H

#include "sys.h"

#define ATK_MS6050_AD0_GPIO_PORT        GPIOC
#define ATK_MS6050_AD0_GPIO_CLK         RCC_APB2Periph_GPIOC
#define ATK_MS6050_AD0_GPIO_PIN         GPIO_Pin_0
#define ATK_MS6050_AD0_GPIO_NUM         0

#define ATK_MS6050_INT_GPIO_PORT        GPIOB
#define ATK_MS6050_INT_GPIO_CLK         RCC_APB2Periph_GPIOB
#define ATK_MS6050_INT_GPIO_PIN         GPIO_Pin_12
#define ATK_MS6050_INT_GPIO_NUM         12
#define ATK_MS6050_INT_PORT_SOURCE      GPIO_PortSourceGPIOB
#define ATK_MS6050_INT_PIN_SOURCE       GPIO_PinSource12
#define ATK_MS6050_INT_EXTI_LINE        EXTI_Line12
#define ATK_MS6050_INT_IRQn             EXTI15_10_IRQn

#define ATK_MS6050_AD0(x)               do { PCout(ATK_MS6050_AD0_GPIO_NUM) = ((x) ? 1 : 0); } while (0)
#define ATK_MS6050_INT_READ()           PBin(ATK_MS6050_INT_GPIO_NUM)

#define ATK_MS6050_IIC_ADDR             0x68
#define ATK_MS6050_DEVICE_ID            0x68
#define ATK_MS6050_DEVICE_ID_ALT        0x70
/* 0: sensor INT not connected, use FIFO polling. 1: INT connected to PB12. */
#define ATK_MS6050_USE_INT              0

#define MPU_ACCEL_OFFS_REG      0x06
#define MPU_PROD_ID_REG         0x0C
#define MPU_SELF_TESTX_REG      0x0D
#define MPU_SELF_TESTY_REG      0x0E
#define MPU_SELF_TESTZ_REG      0x0F
#define MPU_SELF_TESTA_REG      0x10
#define MPU_SAMPLE_RATE_REG     0x19
#define MPU_CFG_REG             0x1A
#define MPU_GYRO_CFG_REG        0x1B
#define MPU_ACCEL_CFG_REG       0x1C
#define MPU_MOTION_DET_REG      0x1F
#define MPU_FIFO_EN_REG         0x23
#define MPU_I2CMST_CTRL_REG     0x24
#define MPU_I2CSLV0_ADDR_REG    0x25
#define MPU_I2CSLV0_REG         0x26
#define MPU_I2CSLV0_CTRL_REG    0x27
#define MPU_I2CSLV1_ADDR_REG    0x28
#define MPU_I2CSLV1_REG         0x29
#define MPU_I2CSLV1_CTRL_REG    0x2A
#define MPU_I2CSLV2_ADDR_REG    0x2B
#define MPU_I2CSLV2_REG         0x2C
#define MPU_I2CSLV2_CTRL_REG    0x2D
#define MPU_I2CSLV3_ADDR_REG    0x2E
#define MPU_I2CSLV3_REG         0x2F
#define MPU_I2CSLV3_CTRL_REG    0x30
#define MPU_I2CSLV4_ADDR_REG    0x31
#define MPU_I2CSLV4_REG         0x32
#define MPU_I2CSLV4_DO_REG      0x33
#define MPU_I2CSLV4_CTRL_REG    0x34
#define MPU_I2CSLV4_DI_REG      0x35
#define MPU_I2CMST_STA_REG      0x36
#define MPU_INTBP_CFG_REG       0x37
#define MPU_INT_EN_REG          0x38
#define MPU_INT_STA_REG         0x3A
#define MPU_ACCEL_XOUTH_REG     0x3B
#define MPU_ACCEL_XOUTL_REG     0x3C
#define MPU_ACCEL_YOUTH_REG     0x3D
#define MPU_ACCEL_YOUTL_REG     0x3E
#define MPU_ACCEL_ZOUTH_REG     0x3F
#define MPU_ACCEL_ZOUTL_REG     0x40
#define MPU_TEMP_OUTH_REG       0x41
#define MPU_TEMP_OUTL_REG       0x42
#define MPU_GYRO_XOUTH_REG      0x43
#define MPU_GYRO_XOUTL_REG      0x44
#define MPU_GYRO_YOUTH_REG      0x45
#define MPU_GYRO_YOUTL_REG      0x46
#define MPU_GYRO_ZOUTH_REG      0x47
#define MPU_GYRO_ZOUTL_REG      0x48
#define MPU_I2CSLV0_DO_REG      0x63
#define MPU_I2CSLV1_DO_REG      0x64
#define MPU_I2CSLV2_DO_REG      0x65
#define MPU_I2CSLV3_DO_REG      0x66
#define MPU_I2CMST_DELAY_REG    0x67
#define MPU_SIGPATH_RST_REG     0x68
#define MPU_MDETECT_CTRL_REG    0x69
#define MPU_USER_CTRL_REG       0x6A
#define MPU_PWR_MGMT1_REG       0x6B
#define MPU_PWR_MGMT2_REG       0x6C
#define MPU_FIFO_CNTH_REG       0x72
#define MPU_FIFO_CNTL_REG       0x73
#define MPU_FIFO_RW_REG         0x74
#define MPU_DEVICE_ID_REG       0x75

#define ATK_MS6050_EOK          0
#define ATK_MS6050_EID          1
#define ATK_MS6050_EACK         2
#define ATK_MS6050_EDMP         3
#define ATK_MS6050_ESELFTEST    4

void atk_ms6050_board_init(void);
void atk_ms6050_int_exti_init(void);
void atk_ms6050_int_exti_cmd(FunctionalState state);
void atk_ms6050_int_exti_clear_flag(void);
uint8_t atk_ms6050_int_pin_read(void);

uint8_t atk_ms6050_write(uint8_t addr, uint8_t reg, uint8_t len, uint8_t *dat);
uint8_t atk_ms6050_write_byte(uint8_t addr, uint8_t reg, uint8_t dat);
uint8_t atk_ms6050_read(uint8_t addr, uint8_t reg, uint8_t len, uint8_t *dat);
uint8_t atk_ms6050_read_byte(uint8_t addr, uint8_t reg, uint8_t *dat);
uint8_t atk_ms6050_get_device_id(uint8_t *id);
uint8_t atk_ms6050_get_last_device_id(void);
void atk_ms6050_clear_last_i2c_error(void);
uint8_t atk_ms6050_get_last_i2c_error_phase(void);
uint8_t atk_ms6050_get_last_i2c_error_dir(void);
uint8_t atk_ms6050_get_last_i2c_error_reg(void);
uint8_t atk_ms6050_get_last_i2c_error_index(void);
uint8_t atk_ms6050_device_check(void);
void atk_ms6050_sw_reset(void);
uint8_t atk_ms6050_set_gyro_fsr(uint8_t fsr);
uint8_t atk_ms6050_set_accel_fsr(uint8_t fsr);
uint8_t atk_ms6050_set_lpf(uint16_t lpf);
uint8_t atk_ms6050_set_rate(uint16_t rate);
uint8_t atk_ms6050_get_temperature(int16_t *temp);
uint8_t atk_ms6050_get_gyroscope(int16_t *gx, int16_t *gy, int16_t *gz);
uint8_t atk_ms6050_get_accelerometer(int16_t *ax, int16_t *ay, int16_t *az);
uint8_t atk_ms6050_init(void);

#endif
