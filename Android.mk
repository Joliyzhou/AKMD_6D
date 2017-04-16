# TODO: Change device number!

#export AKMD_DEVICE_TYPE=8963
#export AKMD_DEVICE_TYPE=8975
export AKMD_DEVICE_TYPE=9911
#export AKMD_SENSOR_ACC=adxl346
#export AKMD_SENSOR_ACC=kxtf9
#export AKMD_SENSOR_ACC=dummy

# value should be yes/no
export AKMD_ACC_AOT=yes

ifeq ($(AKMD_ACC_AOT),yes)
akm_modules := akmd_6D  
else
akm_modules := akmd_6D  libsensors_6D
endif

include $(call all-named-subdir-makefiles,$(akm_modules))

