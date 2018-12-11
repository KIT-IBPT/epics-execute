/*
 * Copyright 2018 aquenos GmbH.
 * Copyright 2018 Karlsruhe Institute of Technology.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * This software has been developed by aquenos GmbH on behalf of the
 * Karlsruhe Institute of Technology's Institute for Beam Physics and
 * Technology.
 *
 * This software contains code originally developed by aquenos GmbH for
 * the s7nodave EPICS device support. aquenos GmbH has relicensed the
 * affected portions of code from the s7nodave EPICS device support
 * (originally licensed under the terms of the GNU GPL) under the terms
 * of the GNU LGPL version 3 or newer.
 */

#include <stdexcept>
#include <system_error>

#include <aaiRecord.h>
#include <aaoRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <mbbiDirectRecord.h>
#include <mbbiRecord.h>
#include <mbboDirectRecord.h>
#include <mbboRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <dbCommon.h>
#include <dbScan.h>
#include <devSup.h>
#include <epicsExport.h>

#include "AaiDeviceSupport.h"
#include "AaoOutputParameterDeviceSupport.h"
#include "AaoStdInDeviceSupport.h"
#include "ExitCodeDeviceSupport.h"
#include "OutputParameterDeviceSupport.h"
#include "RecordAddress.h"
#include "RunDeviceSupport.h"
#include "StringinDeviceSupport.h"
#include "StringoutStdInDeviceSupport.h"
#include "errorPrint.h"

using namespace epics::execute;

