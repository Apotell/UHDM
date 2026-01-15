/*
 Copyright 2020 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   NumUtils.h
 * Author: alain
 *
 * Created on Dec 29, 2020, 10:43 PM
 */

#ifndef UHDM_NUMUTILS_H
#define UHDM_NUMUTILS_H
#pragma once

#include <cctype>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace uhdm::NumUtils {
namespace internal {
// These functions parse a number from a std::string_view into "result".
// Returns the end of the number on success, nullptr otherwise.
// So this can be used in a simple boolean context for success testing, but
// as well in parsing context where advancing to the next position after the
// number is needed.
// All these functions require to check the return value to verify that
// parsing worked.

// Parse any number type with std::from_chars
template <typename number_type>
[[nodiscard]] bool strToNum(std::string_view s, int32_t base, number_type* result) {
  while (!s.empty() && std::isspace((int32_t)s.front())) s.remove_prefix(1);
  while (!s.empty() && std::isspace((int32_t)s.back())) s.remove_suffix(1);
  if (!s.empty() && (s.front() == '+')) s.remove_prefix(1);
  if (s.empty()) return false;
  auto success = std::from_chars(s.data(), s.data() + s.size(), *result, base);
  return ((success.ec == std::errc()) && ((s.data() + s.size()) == success.ptr));
}

template <typename sint_type, typename uint_type, typename result_type>
[[nodiscard]] bool strToInt(std::string_view s, int32_t base, result_type* result) {
  // std::from_chars can't deal with leading spaces or plus, so remove them.
  while (!s.empty() && std::isspace((int32_t)s.front())) s.remove_prefix(1);
  while (!s.empty() && std::isspace((int32_t)s.back())) s.remove_suffix(1);
  if (!s.empty() && (s.front() == '+')) s.remove_prefix(1);
  if (s.empty()) return false;
  std::from_chars_result parse_result;
  // Try parsing as signed first
  sint_type sn = 0;
  parse_result = std::from_chars(s.data(), s.data() + s.size(), sn, base);
  if ((parse_result.ec == std::errc()) && ((s.data() + s.size()) == parse_result.ptr)) {
    *result = static_cast<result_type>(sn);
    return true;
  }

  // If the number surely isn't -ve, try parsing as unsigned
  if ((s.front() != '-') && (parse_result.ec == std::errc::result_out_of_range)) {
    uint_type un = 0;
    parse_result = std::from_chars(s.data(), s.data() + s.size(), un, base);
    if ((parse_result.ec == std::errc()) && ((s.data() + s.size()) == parse_result.ptr)) {
      *result = static_cast<result_type>(un);
      return true;
    }
  }

  return false;
}
}  // namespace internal

[[nodiscard]] std::string hexToBin(std::string_view s);

[[nodiscard]] std::string binToHex(std::string_view s);

[[nodiscard]] std::string toBinary(int32_t size, uint64_t val);

[[nodiscard]] uint64_t getMask(uint64_t wide);

// Parse int values. Returns true if successful, otherwise false.

// Parse integer of the given size (int8_t, uint8_t, int32_t, uint32_t,
// int64_t, uint64_t), but be lenient in the sign interpretation.
// The full signed negative range and positive unsigned range for the integer
// of that size is allowed. The final value is cast to the signed or unsigned
// potentially flipping the sign (but we're leniently allowing that).
template <typename result_type>
[[nodiscard]] inline bool parseIntLenient(std::string_view s, size_t base, result_type* result) {
  if constexpr (sizeof(result_type) == 1) {
    return internal::strToInt<int8_t, uint8_t, result_type>(s, base, result);
  } else if constexpr (sizeof(result_type) == 2) {
    return internal::strToInt<int16_t, uint16_t, result_type>(s, base, result);
  } else if constexpr (sizeof(result_type) == 4) {
    return internal::strToInt<int32_t, uint32_t, result_type>(s, base, result);
  } else {
    return internal::strToInt<int64_t, uint64_t, result_type>(s, base, result);
  }
}

