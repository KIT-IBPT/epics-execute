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

#ifndef EPICS_EXEC_RECORD_VAL_FIELD_NAME_H
#define EPICS_EXEC_RECORD_VAL_FIELD_NAME_H

#include <sstream>
#include <stdexcept>

namespace epics {
namespace execute {

/**
 * Enum used as a template parameter. This enum defines whether the record's
 * VAL or RVAL field is supposed to be used.
 */
enum class RecordValFieldName {

  /**
   * Use the record's VAL field.
   */
  val,

  /**
   * Use the record's RVAL field.
   */
  rval

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_RECORD_VAL_FIELD_NAME_H
