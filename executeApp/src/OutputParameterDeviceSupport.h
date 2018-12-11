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

#ifndef EPICS_EXEC_OUTPUT_PARAMETER_DEVICE_SUPPORT_H
#define EPICS_EXEC_OUTPUT_PARAMETER_DEVICE_SUPPORT_H

#include <sstream>
#include <stdexcept>
#include <type_traits>

#include "BaseDeviceSupport.h"
#include "RecordValFieldName.h"

namespace epics {
namespace execute {

/**
 * Base class for the device support classes of most output records.
 *
 * There are only two notable exceptions: The bo record when used in the "run"
 * mode and the aao record. These two records need special handling and thus are
 * not derived from this class.
 *
 * This device support code only handles record address of type argument or
 * envVar.
 */
template <typename RecordType, RecordValFieldName ValFieldName>
class OutputParameterDeviceSupport : public BaseDeviceSupport<RecordType> {

public:

  /**
   * Constructor. The parameters are passed to the parent constructor.
   */
  OutputParameterDeviceSupport(RecordType *record, RecordAddress const &address,
      bool noConvert) : BaseDeviceSupport<RecordType>(record, address,
          noConvert)  {
  }

  /**
   * Writes the record's value to the underlying command. Depending on the
   * setting specified in the record's OUT field, the record's value is either
   * set as an argument or an environment variable for the specified command.
   */
  void processRecord() {
    std::ostringstream os;
    // The following three using declarations look quite complex. Effectively,
    // they determine the type of the VAL or RVAL field (depending on which one
    // is used). If the type is a floating-point type, the precision of the
    // stream is set to the max. number of digits that this type can represent.
    // If it is not a floating-point type, the precision for the double type is
    // used.
    // This means that we should never lose any precision when converting to a
    // string.
    using ValueFieldReferenceType =
        decltype(this->getValueField(this->getRecord()));
    using ValueFieldType =
        typename std::remove_reference<ValueFieldReferenceType>::type;
    using FloatingPointType =
        typename std::conditional<std::is_floating_point<ValueFieldType>::value, ValueFieldType, double>::type;
    os.precision(std::numeric_limits<FloatingPointType>::max_digits10);
    os << getValueField(this->getRecord());
    RecordAddress const &recordAddress = this->getRecordAddress();
    switch (recordAddress.getType()) {
    case RecordAddress::Type::argument:
      this->getCommand()->setArgument(recordAddress.getArgumentIndex(),
          os.str());
      break;
    case RecordAddress::Type::envVar:
      this->getCommand()->setEnvVar(recordAddress.getEnvVarName(),
          os.str());
      break;
    default:
      throw std::logic_error("Unexpected address type.");
      break;
    }
  }

private:

  template<RecordValFieldName V = ValFieldName>
  inline static auto getValueField(typename std::enable_if<V == RecordValFieldName::val, RecordType>::type *record)
      -> typename std::add_lvalue_reference<decltype(record->val)>::type {
    return record->val;
  }

  template<RecordValFieldName V = ValFieldName>
  inline static auto getValueField(typename std::enable_if<V == RecordValFieldName::rval, RecordType>::type *record)
      -> typename std::add_lvalue_reference<decltype(record->rval)>::type {
    return record->rval;
  }

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_OUTPUT_PARAMETER_DEVICE_SUPPORT_H
