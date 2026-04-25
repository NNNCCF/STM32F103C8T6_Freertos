#ifndef __APP_I2C_H
#define __APP_I2C_H

#include "stm32f10x.h"

/* I2C1 default: PB6=SCL, PB7=SDA */
#define APP_I2C_INSTANCE            I2C1
#define APP_I2C_RCC_APB1            RCC_APB1Periph_I2C1
#define APP_I2C_RCC_APB2_GPIO       RCC_APB2Periph_GPIOB
#define APP_I2C_GPIO_PORT           GPIOB
#define APP_I2C_SCL_PIN             GPIO_Pin_6
#define APP_I2C_SDA_PIN             GPIO_Pin_7
#define APP_I2C_SPEED               100000u

void APP_I2C_Init(void);

uint8_t APP_I2C_Write(uint8_t dev_addr_7bit, const uint8_t *data, uint8_t len);
uint8_t APP_I2C_Read(uint8_t dev_addr_7bit, uint8_t *data, uint8_t len);
uint8_t APP_I2C_WriteBytes(uint8_t dev_addr_7bit, uint8_t reg, const uint8_t *data, uint8_t len);
uint8_t APP_I2C_ReadBytes(uint8_t dev_addr_7bit, uint8_t reg, uint8_t *data, uint8_t len);
uint8_t APP_I2C_WriteByte(uint8_t dev_addr_7bit, uint8_t reg, uint8_t data);
uint8_t APP_I2C_ReadByte(uint8_t dev_addr_7bit, uint8_t reg, uint8_t *data);

#endif
