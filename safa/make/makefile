################################################################################
# Makefile creado a partir de Eclipse
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: safa

dependents:
	-cd ../../teklib/Debug && $(MAKE) clean all

# Tool invocations
safa: $(OBJS) $(USER_OBJS) ../../teklib/Debug/libteklib.so
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -L"../../teklib/Debug" -o "safa" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) safa
	-@echo ' '

.PHONY: all clean dependents
../../teklib/Debug/libteklib.so:

-include ../makefile.targets
