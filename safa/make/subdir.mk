################################################################################
# Makefile creado a partir de Eclipse
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/safa.c 

OBJS += \
./src/safa.o 

C_DEPS += \
./src/safa.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"../../teklib" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


