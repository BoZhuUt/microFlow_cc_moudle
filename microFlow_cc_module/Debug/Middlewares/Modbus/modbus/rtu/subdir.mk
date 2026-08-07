################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Modbus/modbus/rtu/mbcrc.c \
../Middlewares/Modbus/modbus/rtu/mbrtu.c 

OBJS += \
./Middlewares/Modbus/modbus/rtu/mbcrc.o \
./Middlewares/Modbus/modbus/rtu/mbrtu.o 

C_DEPS += \
./Middlewares/Modbus/modbus/rtu/mbcrc.d \
./Middlewares/Modbus/modbus/rtu/mbrtu.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Modbus/modbus/rtu/%.o Middlewares/Modbus/modbus/rtu/%.su Middlewares/Modbus/modbus/rtu/%.cyclo: ../Middlewares/Modbus/modbus/rtu/%.c Middlewares/Modbus/modbus/rtu/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L432xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Middlewares/Modbus -I../Middlewares/Modbus/port -I../Middlewares/Modbus/modbus/include -I../Middlewares/Modbus/modbus/rtu -I../Middlewares/autoRegmap -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-Modbus-2f-modbus-2f-rtu

clean-Middlewares-2f-Modbus-2f-modbus-2f-rtu:
	-$(RM) ./Middlewares/Modbus/modbus/rtu/mbcrc.cyclo ./Middlewares/Modbus/modbus/rtu/mbcrc.d ./Middlewares/Modbus/modbus/rtu/mbcrc.o ./Middlewares/Modbus/modbus/rtu/mbcrc.su ./Middlewares/Modbus/modbus/rtu/mbrtu.cyclo ./Middlewares/Modbus/modbus/rtu/mbrtu.d ./Middlewares/Modbus/modbus/rtu/mbrtu.o ./Middlewares/Modbus/modbus/rtu/mbrtu.su

.PHONY: clean-Middlewares-2f-Modbus-2f-modbus-2f-rtu

