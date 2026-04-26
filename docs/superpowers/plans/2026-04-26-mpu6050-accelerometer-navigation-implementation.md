# MPU6050 Accelerometer Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Switch `app_env_monitor` page routing from gyro-integrated heading to stable accelerometer-based face detection using the root-level `ATK_MS6050` driver.

**Architecture:** Retarget the `ATK_MS6050` low-level software-I2C pins away from the shared sensor bus, keep `HARDWARE/SENSOR/MPU6050.*` as the project-facing wrapper, then simplify `USER/app_env_monitor.c` so current-page selection depends only on `AX/AY` thresholds plus debounce and cooldown timing.

**Tech Stack:** STM32F103C8, StdPeriph GPIO, FreeRTOS, ARMCC5/Keil uVision, OLED framebuffer UI, ATK_MS6050 software-I2C driver.

---

### Task 1: Retarget the ATK driver and project file

**Files:**
- Modify: `ATK_MS6050/atk_ms6050_iic.h`
- Modify: `ATK_MS6050/atk_ms6050_iic.c`
- Modify: `USER/Template.uvprojx`

- [ ] **Step 1: Move ATK software-I2C off the shared environment-sensor bus**

```c
#define ATK_MS6050_IIC_SCL_GPIO_PORT    GPIOA
#define ATK_MS6050_IIC_SCL_GPIO_CLK     RCC_APB2Periph_GPIOA
#define ATK_MS6050_IIC_SCL_GPIO_PIN     GPIO_Pin_0

#define ATK_MS6050_IIC_SDA_GPIO_PORT    GPIOA
#define ATK_MS6050_IIC_SDA_GPIO_CLK     RCC_APB2Periph_GPIOA
#define ATK_MS6050_IIC_SDA_GPIO_PIN     GPIO_Pin_1
```

- [ ] **Step 2: Remove the hardcoded `GPIOB` init path so the new macros actually take effect**

```c
GPIO_Init(ATK_MS6050_IIC_SCL_GPIO_PORT, &gpio_init_struct);
```

- [ ] **Step 3: Add ATK source files to the Keil project and fix include paths**

```xml
<IncludePath>..\USER;..\CORE;..\STM32F10x_FWLib\inc;..\SYSTEM\sys;..\SYSTEM\delay;..\SYSTEM\usart;..\OBJ;..\FreeRTOS-Kernel\portable\RVDS\ARM_CM3;..\FreeRTOS-Kernel\include;..\HARDWARE\LED;..\HARDWARE\OLED;..\HARDWARE\SENSOR;..\ATK_MS6050;..\ATK_MS6050\eMPL</IncludePath>
```

- [ ] **Step 4: Verify the project file contains the new source entries**

Run: `rg -n "atk_ms6050.c|atk_ms6050_iic.c|..\\\\ATK_MS6050" USER\\Template.uvprojx`

Expected: paths point to `..\ATK_MS6050\...`, not `..\HARDWARE\ATK_MS6050\...`

### Task 2: Replace the local MPU6050 backend with an ATK wrapper

**Files:**
- Modify: `HARDWARE/SENSOR/MPU6050.c`
- Review: `HARDWARE/SENSOR/MPU6050.h`

- [ ] **Step 1: Replace the local bit-banged backend with a wrapper over ATK register access**

```c
#include "atk_ms6050.h"

static uint8_t mpu6050_map_status(uint8_t status)
{
    if (status == ATK_MS6050_EOK) return MPU6050_OK;
    if (status == ATK_MS6050_EID) return MPU6050_ERR_ID;
    return MPU6050_ERR_I2C;
}
```

- [ ] **Step 2: Keep the public API stable for `app_env_monitor.c`**

```c
uint8_t MPU6050_Init(void);
uint8_t MPU6050_ReadWhoAmI(uint8_t *who_am_i);
uint8_t MPU6050_ReadData(MPU6050_Data_t *data);
```

- [ ] **Step 3: Read one raw 14-byte sensor frame through ATK and convert it into existing fields**

```c
status = atk_ms6050_read(ATK_MS6050_IIC_ADDR, MPU_ACCEL_XOUTH_REG, 14, raw);
data->accel_x_g = (float)data->accel_x_raw / 16384.0f;
data->gyro_z_dps = (float)data->gyro_z_raw / 65.5f;
```

- [ ] **Step 4: Verify the wrapper no longer contains the old local software-I2C implementation**

Run: `rg -n "MPU6050_I2C_|GPIO_Mode_Out_OD|GPIO_Mode_IPU" HARDWARE\\SENSOR\\MPU6050.c`

Expected: no matches from the deleted custom backend

### Task 3: Convert page routing to pure accelerometer classification

**Files:**
- Modify: `USER/app_env_monitor.c`

- [ ] **Step 1: Remove `yaw` and gyro-bias state from the app context and macros**

```c
#define ENV_MONITOR_ACCEL_AXIS_TRIGGER_G    0.90f
#define ENV_MONITOR_ACCEL_CROSS_AXIS_MAX_G  0.35f
#define ENV_MONITOR_PAGE_STABLE_MS           200U
#define ENV_MONITOR_PAGE_COOLDOWN_MS         300U
```

- [ ] **Step 2: Add an accelerometer-only classifier**

```c
if ((data->accel_x_g >= ENV_MONITOR_ACCEL_AXIS_TRIGGER_G) &&
    (fabsf(data->accel_y_g) <= ENV_MONITOR_ACCEL_CROSS_AXIS_MAX_G))
{
    return ENV_PAGE_HOME;
}
```

- [ ] **Step 3: Update MPU polling so page changes use only the classifier plus debounce/cooldown**

```c
observed_page = env_monitor_classify_page_from_accel(&context->mpu6050_data);
dirty |= env_monitor_update_page_state(context, observed_page, now_tick);
```

- [ ] **Step 4: Simplify the test page and UART log to show `AX/AY/AZ` instead of `Y/GZ`**

```c
format_tagged_signed_1(context->line, "AX", context->mpu6050_data.accel_x_g, "g");
format_tagged_signed_1(context->line, "AY", context->mpu6050_data.accel_y_g, "g");
format_tagged_signed_1(context->line, "AZ", context->mpu6050_data.accel_z_g, "g");
```

- [ ] **Step 5: Update stale user-facing MPU wiring hints**

```c
printf("MPU READY PA0/PA1\r\n");
OLED_ShowString(28u, 50u, (u8 *)"PA0/PA1", 8, 1);
```

### Task 4: Static verification and handoff

**Files:**
- Review: `ATK_MS6050/atk_ms6050_iic.h`
- Review: `ATK_MS6050/atk_ms6050_iic.c`
- Review: `HARDWARE/SENSOR/MPU6050.c`
- Review: `USER/app_env_monitor.c`
- Review: `USER/Template.uvprojx`

- [ ] **Step 1: Check the final diff stays focused on the approved design**

Run: `git diff -- ATK_MS6050/atk_ms6050_iic.h ATK_MS6050/atk_ms6050_iic.c HARDWARE/SENSOR/MPU6050.c USER/app_env_monitor.c USER/Template.uvprojx`

Expected: only ATK integration and accelerometer-page-routing changes appear

- [ ] **Step 2: Check for obvious project-file or patch corruption**

Run: `git diff --check`

Expected: no whitespace or merge-marker errors in edited files

- [ ] **Step 3: Rebuild in Keil if available**

Run: `UV4.exe -b USER\\Template.uvprojx -j0`

Expected: successful build output or a clear note that the local environment lacks Keil CLI