// Parse signed int8, stricly matching its range
[[nodiscard]] inline bool parseInt8(std::string_view s, int8_t* result) { return internal::strToNum(s, 10, result); }
// Parse unsigned int8, stricly matching its range
[[nodiscard]] inline bool parseUInt8(std::string_view s, uint8_t* result) { return internal::strToNum(s, 10, result); }

// Parse signed int16, stricly matching its range
[[nodiscard]] inline bool parseInt16(std::string_view s, int16_t* result) { return internal::strToNum(s, 10, result); }
// Parse unsigned int16, stricly matching its range
[[nodiscard]] inline bool parseUInt16(std::string_view s, uint16_t* result) {
  return internal::strToNum(s, 10, result);
}

// Parse signed int32, stricly matching its range
[[nodiscard]] inline bool parseInt32(std::string_view s, int32_t* result) { return internal::strToNum(s, 10, result); }
// Parse unsigned int32, stricly matching its range; no negative numbers
[[nodiscard]] inline bool parseUint32(std::string_view s, uint32_t* result) {
  return internal::strToNum(s, 10, result);
}

// Parse signed int64, stricly matching its range; no negative numbers
[[nodiscard]] inline bool parseInt64(std::string_view s, int64_t* result) { return internal::strToNum(s, 10, result); }
// Parse unsigned int64, stricly matching its range; no negative numbers
[[nodiscard]] inline bool parseUint64(std::string_view s, uint64_t* result) {
  return internal::strToNum(s, 10, result);
}

template <typename result_type>
[[nodiscard]] inline bool parseBinary(std::string_view s, result_type* result) {
  if constexpr (sizeof(result_type) == 1) {
    return internal::strToInt<int8_t, uint8_t, result_type>(s, 2, result);
  } else if constexpr (sizeof(result_type) == 2) {
    return internal::strToInt<int16_t, uint16_t, result_type>(s, 2, result);
  } else if constexpr (sizeof(result_type) == 4) {
    return internal::strToInt<int32_t, uint32_t, result_type>(s, 2, result);
  } else {
    return internal::strToInt<int64_t, uint64_t, result_type>(s, 2, result);
  }
}

template <typename result_type>
[[nodiscard]] inline bool parseOctal(std::string_view s, result_type* result) {
  if constexpr (sizeof(result_type) == 1) {
    return internal::strToInt<int8_t, uint8_t, result_type>(s, 8, result);
  } else if constexpr (sizeof(result_type) == 2) {
    return internal::strToInt<int16_t, uint16_t, result_type>(s, 8, result);
  } else if constexpr (sizeof(result_type) == 4) {
    return internal::strToInt<int32_t, uint32_t, result_type>(s, 8, result);
  } else {
    return internal::strToInt<int64_t, uint64_t, result_type>(s, 8, result);
  }
}

template <typename result_type>
[[nodiscard]] inline bool parseHex(std::string_view s, result_type* result) {
  if constexpr (sizeof(result_type) == 1) {
    return internal::strToInt<int8_t, uint8_t, result_type>(s, 16, result);
  } else if constexpr (sizeof(result_type) == 2) {
    return internal::strToInt<int16_t, uint16_t, result_type>(s, 16, result);
  } else if constexpr (sizeof(result_type) == 4) {
    return internal::strToInt<int32_t, uint32_t, result_type>(s, 16, result);
  } else {
    return internal::strToInt<int64_t, uint64_t, result_type>(s, 16, result);
  }
}

// Parse float value. Returns true if successful, otherwise false.
[[nodiscard]] bool parseFloat(std::string_view s, float* result);

// Parse double value. Returns true if successful, otherwise false.
[[nodiscard]] bool parseDouble(std::string_view s, double* result);

// Parse long double value. Returns true if successful, otherwise false.
[[nodiscard]] bool parseLongDouble(std::string_view s, long double* result);
}  // namespace uhdm::NumUtils

#endif  // UHDM_NUMUTILS_H
