TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += execute

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install execute.dbd into <top>/dbd
DBD += execute.dbd

# specify all source files to be compiled and added to the library
execute_SRCS += Command.cpp
execute_SRCS += CommandRegistry.cpp
execute_SRCS += RecordAddress.cpp
execute_SRCS += errorPrint.cpp
execute_SRCS += recordDeviceSupportDefinitions.cpp
execute_SRCS += registrar.cpp

execute_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

