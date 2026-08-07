################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Modbus/port/portevent.c \
../Middlewares/Modbus/port/portserial.c \
../Middlewares/Modbus/port/porttimer.c 

OBJS += \
./Middlewares/Modbus/port/portevent.o \
./Middlewares/Modbus/port/portserial.o \
./Middlewares/Modbus/port/porttimer.o 

C_DEPS += \
./Middlewares/Modbus/port/portevent.d \
./Middlewares/Modbus/port/portserial.d \
./Middlewares/Modbus/port/porttimer.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Modbus/port/%.o Middlewares/Modbus/port/%.su Middlewares/Modbus/port/%.cyclo: ../Middlewares/Modbus/port/%.c Middlewares/Modbus/port/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L432xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Middlewares/Modbus -I../Middlewares/Modbus/port -I../Middlewares/Modbus/modbus/include -I../Middlewares/Modbus/modbus/rtu -I../Middlewares/autoRegmap -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-Modbus-2f-port

clean-Middlewares-2f-Modbus-2f-port:
	-$(RM) ./Middlewares/Modbus/port/portevent.cyclo ./Middlewares/Modbus/port/portevent.d ./Middlewares/Modbus/port/portevent.o ./Middlewares/Modbus/port/portevent.su ./Middlewares/Modbus/port/portserial.cyclo ./Middlewares/Modbus/port/portserial.d ./Middlewares/Modbus/port/portserial.o ./Middlewares/Modbus/port/portserial.su ./Middlewares/Modbus/port/porttimer.cyclo ./Middlewares/Modbus/port/porttimer.d ./Middlewares/Modbus/port/porttimer.o ./Middlewares/Modbus/port/porttimer.su

.PHONY: clean-Middlewares-2f-Modbus-2f-port

