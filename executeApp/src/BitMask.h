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

#ifndef EPICS_EXEC_BITMASK_H
#define EPICS_EXEC_BITMASK_H

#include <type_traits>

namespace epics {
namespace execute {

/**
 * Defines whether bit-mask operations are enabled for an enum type.
 *
 * In order to enable bit-mask operations for a type, define a template
 * specialization for this type, that used std::true_type as its base class.
 * For example:
 *
 * template<> struct EnableBitMask<MyEnumType> : public std::true_type {};
 */
template<typename EnumType>
struct EnableBitMask : public std::false_type {
};

namespace detail {
namespace bitMask {

template<typename EnumType>
using CheckedEnumType
    = typename std::enable_if<std::is_enum<EnumType>::value, EnumType>::type;

template<typename EnumType>
using NumericType = typename std::underlying_type<CheckedEnumType<EnumType>>::type;

} // namespace bitMask
} // namespace detail

/**
 * Bit mask of an enum type. Only enum type that use numeric values with
 * distinct bits are suitable for use with this class. For this reason, the use
 * of bit masks must be explicitly enabled by defining a template specialzation
 * for EnableBitMask<EnumType>.
 */
template<typename EnumType>
class BitMask {

private:

  static_assert(std::is_enum<EnumType>::value,
      "BitMask can only be used with enum types.");

  static_assert(EnableBitMask<EnumType>::value,
      "Use of this template has to be enabled explicitly through EnableBitMask.");

  using NumericType = typename detail::bitMask::NumericType<EnumType>;

public:

  constexpr inline BitMask() : mask(0) {
  }

  constexpr inline BitMask(EnumType value)
      : mask(static_cast<NumericType>(value)) {
  }

  constexpr inline BitMask operator&(BitMask other) const {
    return BitMask(this->mask & other.mask);
  }

  inline BitMask &operator&=(BitMask other) {
    this->mask &= other.mask;
    return *this;
  }

  constexpr inline BitMask operator|(BitMask other) const {
    return BitMask(this->mask | other.mask);
  }

  inline BitMask &operator|=(BitMask other) {
    this->mask |= other.mask;
    return *this;
  }

  constexpr inline BitMask operator^(BitMask other) const {
    return BitMask(this->mask ^ other.mask);
  }

  inline BitMask &operator^=(BitMask other) {
    this->mask ^= other.mask;
    return *this;
  }

  constexpr inline BitMask operator~() const {
    return BitMask(~this->mask);
  }

  constexpr inline operator bool() const {
    return this->mask != 0;
  }

private:

  NumericType mask;

  constexpr inline BitMask(NumericType mask) : mask(mask) {
  }

};

namespace detail {
namespace bitMask {

template<typename EnumType>
using BitMaskIfEnabled
    = typename std::enable_if<EnableBitMask<EnumType>::value, BitMask<EnumType>>::type;

} // namespace bitMask
} // namespace detail

template<typename EnumType>
constexpr typename detail::bitMask::BitMaskIfEnabled<EnumType> operator&(
    EnumType v1, EnumType v2) {
  return BitMask<EnumType>(v1) & v2;
}

template<typename EnumType>
constexpr typename detail::bitMask::BitMaskIfEnabled<EnumType> operator|(
    EnumType v1, EnumType v2) {
  return BitMask<EnumType>(v1) | v2;
}

template<typename EnumType>
constexpr typename detail::bitMask::BitMaskIfEnabled<EnumType> operator^(
    EnumType v1, EnumType v2) {
  return BitMask<EnumType>(v1) ^ v2;
}

template<typename EnumType>
constexpr typename detail::bitMask::BitMaskIfEnabled<EnumType> operator~(EnumType v) {
  return ~(BitMask<EnumType>(v));
}

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_BITMASK_H