namespace {

/**
 * Factory for creating the device support for an aao record. Depending on the
 * type specified in the record's address, this factory creates an
 * AaoStdInDeviceSupport or an AaoOutputParameterDeviceSupport.
 */
struct AaoDeviceSupportFactory {
  static BaseDeviceSupport<::aaoRecord> *createDeviceSupport(
      ::aaoRecord *record) {
    auto address = RecordAddress::parse(record->out,
        RecordAddress::Type::argument | RecordAddress::Type::envVar
        | RecordAddress::Type::standardInput);
    if (address.getType() == RecordAddress::Type::standardInput) {
      return new AaoStdInDeviceSupport(record, address);
    } else {
      return new AaoOutputParameterDeviceSupport(record, address);
    }
  }
};

/**
 * Factory for creating the device support for a bo record. Depending on the
 * type specified in the record's address, this factory creates an
 * OutputParameterDeviceSupport or a RunDeviceSupport.
 */
struct BoDeviceSupportFactory {
  static BaseDeviceSupport<::boRecord> *createDeviceSupport(
      ::boRecord *record) {
    auto address = RecordAddress::parse(record->out,
        RecordAddress::Type::argument | RecordAddress::Type::envVar
            | RecordAddress::Type::run);
    if (address.getType() == RecordAddress::Type::run) {
      return new RunDeviceSupport<::boRecord>(record, address);
    } else {
      return new OutputParameterDeviceSupport<::boRecord, RecordValFieldName::rval>(record, address, false);
    }
  }
};

/**
 * Factory for creating the ExitCodeDeviceSupport.
 */
template<typename RecordType, RecordValFieldName ValFieldName>
struct ExitCodeDeviceSupportFactory {
  static BaseDeviceSupport<RecordType> *createDeviceSupport(
      RecordType *record) {
    auto address = RecordAddress::parse(record->inp, RecordAddress::Type::exitCode);
    return new ExitCodeDeviceSupport<RecordType, ValFieldName>(record, address);
  }
};

/**
 * Factory for creating the AaoDeviceSupport and StringoutDeviceSupport. These
 * two device supports are largely similar and only differ in internal
 * implementation details.
 */
template<typename RecordType, typename DeviceSupportType>
struct OutputDeviceSupportFactory {
  static BaseDeviceSupport<RecordType> *createDeviceSupport(
      RecordType *record) {
    auto address = RecordAddress::parse(record->inp,
        RecordAddress::Type::standardError
        | RecordAddress::Type::standardOutput);
    return new DeviceSupportType(record, address);
  }
};

/**
 * Factory for creating the OutputParameterDeviceSupport.
 */
template<typename RecordType, RecordValFieldName ValFieldName, bool NoConvert=false>
struct OutputParameterDeviceSupportFactory {
  static BaseDeviceSupport<RecordType> *createDeviceSupport(
      RecordType *record) {
    auto address = RecordAddress::parse(record->out,
        RecordAddress::Type::argument | RecordAddress::Type::envVar);
    return new OutputParameterDeviceSupport<RecordType, ValFieldName>(record,
        address, NoConvert);
  }
};

/**
 * Factory for creating the device support for a stringout record. Depending on
 * the type specified in the record's address, this factory creates an
 * StringoutStdInDeviceSupport or an OutputParameterDeviceSupport.
 */
struct StringoutDeviceSupportFactory {
  static BaseDeviceSupport<::stringoutRecord> *createDeviceSupport(
      ::stringoutRecord *record) {
    auto address = RecordAddress::parse(record->out,
        RecordAddress::Type::argument | RecordAddress::Type::envVar
        | RecordAddress::Type::standardInput);
    if (address.getType() == RecordAddress::Type::standardInput) {
      return new StringoutStdInDeviceSupport(record, address);
    } else {
      return new OutputParameterDeviceSupport<::stringoutRecord, RecordValFieldName::val>(
          record, address, false);
    }
  }
};

/**
 * Template declaration for mapping record types to device support factories.
 * Different records use different device support factories because the logic
 * that chooses the device support for a record depends on the record type.
 * Most records only have a single device support associated with them, but
 * some records (in particular the bo record) uses different device supports
 * based on the address.
 */
template<typename RecordType>
struct DeviceSupportFactories;

auto constexpr valField = RecordValFieldName::val;
auto constexpr rvalField = RecordValFieldName::rval;

/**
 * Template specialzation for the aai record.
 */
template<>
struct DeviceSupportFactories<::aaiRecord> {
  using Factory = OutputDeviceSupportFactory<::aaiRecord, AaiDeviceSupport>;
};

/**
 * Template specialzation for the aao record.
 */
template<>
struct DeviceSupportFactories<::aaoRecord> {
  using Factory = AaoDeviceSupportFactory;
};

/**
 * Template specialzation for the ao record.
 */
template<>
struct DeviceSupportFactories<::aoRecord> {
  using Factory = OutputParameterDeviceSupportFactory<::aoRecord, valField, true>;
};

/**
 * Template specialzation for the bi record.
 */
template<>
struct DeviceSupportFactories<::biRecord> {
  using Factory = ExitCodeDeviceSupportFactory<::biRecord, rvalField>;
};

/**
 * Template specialzation for the bo record.
 */
template<>
struct DeviceSupportFactories<::boRecord> {
  using Factory = BoDeviceSupportFactory;
};

/**
 * Template specialzation for the longin record.
 */
template<>
struct DeviceSupportFactories<::longinRecord> {
  using Factory = ExitCodeDeviceSupportFactory<::longinRecord, valField>;
};

/**
 * Template specialzation for the longout record.
 */
template<>
struct DeviceSupportFactories<::longoutRecord> {
  using Factory = OutputParameterDeviceSupportFactory<::longoutRecord, valField>;
};

/**
 * Template specialzation for the mbbi record.
 */
template<>
struct DeviceSupportFactories<::mbbiRecord> {
  using Factory = ExitCodeDeviceSupportFactory<::mbbiRecord, rvalField>;
};

/**
 * Template specialzation for the mbbiDirect record.
 */
template<>
struct DeviceSupportFactories<::mbbiDirectRecord> {
  using Factory = ExitCodeDeviceSupportFactory<::mbbiDirectRecord, rvalField>;
};

/**
 * Template specialzation for the mbbi record.
 */
template<>
struct DeviceSupportFactories<::mbboRecord> {
  using Factory = OutputParameterDeviceSupportFactory<::mbboRecord, rvalField>;
};

/**
 * Template specialzation for the mbbiDirect record.
 */
template<>
struct DeviceSupportFactories<::mbboDirectRecord> {
  using Factory =
      OutputParameterDeviceSupportFactory<::mbboDirectRecord, rvalField>;
};

/**
 * Template specialzation for the stringin record.
 */
template<>
struct DeviceSupportFactories<::stringinRecord> {
  using Factory =
      OutputDeviceSupportFactory<::stringinRecord, StringinDeviceSupport>;
};

/**
 * Template specialzation for the stringout record.
 */
template<>
struct DeviceSupportFactories<::stringoutRecord> {
  using Factory = StringoutDeviceSupportFactory;
};

/**
 * Creates the device support instance and registers it with the record.
 */
template<typename RecordType, bool ConsiderNoConvert=false>
long initRecord(void *recordVoid) noexcept {
  if (!recordVoid) {
    errorExtendedPrintf(
        "Record initialization failed: Pointer to record structure is null.");
    return -1;
  }
  bool noConvert;
  auto record = static_cast<RecordType *>(recordVoid);
  try {
    BaseDeviceSupport<RecordType> *deviceSupport =
        DeviceSupportFactories<RecordType>::Factory::createDeviceSupport(record);
    record->dpvt = deviceSupport;
    noConvert = deviceSupport->isNoConvert();
  } catch (std::exception &e) {
    record->dpvt = nullptr;
    errorExtendedPrintf("%s Record initialization failed: %s", record->name,
        e.what());
    return -1;
  } catch (...) {
    record->dpvt = nullptr;
    errorExtendedPrintf("%s Record initialization failed: Unknown error.",
        record->name);
    return -1;
  }
  if (ConsiderNoConvert && noConvert) {
    return 2;
  } else {
    return 0;
  }
}

/**
 * Handles processing of the record.
 */
template<typename RecordType, bool ConsiderNoConvert=false>
long processRecord(void *recordVoid) noexcept {
  if (!recordVoid) {
    errorExtendedPrintf(
        "Record processing failed: Pointer to record structure is null.");
    return -1;
  }
  bool noConvert;
  auto record = static_cast<RecordType *>(recordVoid);
  try {
    auto deviceSupport =
        static_cast<BaseDeviceSupport<RecordType> *>(record->dpvt);
    if (!deviceSupport) {
      throw std::runtime_error(
          "Pointer to device support data structure is null.");
    }
    noConvert = deviceSupport->isNoConvert();
    deviceSupport->processRecord();
  } catch (std::exception &e) {
    errorExtendedPrintf("%s Record processing failed: %s", record->name,
        e.what());
    return -1;
  } catch (...) {
    errorExtendedPrintf("%s Record processing failed: Unknown error.",
        record->name);
    return -1;
  }
  if (ConsiderNoConvert && noConvert) {
    return 2;
  } else {
    return 0;
  }
}

/**
 * Type alias for the get_ioint_info functions. These functions have a slightly
 * different signature than the other functions, even though the definition in
 * the structures in the record header files might indicate something else.
 */
typedef long (*DEVSUPFUN_GET_IOINT_INFO)(int, ::dbCommon *, ::IOSCANPVT *);

/**
 * Device support structure as expected by most record types. The notable
 * exceptions are the ai and ao records, where the device support structure
 * has an additional field.
 */
typedef struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN process;
} DeviceSupportStruct;

