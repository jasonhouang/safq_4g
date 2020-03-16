BSP_ROOT ?= /home/jason/Workspaces/embedded/safq_4g/bsp/stm32/stm32f407-4g-ec20
RTT_ROOT ?= /home/jason/Workspaces/embedded/safq_4g

CROSS_COMPILE ?=/opt/gcc-arm-none-eabi-6_2-2016q4/bin/arm-none-eabi-

CFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -Dgcc -O0 -gdwarf-2 -g
AFLAGS := -c -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -x assembler-with-cpp -Wa,-mimplicit-it=thumb  -gdwarf-2
LFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -Wl,--gc-sections,-Map=rt-thread.map,-cref,-u,Reset_Handler -T board/linker_scripts/link.lds
CXXFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -Dgcc -O0 -gdwarf-2 -g

CPPPATHS :=-I$(RTT_ROOT)/bsp/stm32/libraries/HAL_Drivers \
	-I$(RTT_ROOT)/bsp/stm32/libraries/HAL_Drivers/config \
	-I$(RTT_ROOT)/bsp/stm32/libraries/HAL_Drivers/drv_flash \
	-I$(RTT_ROOT)/bsp/stm32/libraries/STM32F4xx_HAL/CMSIS/Device/ST/STM32F4xx/Include \
	-I$(RTT_ROOT)/bsp/stm32/libraries/STM32F4xx_HAL/CMSIS/Include \
	-I$(RTT_ROOT)/bsp/stm32/libraries/STM32F4xx_HAL/STM32F4xx_HAL_Driver/Inc \
	-I$(BSP_ROOT) \
	-I$(BSP_ROOT)/applications \
	-I$(BSP_ROOT)/board \
	-I$(BSP_ROOT)/board/CubeMX_Config/Inc \
	-I$(BSP_ROOT)/board/ports \
	-I$(BSP_ROOT)/packages/SystemView-latest \
	-I$(BSP_ROOT)/packages/SystemView-latest/SystemView_Src/Config \
	-I$(BSP_ROOT)/packages/SystemView-latest/SystemView_Src/SEGGER \
	-I$(BSP_ROOT)/packages/ali-iotkit-v3.0.1/iotkit-embedded/src/dev_model \
	-I$(BSP_ROOT)/packages/ali-iotkit-v3.0.1/iotkit-embedded/src/dev_model/client \
	-I$(BSP_ROOT)/packages/ali-iotkit-v3.0.1/iotkit-embedded/src/dev_model/server \
	-I$(BSP_ROOT)/packages/ali-iotkit-v3.0.1/iotkit-embedded/src/dev_sign \
	-I$(BSP_ROOT)/packages/ali-iotkit-v3.0.1/iotkit-embedded/src/infra \
	-I$(BSP_ROOT)/packages/ali-iotkit-v3.0.1/iotkit-embedded/src/mqtt \
	-I$(BSP_ROOT)/packages/ali-iotkit-v3.0.1/iotkit-embedded/src/mqtt/impl \
	-I$(BSP_ROOT)/packages/ali-iotkit-v3.0.1/iotkit-embedded/wrappers \
	-I$(BSP_ROOT)/packages/at_device-v2.0.1/class/ec20 \
	-I$(BSP_ROOT)/packages/at_device-v2.0.1/inc \
	-I$(BSP_ROOT)/packages/cJSON-v1.0.2 \
	-I$(BSP_ROOT)/packages/fal-latest/inc \
	-I$(BSP_ROOT)/packages/mbedtls-v2.7.10/mbedtls/include \
	-I$(BSP_ROOT)/packages/mbedtls-v2.7.10/ports/inc \
	-I$(BSP_ROOT)/packages/ota_downloader-latest \
	-I$(RTT_ROOT)/components/dfs/filesystems/devfs \
	-I$(RTT_ROOT)/components/dfs/include \
	-I$(RTT_ROOT)/components/drivers/include \
	-I$(RTT_ROOT)/components/drivers/spi \
	-I$(RTT_ROOT)/components/drivers/spi/sfud/inc \
	-I$(RTT_ROOT)/components/finsh \
	-I$(RTT_ROOT)/components/libc/compilers/newlib \
	-I$(RTT_ROOT)/components/net/at/at_socket \
	-I$(RTT_ROOT)/components/net/at/include \
	-I$(RTT_ROOT)/components/net/netdev/include \
	-I$(RTT_ROOT)/components/net/sal_socket/impl \
	-I$(RTT_ROOT)/components/net/sal_socket/include \
	-I$(RTT_ROOT)/components/net/sal_socket/include/dfs_net \
	-I$(RTT_ROOT)/components/net/sal_socket/include/socket \
	-I$(RTT_ROOT)/components/net/sal_socket/include/socket/sys_socket \
	-I$(RTT_ROOT)/components/utilities/ymodem \
	-I$(RTT_ROOT)/include \
	-I$(RTT_ROOT)/libcpu/arm/common \
	-I$(RTT_ROOT)/libcpu/arm/cortex-m4 

DEFINES := -DHAVE_CCONFIG_H -DRT_USING_NEWLIB -DSTM32F407xx -DUSE_HAL_DRIVER
