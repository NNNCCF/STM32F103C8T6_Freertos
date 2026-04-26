#ifndef __ATK_MS6050_IIC_H
#define __ATK_MS6050_IIC_H

#include "sys.h"

#define ATK_MS6050_IIC_SCL_GPIO_PORT    GPIOA
#define ATK_MS6050_IIC_SCL_GPIO_CLK     RCC_APB2Periph_GPIOA
#define ATK_MS6050_IIC_SCL_GPIO_PIN     GPIO_Pin_0
#define ATK_MS6050_IIC_SCL_GPIO_NUM     0

#define ATK_MS6050_IIC_SDA_GPIO_PORT    GPIOA
#define ATK_MS6050_IIC_SDA_GPIO_CLK     RCC_APB2Periph_GPIOA
#define ATK_MS6050_IIC_SDA_GPIO_PIN     GPIO_Pin_1
#define ATK_MS6050_IIC_SDA_GPIO_NUM     1

#define ATK_MS6050_IIC_SCL(x)           do { PAout(ATK_MS6050_IIC_SCL_GPIO_NUM) = ((x) ? 1 : 0); } while (0)
#define ATK_MS6050_IIC_SDA(x)           do { PAout(ATK_MS6050_IIC_SDA_GPIO_NUM) = ((x) ? 1 : 0); } while (0)
#define ATK_MS6050_IIC_READ_SDA()       PAin(ATK_MS6050_IIC_SDA_GPIO_NUM)

void atk_ms6050_iic_start(void);
void atk_ms6050_iic_stop(void);
uint8_t atk_ms6050_iic_wait_ack(void);
void atk_ms6050_iic_ack(void);
void atk_ms6050_iic_nack(void);
void atk_ms6050_iic_send_byte(uint8_t dat);
uint8_t atk_ms6050_iic_read_byte(uint8_t ack);
void atk_ms6050_iic_init(void);

#endif