template<typename RecordType>
constexpr DeviceSupportStruct deviceSupportStruct() {
  return {5, nullptr, nullptr, initRecord<RecordType>, nullptr,
      processRecord<RecordType>};
}

} // anonymous namespace

extern "C" {

/**
 * aai record type.
 */
auto devAaiExecute = deviceSupportStruct<::aaiRecord>();
epicsExportAddress(dset, devAaiExecute);

/**
 * aao record type.
 */
auto devAaoExecute = deviceSupportStruct<::aaoRecord>();
epicsExportAddress(dset, devAaoExecute);

/**
 * ao record type.  This record type expects an additional field
 * (special_linconv) in the device support structure, so we cannot use the usual
 * template function.
 */
struct {
  long numberOfFunctionPointers;
  DEVSUPFUN report;
  DEVSUPFUN init;
  DEVSUPFUN init_record;
  DEVSUPFUN_GET_IOINT_INFO get_ioint_info;
  DEVSUPFUN write;
  DEVSUPFUN special_linconv;
} devAoExecute = {6, nullptr, nullptr, initRecord<::aoRecord, true>, nullptr,
    processRecord<::aoRecord>, nullptr};
epicsExportAddress(dset, devAoExecute);

/**
 * bi record type.
 */
auto devBiExecute = deviceSupportStruct<::biRecord>();
epicsExportAddress(dset, devBiExecute);

/**
 * bo record type.
 */
auto devBoExecute = deviceSupportStruct<::boRecord>();
epicsExportAddress(dset, devBoExecute);

/**
 * longin record type.
 */
auto devLonginExecute = deviceSupportStruct<::longinRecord>();
epicsExportAddress(dset, devLonginExecute);

/**
 * longout record type.
 */
auto devLongoutExecute = deviceSupportStruct<::longoutRecord>();
epicsExportAddress(dset, devLongoutExecute);

/**
 * mbbi record type.
 */
auto devMbbiExecute = deviceSupportStruct<::mbbiRecord>();
epicsExportAddress(dset, devMbbiExecute);

/**
 * mbbiDirect record type.
 */
auto devMbbiDirectExecute = deviceSupportStruct<::mbbiDirectRecord>();
epicsExportAddress(dset, devMbbiDirectExecute);

/**
 * mbbo record type.
 */
auto devMbboExecute = deviceSupportStruct<::mbboRecord>();
epicsExportAddress(dset, devMbboExecute);

/**
 * mbboDirect record type.
 */
auto devMbboDirectExecute = deviceSupportStruct<::mbboDirectRecord>();
epicsExportAddress(dset, devMbboDirectExecute);

/**
 * stringin record type.
 */
auto devStringinExecute = deviceSupportStruct<::stringinRecord>();
epicsExportAddress(dset, devStringinExecute);

/**
 * stringout record type.
 */
auto devStringoutExecute = deviceSupportStruct<::stringoutRecord>();
epicsExportAddress(dset, devStringoutExecute);

} // extern "C"
