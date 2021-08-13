/*
 * Copyright 2018-2021 aquenos GmbH.
 * Copyright 2018-2021 Karlsruhe Institute of Technology.
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

#include <sstream>

#include "RecordAddress.h"

namespace epics {
namespace execute {

namespace {

class Parser {

public:

  Parser(std::string const &addressString,
      BitMask<RecordAddress::Type> allowedTypes)
      : addressString(addressString), allowedTypes(allowedTypes),
      position(0) {
  }

  RecordAddress parse() {
    auto foundCommandId = commandId();
    separator();
    auto foundType = type();
    int foundArgumentIndex = 0;
    std::string foundEnvVarName;
    BitMask<RecordAddress::Option> foundOptions;
    switch (foundType) {
    case RecordAddress::Type::argument:
      separator();
      foundArgumentIndex = argumentIndex();
      break;
    case RecordAddress::Type::envVar:
      separator();
      foundEnvVarName = envVarName();
      break;
    case RecordAddress::Type::exitCode:
      break;
    case RecordAddress::Type::run:
      // The additional options are optional, but if they are present, they must
      // be separated by a separator.
      if (!isEndOfString()) {
        separator();
        foundOptions = options(foundType);
      }
      break;
    case RecordAddress::Type::standardError:
      break;
    case RecordAddress::Type::standardInput:
      // The additional options are optional, but if they are present, they must
      // be separated by a separator.
      if (!isEndOfString()) {
        separator();
        foundOptions = options(foundType);
      }
      break;
    case RecordAddress::Type::standardOutput:
      break;
    }
    if (!isEndOfString()) {
      throwException(std::string("Expected end of string, but found \"")
          + excerpt() + "\".");
    }
    return RecordAddress(foundCommandId, foundType, foundArgumentIndex,
        foundEnvVarName, foundOptions);
  }

private:

  static std::string const commandIdChars;
  static std::string const digits0To9Chars;
  static std::string const digits1To9Chars;
  static std::string const envVarNameChars;
  static std::string const separatorChars;

  std::string addressString;
  BitMask<RecordAddress::Type> allowedTypes;
  std::size_t position;

  bool accept(std::string const &str) {
    if (addressString.length() - position < str.length()) {
      return false;
    }
    if (addressString.substr(position, str.length()) == str) {
      position += str.length();
      return true;
    } else {
      return false;
    }
  }

  bool acceptAnyOf(std::string const &characters) {
    if (isEndOfString()) {
      return false;
    }
    if (characters.find(peek()) != std::string::npos) {
      ++position;
      return true;
    } else {
      return false;
    }
  }

  int argumentIndex() {
    auto startPos = position;
    expectAnyOf(digits1To9Chars);
    int numberOfDigits = 1;
    while (acceptAnyOf(digits0To9Chars)) {
      ++numberOfDigits;
      // We can assume that no reasonable program will take more than
      // 10000 arguments, so we limit the argument index to four digits. As we
      // have to allocate some memory for every argument with a lower index
      // (even if that argument is not used), this protects us from running out
      // of memory.
      if (numberOfDigits > 4) {
        throwException("The argument index must have a max. number of four digits.");
      }
    }
    auto endPos = position;
    return stoi(addressString.substr(startPos, endPos - startPos));
  }

  std::string commandId() {
    auto startPos = position;
    expectAnyOf(commandIdChars);
    do {
    } while (acceptAnyOf(commandIdChars));
    auto endPos = position;
    return addressString.substr(startPos, endPos - startPos);
  }

  std::string envVarName() {
    auto startPos = position;
    expectAnyOf(envVarNameChars);
    do {
    } while (acceptAnyOf(envVarNameChars));
    auto endPos = position;
    return addressString.substr(startPos, endPos - startPos);
  }

  std::string excerpt() {
    if (addressString.length() - position > 5) {
      return addressString.substr(position, 5);
    } else {
      return addressString.substr(position);
    }
  }

  void expect(std::string const& str) {
    if (isEndOfString()) {
      throwException(std::string("Expected \"") + str
          + "\", but found end of string.");
    } else {
      throwException(std::string("Expected \"") + str
          + "\", but found \"" + excerpt() + "\".");
    }
  }

  void expectAnyOf(std::string const &characters) {
    if (!acceptAnyOf(characters)) {
      if (isEndOfString()) {
        throwException(std::string("Expected any of \"") + characters
            + "\", but found end of string.");
      } else {
        throwException(std::string("Expected any of \"") + characters
            + "\", but found \"" + peek() + "\".");
      }
    }
  }

  bool isEndOfString() {
    return position == addressString.length();
  }

  BitMask<RecordAddress::Option> options(RecordAddress::Type type) {
    switch (type) {
    case RecordAddress::Type::run:
      if (accept("wait")) {
        return RecordAddress::Option::wait;
      }
      break;
    case RecordAddress::Type::standardInput:
      if (accept("null-terminated")) {
        return RecordAddress::Option::nullTerminated;
      }
      break;
    default:
      // The other types do not take any additional options.
      break;
    }
    return BitMask<RecordAddressOption>();
  }

  char peek() {
    return addressString.at(position);
  }

  void separator() {
    expectAnyOf(separatorChars);
    do {
    } while (acceptAnyOf(separatorChars));
  }

  void throwException(std::string const &message) const {
    std::ostringstream os;
    os << "Error at character " << (position + 1)
        << " of the record address: " << message;
    throw std::invalid_argument(os.str());
  }

  RecordAddress::Type type() {
    if (isEndOfString()) {
      throwException("Expected type specifier, but found end of string.");
      // This throw statement is never used, but it is needed to avoid a
      // compiler warning.
      throw std::exception();
    } else if (accept("arg")) {
      if (!(allowedTypes & RecordAddress::Type::argument)) {
        throw std::invalid_argument(
            "Type arg is not allowed for this record type.");
      }
      return RecordAddress::Type::argument;
    } else if (accept("env")) {
      if (!(allowedTypes & RecordAddress::Type::envVar)) {
        throw std::invalid_argument(
            "Type env is not allowed for this record type.");
      }
      return RecordAddress::Type::envVar;
    } else if (accept("exit_code")) {
      if (!(allowedTypes & RecordAddress::Type::exitCode)) {
        throw std::invalid_argument(
            "Type exit_code is not allowed for this record type.");
      }
      return RecordAddress::Type::exitCode;
    } else if (accept("run")) {
      if (!(allowedTypes & RecordAddress::Type::run)) {
        throw std::invalid_argument(
            "Type run is not allowed for this record type.");
      }
      return RecordAddress::Type::run;
    } else if (accept("stderr")) {
      if (!(allowedTypes & RecordAddress::Type::standardError)) {
        throw std::invalid_argument(
            "Type stderr is not allowed for this record type.");
      }
      return RecordAddress::Type::standardError;
    } else if (accept("stdin")) {
      if (!(allowedTypes & RecordAddress::Type::standardInput)) {
        throw std::invalid_argument(
            "Type stdin is not allowed for this record type.");
      }
      return RecordAddress::Type::standardInput;
    } else if (accept("stdout")) {
      if (!(allowedTypes & RecordAddress::Type::standardOutput)) {
        throw std::invalid_argument(
            "Type stdout is not allowed for this record type.");
      }
      return RecordAddress::Type::standardOutput;
    } else {
      throwException(std::string("Expected type specifier, but found \"")
          + excerpt() + "\".");
      // This throw statement is never used, but it is needed to avoid a
      // compiler warning.
      throw std::exception();
    }

  }

};

std::string const Parser::commandIdChars = std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789");
std::string const Parser::digits0To9Chars = std::string("0123456789");
std::string const Parser::digits1To9Chars = std::string("123456789");
std::string const Parser::envVarNameChars = std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789");
std::string const Parser::separatorChars = std::string(" \t");

} // anonymous namespace

RecordAddress RecordAddress::parse(::DBLINK const &addressField,
    BitMask<Type> allowedTypes) {
  if (addressField.type != INST_IO
      || addressField.value.instio.string == nullptr
      || addressField.value.instio.string[0] == 0) {
    throw std::invalid_argument(
        "Invalid device address. Maybe mixed up INP/OUT or forgot '@'?");
  }
  return Parser(addressField.value.instio.string, allowedTypes).parse();
}

} // namespace execute
} // namespace epics
