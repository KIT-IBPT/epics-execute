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
DBDCAT += execute.dbd

execute_DBD += common.dbd
# All EPICS Base release since version 3.16 support the lsi and lso records.
ifdef BASE_3_16
execute_DBD += longString.dbd
endif
# The lsi and lso records are also available in 3.15 starting with 3.15.0.2.
# We cannot easily check for a certain patch level, so we enable these records
# for all versions starting with 3.15.1.
ifeq (BASE_3_15, YES)
ifneq (EPICS_MODIFICATION, 0)
execute_DBD += longString.dbd
endif
endif

# specify all source files to be compiled and added to the library
execute_SRCS += Command.cpp
execute_SRCS += CommandRegistry.cpp
execute_SRCS += RecordAddress.cpp
execute_SRCS += ThreadPoolExecutor.cpp
execute_SRCS += errorPrint.cpp
execute_SRCS += recordDeviceSupportDefinitions.cpp
execute_SRCS += registrar.cpp

execute_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

