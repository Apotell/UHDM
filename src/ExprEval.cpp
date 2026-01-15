/*

 Copyright 2019-2022 Alain Dargelas

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
 * File:   ExprEval.cpp
 * Author: hs
 *
 * Created on July 3, 2021, 8:03 PM
 */

#include <uhdm/Elaborator.h>
#include <uhdm/ExprEval.h>
#include <uhdm/NumUtils.h>
#include <uhdm/Serializer.h>
#include <uhdm/UhdmVisitor.h>
#include <uhdm/Utils.h>
#include <uhdm/uhdm.h>

#include <bitset>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4018)  // '<=': signed/unsigned mismatch
#pragma warning(disable : 4293)  // '<<': shift count negative or too big,
                                 // undefined behavior
#pragma warning(disable : 4389)  // signed/unsigned mismatch
#endif

namespace uhdm {

struct identity final {
  constexpr static bool boolean_result = false;

  template <class T>
  inline constexpr T operator()(const T &x) const {
    return x;
  };
  inline std::string operator()(std::string_view x) const { return std::string(x); };
};

struct unary_reduce_and final {
  constexpr static bool boolean_result = false;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr bool operator()(T v) const {
    using UT = typename std::make_unsigned<T>::type;
    UT u = static_cast<UT>(v);
    return u == std::numeric_limits<UT>::max();
  }
  inline std::string operator()(std::string_view v) const {
    if (v.empty()) return "0";

    // v may contain '0', '1', 'x', 'z', or other characters.
    // Reduction AND -> if ANY non-'1' found -> result is '0'
    for (char c : v) {
      if (c != '1') return "0";
    }
    return "1";
  }
};

struct unary_reduce_or final {
  constexpr static bool boolean_result = false;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr bool operator()(T v) const {
    using UT = typename std::make_unsigned<T>::type;
    return static_cast<UT>(v) != 0;
  }
  inline std::string operator()(std::string_view v) const {
    if (v.empty()) return "0";

    // If ANY bit is '1' -> result is '1'
    for (char c : v) {
      if (c == '1') return "1";
    }

    return "0";
  }
};

struct unary_reduce_xor final {
  constexpr static bool boolean_result = false;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr bool operator()(T v) const {
    using UT = typename std::make_unsigned<T>::type;

    UT x = static_cast<UT>(v);
    bool parity = false;
    while (x) {
      parity ^= (x & 1u);
      x >>= 1u;
    }
    return parity;
  }
  inline std::string operator()(std::string_view v) const {
    if (v.empty()) return "0";

    bool parity = false;
    bool hasUnknown = false;

    for (char c : v) {
      switch (c) {
        case '1': parity = !parity; break;
        case '0': break;
        case 'x':
        case 'X':
        case 'z':
        case 'Z':
        default: hasUnknown = true; break;
      }
    }

    // Unknown dominates ONLY if parity cannot be resolved with known bits
    if (hasUnknown) return "x";

    return parity ? "1" : "0";
  }
};

struct unary_negate final {
  constexpr static bool boolean_result = false;

  template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return std::negate()(v);
  }
  inline std::string operator()(std::string_view v) const {
    if (v.empty()) return "0";

    const size_t width = v.size();

    // If any unknown bit -> full unknown
    for (char c : v) {
      if ((c == 'x') || (c == 'z')) {
        return std::string(width, 'x');
      } else if ((c == 'X') || (c == 'Z')) {
        return std::string(width, 'X');
      }
    }

    // Convert binary string to signed integer
    int64_t val = 0;
    for (char c : v) {
      val = (val << 1) | ((c == '1') ? 1 : 0);
    }

    // Determine sign bit (2’s complement)
    bool negative = (v[0] == '1');
    if (negative) {
      // Convert from two's complement to signed integer
      val -= (1LL << width);
    }

    // Apply negate
    val = -val;

    // Convert back to two's complement binary with fixed width
    int64_t mod = (1LL << width);
    val &= (mod - 1);  // wrap

    std::string out(width, '0');
    for (size_t i = 0; i < width; ++i) {
      if (val & (1LL << (width - 1 - i))) out[i] = '1';
    }

    return out;
  }
};

struct unary_bit_not final {
  constexpr static bool boolean_result = false;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return std::bit_not()(v);
  }
  template <typename T, typename std::enable_if<std::is_floating_point<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v;
  }
  inline std::string operator()(std::string_view v) const {
    if (v.empty()) return "";

    std::string out;
    out.reserve(v.size());

    for (char c : v) {
      switch (c) {
        case '0': out.push_back('1'); break;
        case '1': out.push_back('0'); break;
        case 'x':
        case 'z': out.push_back('x'); break;  // unknown result
        case 'X':
        case 'Z': out.push_back('X'); break;  // unknown result
        default: out.push_back('x'); break;   // any illegal -> unknown
      }
    }

    return out;
  }
};

struct unary_logical_not final {
  constexpr static bool boolean_result = true;

  template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return std::logical_not()(v);
  }
  inline std::string operator()(std::string_view v) const {
    bool has_one = false;
    bool has_zero = false;
    bool has_xz = false;

    for (char c : v) {
      switch (c) {
        case '1': has_one = true; break;
        case '0': has_zero = true; break;
        case 'x':
        case 'X':
        case 'z':
        case 'Z': has_xz = true; break;
        default: break;
      }
    }

    // Logical reduction:
    // If any 1 → boolean = 1 → !1 = 0
    if (has_one) return "0";

    // No 1s. If all zeros → boolean = 0 → !0 = 1
    if (!has_xz) return "1";

    // No 1s, has X/Z → boolean = X → !X = X
    return "x";
  }
};

struct unary_and_op final {
  constexpr static bool boolean_result = true;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return unary_reduce_and()(v) ? T{1} : T{0};
  }
  template <typename T, typename std::enable_if<std::is_floating_point<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v;
  }
  inline std::string operator()(std::string_view v) const {
    if (v.empty()) return "x";

    bool hasZero = false;
    bool hasXorZ = false;

    for (char c : v) {
      switch (c) {
        case '1': break;
        case '0': hasZero = true; break;
        case 'x':
        case 'X':
        case 'z':
        case 'Z': hasXorZ = true; break;
        default: hasXorZ = true; break;
      }
    }

    // If any 0 present, definite false => result 0
    if (hasZero) return "0";

    // If all bits 1 => true => result 1
    if (!hasZero && !hasXorZ) return "1";

    // Otherwise, uncertainty => result x
    return "x";
  }
};

struct unary_nand_op final {
  constexpr static bool boolean_result = true;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return unary_reduce_and()(v) ? T{0} : T{1};
  }
  template <typename T, typename std::enable_if<std::is_floating_point<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v;
  }
  inline std::string operator()(std::string_view v) const {
    if (v.empty()) return "x";

    bool hasZero = false;
    bool hasXorZ = false;

    for (char c : v) {
      switch (c) {
        case '1': break;
        case '0': hasZero = true; break;
        case 'x':
        case 'X':
        case 'z':
        case 'Z': hasXorZ = true; break;
        default: hasXorZ = true; break;
      }
    }

    // Reduction AND result outcomes
    if (hasZero) {
      // &v = 0  => ~&v = 1
      return "1";
    }

    if (!hasZero && !hasXorZ) {
      // &v = 1 => ~&v = 0
      return "0";
    }

    // &v = x => ~&v = x
    return "x";
  }
};

struct unary_or_op final {
  constexpr static bool boolean_result = true;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return unary_reduce_or()(v) ? T{1} : T{0};
  }
  template <typename T, typename std::enable_if<std::is_floating_point<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v;
  }
  inline std::string operator()(std::string_view v) const {
    if (v.empty()) return "x";

    bool hasXorZ = false;

    for (char c : v) {
      switch (c) {
        case '1': return "1";  // Short-circuit: OR reduces to 1
        case '0': break;       // continue checking
        case 'x':
        case 'X':
        case 'z':
        case 'Z': hasXorZ = true; break;
        default: hasXorZ = true; break;
      }
    }

    if (hasXorZ) {
      return "x";
    }

    return "0";  // All zeros
  }
};

struct unary_nor_op final {
  constexpr static bool boolean_result = true;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return unary_reduce_or()(v) ? T{0} : T{1};
  }
  template <typename T, typename std::enable_if<std::is_floating_point<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v;
  }
  inline std::string operator()(std::string_view v) const {
    bool has1 = false;
    bool hasUnknown = false;

    for (char c : v) {
      if (c == '1')
        has1 = true;
      else if (c != '0')  // x, z, ?, etc
        hasUnknown = true;
    }

    if (has1) return "0";        // OR = 1, so NOR = 0
    if (hasUnknown) return "x";  // OR = x, so NOR = x
    return "1";                  // all zeros → OR = 0 → NOR = 1
  }
};

struct unary_xor_op final {
  constexpr static bool boolean_result = true;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return unary_reduce_xor()(v) ? T{1} : T{0};
  }
  template <typename T, typename std::enable_if<std::is_floating_point<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v;
  }
  inline std::string operator()(std::string_view v) const {
    bool parity = false;

    for (char c : v) {
      if (c == '1')
        parity ^= true;
      else if (c == '0')
        parity ^= false;
      else
        return "x";  // SystemVerilog: unknown propagates
    }

    return parity ? "1" : "0";
  }
};

struct unary_xnor_op final {
  constexpr static bool boolean_result = true;

  template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return unary_reduce_xor()(v) ? T{0} : T{1};
  }
  template <typename T, typename std::enable_if<std::is_floating_point<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v;
  }
  inline std::string operator()(std::string_view v) const {
    bool parity = false;

    for (char c : v) {
      if (c == '1')
        parity ^= true;
      else if (c == '0')
        parity ^= false;
      else
        return "x";  // SystemVerilog unknown propagation
    }

    // XNOR = NOT XOR
    return parity ? "0" : "1";
  }
};

struct unary_preinc_op final {
  constexpr static bool boolean_result = false;

  template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v + 1;
  }
  inline std::string operator()(std::string_view v) const {
    // 1. X/Z propagation
    if (v.find_first_of("xz") != std::string::npos) {
      return std::string(v.size(), 'x');
    } else if (v.find_first_of("XZ") != std::string::npos) {
      return std::string(v.size(), 'X');
    }

    // 2. Convert to unsigned integer
    unsigned long long val = 0;
    for (char c : v) {
      val = (val << 1) | (c == '1');
    }

    // 3. Increment
    val += 1;

    // 4. Truncate to original width (wrap-around)
    std::string result(v.size(), '0');
    for (int32_t i = int32_t(v.size()) - 1; i >= 0; --i) {
      result[i] = (val & 1ULL) ? '1' : '0';
      val >>= 1ULL;
    }

    return result;
  }
};

struct unary_predec_op final {
  constexpr static bool boolean_result = false;

  template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v - 1;
  }

  inline std::string operator()(std::string_view v) const {
    const size_t width = v.size();

    // X/Z propagation
    if (v.find_first_of("xz") != std::string::npos) return std::string(width, 'x');
    if (v.find_first_of("XZ") != std::string::npos) return std::string(width, 'X');

    // Convert binary string to unsigned
    unsigned long long val = 0;
    for (char c : v) {
      val = (val << 1) | (c == '1');
    }

    // Mask to width (important!)
    unsigned long long mask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1);

    // Pre-decrement with wrap-around
    val = (val - 1) & mask;

    // Convert back to binary string
    std::string result(width, '0');
    for (int32_t i = int32_t(width) - 1; i >= 0; --i) {
      result[i] = (val & 1ULL) ? '1' : '0';
      val >>= 1ULL;
    }

    return result;
  }
};

struct unary_postinc_op final {
  constexpr static bool boolean_result = false;

  template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v;
  }
  inline std::string operator()(std::string_view v) const { return std::string(v); }
};

struct unary_postdec_op final {
  constexpr static bool boolean_result = false;

  template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, bool>::type = true>
  inline constexpr T operator()(T v) const {
    return v;
  }
  inline std::string operator()(std::string_view v) const { return std::string(v); }
};

struct binary_plus final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            std::enable_if_t<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool> = true>
  inline constexpr auto operator()(A a, B b) const {
    return a + b;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // Any X/Z → return X
    auto has_unknown = [](std::string_view v) { return v.find_first_of("xXzZ") != std::string::npos; };

    if (has_unknown(a) || has_unknown(b)) {
      return "x";
    }

    // Convert binary strings → integers
    auto to_uint = [](std::string_view v) -> uint64_t {
      uint64_t val = 0;
      for (char c : v) {
        val = (val << 1) | (c == '1' ? 1 : 0);
      }
      return val;
    };

    uint64_t av = to_uint(a);
    uint64_t bv = to_uint(b);
    uint64_t sum = av + bv;

    // Result width = max width of inputs
    size_t width = std::max(a.size(), b.size());

    // Convert back to binary with correct width
    std::string out(width, '0');
    for (size_t i = 0; i < width; i++) {
      size_t bit = width - 1 - i;
      out[i] = ((sum >> bit) & 1) ? '1' : '0';
    }

    return out;
  }
};

struct binary_minus final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            std::enable_if_t<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool> = true>
  inline constexpr auto operator()(A a, B b) const {
    return a - b;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // Check X/Z in operand
    auto has_unknown = [](std::string_view v) { return v.find_first_of("xXzZ") != std::string::npos; };

    if (has_unknown(a) || has_unknown(b)) {
      return "x";  // SystemVerilog: arithmetic with X/Z → X
    }

    // Convert binary string to uint64
    auto to_uint = [](std::string_view v) -> uint64_t {
      uint64_t val = 0;
      for (char c : v) {
        val = (val << 1) | (c == '1' ? 1 : 0);
      }
      return val;
    };

    uint64_t av = to_uint(a);
    uint64_t bv = to_uint(b);

    // Perform subtraction
    uint64_t diff = av - bv;

    // Result width = max operand width
    size_t width = std::max(a.size(), b.size());

    // Convert back to binary string
    std::string out(width, '0');
    for (size_t i = 0; i < width; i++) {
      size_t bit = width - 1 - i;
      out[i] = ((diff >> bit) & 1) ? '1' : '0';
    }

    return out;
  }
};

struct binary_multiplies final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            std::enable_if_t<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool> = true>
  inline constexpr auto operator()(A a, B b) const {
    return a * b;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // Check for X/Z in operands
    auto has_unknown = [](std::string_view v) { return v.find_first_of("xXzZ") != std::string::npos; };

    if (has_unknown(a) || has_unknown(b)) {
      return "x";  // X propagation for arithmetic ops
    }

    // Convert binary string to uint64
    auto to_uint = [](std::string_view v) -> uint64_t {
      uint64_t val = 0;
      for (char c : v) {
        val = (val << 1) | (c == '1' ? 1ULL : 0ULL);
      }
      return val;
    };

    uint64_t av = to_uint(a);
    uint64_t bv = to_uint(b);

    uint64_t product = av * bv;

    // Result width = a_width + b_width (SystemVerilog rule)
    size_t width = std::max(a.size(), b.size());

    // Convert result to binary string
    std::string out(width, '0');
    for (size_t i = 0; i < width; i++) {
      size_t bit = width - 1 - i;
      out[i] = ((product >> bit) & 1ULL) ? '1' : '0';
    }

    return out;
  }
};

struct binary_divides final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            std::enable_if_t<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool> = true>
  inline constexpr auto operator()(A a, B b) const {
    return a / b;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // Check for X/Z in operands
    auto has_unknown = [](std::string_view v) { return v.find_first_of("xXzZ") != std::string::npos; };

    if (has_unknown(a) || has_unknown(b)) {
      return "x";  // X propagation
    }

    // Convert binary string to uint64
    auto to_uint = [](std::string_view v) -> uint64_t {
      uint64_t val = 0;
      for (char c : v) {
        val = (val << 1) | (c == '1' ? 1ULL : 0ULL);
      }
      return val;
    };

    uint64_t av = to_uint(a);
    uint64_t bv = to_uint(b);

    // Division by zero → X (SystemVerilog behavior)
    if (bv == 0) return "x";

    uint64_t div = av / bv;

    // Result width = width of A (SystemVerilog rule)
    size_t width = a.size();

    // Convert result to binary string
    std::string out(width, '0');
    for (size_t i = 0; i < width; i++) {
      size_t bit = width - 1 - i;
      out[i] = ((div >> bit) & 1ULL) ? '1' : '0';
    }

    return out;
  }
};

struct binary_bit_or final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a | b;
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // Make widths equal (pad left with zeros)
    size_t w = std::max(a.size(), b.size());

    auto pad = [&](std::string_view v) {
      std::string s(w - v.size(), '0');
      s += v;
      return s;
    };

    std::string as = pad(a);
    std::string bs = pad(b);

    std::string out(w, '0');

    auto is1 = [](char c) { return c == '1'; };
    auto is0 = [](char c) { return c == '0'; };
    auto is_unknown = [](char c) { return (c == 'x') || (c == 'X') || (c == 'z') || (c == 'Z'); };

    for (size_t i = 0; i < w; i++) {
      char A = as[i];
      char B = bs[i];

      if (is1(A) || is1(B)) {
        // If either is '1' → result = 1
        out[i] = '1';
      } else if (is0(A) && is0(B)) {
        // If both are 0 → result = 0
        out[i] = '0';
      } else if ((A == 'z') || (B == 'z')) {
        // If z involved → x
        out[i] = 'x';
      } else if ((A == 'Z') || (B == 'Z')) {
        out[i] = 'X';
      } else {
        // Remaining cases are X combinations → x
        out[i] = 'x';
      }
    }

    return out;
  }
};

struct binary_bit_xor final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a ^ b;
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    size_t w = std::max(a.size(), b.size());

    auto pad_msb = [&](std::string_view v) {
      std::string s(w - v.size(), '0');  // pad MSB
      s += v;
      return s;
    };

    std::string as = pad_msb(a);
    std::string bs = pad_msb(b);
    std::string out(w, '0');

    for (size_t i = 0; i < w; i++) {
      char A = as[i];
      char B = bs[i];

      // If either is unknown (x/z), output X
      if ((A == 'x') || (A == 'z') || (B == 'x') || (B == 'z')) {
        out[i] = 'x';
      } else if ((A == 'X') || (A == 'Z') || (B == 'X') || (B == 'Z')) {
        out[i] = 'X';
      } else {
        // Both 0 or 1, normal XOR
        out[i] = ((A == '1') != (B == '1')) ? '1' : '0';
      }
    }

    return out;
  }
};

struct binary_modulus_op {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a % b;
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return std::fmod(a, b);
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // If either operand has unknown bits → result is X
    auto has_unknown_xz = [](std::string_view s) { return s.find_first_of("xz") != std::string_view::npos; };
    auto has_unknown_XZ = [](std::string_view s) { return s.find_first_of("XZ") != std::string_view::npos; };

    if (has_unknown_xz(a) || has_unknown_xz(b)) {
      size_t w = std::max(a.size(), b.size());
      return std::string(w, 'x');
    } else if (has_unknown_XZ(a) || has_unknown_XZ(b)) {
      size_t w = std::max(a.size(), b.size());
      return std::string(w, 'X');
    }

    // Convert binary strings to integer
    auto bin_to_int = [](std::string_view s) -> uint64_t {
      uint64_t val = 0;
      for (char c : s) {
        val <<= 1;
        if (c == '1') val |= 1;
      }
      return val;
    };

    uint64_t val_a = bin_to_int(a);
    uint64_t val_b = bin_to_int(b);

    // Avoid division by zero
    if (val_b == 0) {
      size_t w = std::max(a.size(), b.size());
      return std::string(w, 'x');  // X for invalid modulus
    }

    uint64_t val_r = val_a % val_b;

    // Convert back to binary string, pad to original width
    size_t width = a.size();  // or max(a.size(), b.size())
    std::string out(width, '0');
    for (size_t i = 0; i < width; i++) {
      if ((val_r >> (width - 1 - i)) & 1) {
        out[i] = '1';
      }
    }

    return out;
  }
};

struct binary_bit_and {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a & b;
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    size_t width = std::max(a.size(), b.size());
    std::string out(width, '0');

    for (size_t i = 0; i < width; ++i) {
      char bit_a = (i < width - a.size()) ? '0' : a[i - (width - a.size())];
      char bit_b = (i < width - b.size()) ? '0' : b[i - (width - b.size())];

      if ((bit_a == '0') || (bit_b == '0')) {
        out[i] = '0';
      } else if ((bit_a == '1') && (bit_b == '1')) {
        out[i] = '1';
      } else {
        // x, z propagation
        out[i] = 'x';
      }
    }

    return out;
  }
};

struct binary_equal final {
  constexpr static bool boolean_result = true;

  template <typename A, typename B,
            std::enable_if_t<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool> = true>
  inline constexpr auto operator()(A a, B b) const {
    return a == b;
  }

  inline std::string operator()(std::string_view a, std::string_view b) const {
    auto get_bit = [](std::string_view s, size_t i, size_t w) -> char {
      return (i < w - s.size()) ? '0' : s[i - (w - s.size())];
    };

    const size_t width = std::max(a.size(), b.size());

    char unknown = 0;
    for (size_t i = 0; i < width; ++i) {
      char ba = get_bit(a, i, width);
      char bb = get_bit(b, i, width);

      if ((ba == 'x') || (ba == 'z') || (bb == 'x') || (bb == 'z')) {
        if (unknown == 0) unknown = 'x';
      } else if ((ba == 'X') || (ba == 'Z') || (bb == 'X') || (bb == 'Z')) {
        if (unknown == 0) unknown = 'X';
      } else if (ba != bb) {
        return "0";  // definite mismatch
      }
    }
    if (unknown == 0) unknown = '1';
    return std::string(1, unknown);
  }
};

struct binary_not_equal final {
  constexpr static bool boolean_result = true;

  template <typename A, typename B,
            std::enable_if_t<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool> = true>
  inline constexpr auto operator()(A a, B b) const {
    return a != b;
  }

  inline std::string operator()(std::string_view a, std::string_view b) const {
    auto get_bit = [](std::string_view s, size_t i, size_t w) -> char {
      return (i < w - s.size()) ? '0' : s[i - (w - s.size())];
    };

    const size_t width = std::max(a.size(), b.size());

    char unknown = 0;
    for (size_t i = 0; i < width; ++i) {
      char ba = get_bit(a, i, width);
      char bb = get_bit(b, i, width);

      if ((ba == 'x') || (ba == 'z') || (bb == 'x') || (bb == 'z')) {
        if (unknown == 0) unknown = 'x';
      } else if ((ba == 'X') || (ba == 'Z') || (bb == 'X') || (bb == 'Z')) {
        if (unknown == 0) unknown = 'X';
      } else if (ba != bb) {
        return "1";  // definite inequality
      }
    }
    if (unknown == 0) unknown = '0';
    return std::string(1, unknown);
  }
};

struct binary_greater final {
  constexpr static bool boolean_result = true;

  template <typename A, typename B,
            typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a > b;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // If either input contains unknown ('x', 'X', 'z', 'Z'), return "x"
    if ((a.find_first_of("xz") != std::string::npos) || (b.find_first_of("xz") != std::string::npos)) {
      return "x";
    } else if ((a.find_first_of("XZ") != std::string::npos) || (b.find_first_of("XZ") != std::string::npos)) {
      return "X";
    }

    // Pad the shorter string with '0' on the left
    const size_t width = std::max(a.size(), b.size());
    const std::string a_padded = StrCat(std::string(width - a.size(), '0'), a);
    const std::string b_padded = StrCat(std::string(width - b.size(), '0'), b);

    // Compare as unsigned
    for (size_t i = 0; i < width; ++i) {
      if (a_padded[i] > b_padded[i]) return "1";
      if (a_padded[i] < b_padded[i]) return "0";
    }

    // Equal
    return "0";
  }
};

struct binary_greater_equal final {
  constexpr static bool boolean_result = true;

  template <typename A, typename B,
            typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a >= b;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // If either input contains unknown ('x', 'X', 'z', 'Z'), return "x"
    if ((a.find_first_of("xz") != std::string::npos) || (b.find_first_of("xz") != std::string::npos)) {
      return "x";
    } else if ((a.find_first_of("XZ") != std::string::npos) || (b.find_first_of("XZ") != std::string::npos)) {
      return "X";
    }

    // Pad the shorter string with '0' on the left
    const size_t width = std::max(a.size(), b.size());
    const std::string a_padded = StrCat(std::string(width - a.size(), '0'), a);
    const std::string b_padded = StrCat(std::string(width - b.size(), '0'), b);

    // Compare bit by bit
    for (size_t i = 0; i < width; ++i) {
      if (a_padded[i] > b_padded[i]) return "1";
      if (a_padded[i] < b_padded[i]) return "0";
    }

    // Equal
    return "1";
  }
};

struct binary_less final {
  constexpr static bool boolean_result = true;

  template <typename A, typename B,
            typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a < b;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // If either input contains unknown ('x', 'X', 'z', 'Z'), return "x"
    if ((a.find_first_of("xz") != std::string::npos) || (b.find_first_of("xz") != std::string::npos)) {
      return "x";
    } else if ((a.find_first_of("XZ") != std::string::npos) || (b.find_first_of("XZ") != std::string::npos)) {
      return "X";
    }

    // Pad the shorter string with '0' on the left
    const size_t width = std::max(a.size(), b.size());
    const std::string a_padded = StrCat(std::string(width - a.size(), '0'), a);
    const std::string b_padded = StrCat(std::string(width - b.size(), '0'), b);

    // Compare bit by bit
    for (size_t i = 0; i < width; ++i) {
      if (a_padded[i] < b_padded[i]) return "1";
      if (a_padded[i] > b_padded[i]) return "0";
    }

    // Equal
    return "0";
  }
};

struct binary_less_equal final {
  constexpr static bool boolean_result = true;

  template <typename A, typename B,
            typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a <= b;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    // If either input contains unknown ('x', 'X', 'z', 'Z'), return "x"
    if ((a.find_first_of("xz") != std::string::npos) || (b.find_first_of("xz") != std::string::npos)) {
      return "x";
    } else if ((a.find_first_of("XZ") != std::string::npos) || (b.find_first_of("XZ") != std::string::npos)) {
      return "X";
    }

    // Pad the shorter string with '0' on the left
    const size_t width = std::max(a.size(), b.size());
    const std::string a_padded = StrCat(std::string(width - a.size(), '0'), a);
    const std::string b_padded = StrCat(std::string(width - b.size(), '0'), b);

    // Compare bit by bit
    for (size_t i = 0; i < width; ++i) {
      if (a_padded[i] < b_padded[i]) return "1";
      if (a_padded[i] > b_padded[i]) return "0";
    }

    // Equal
    return "1";
  }
};

struct binary_logical_and_op final {
  constexpr static bool boolean_result = true;

  template <typename A, typename B,
            std::enable_if_t<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool> = true>
  inline constexpr auto operator()(A a, B b) const {
    return (a && b);
  }

  inline std::string operator()(std::string_view a, std::string_view b) const {
    auto reduce_to_bit = [](std::string_view v) {
      char unknown = 0;

      for (char c : v) {
        if (c == '1') return '1';
        if ((c == 'x') || (c == 'z')) {
          if (unknown == 0) unknown = c;
        }
        if ((c == 'X') || (c == 'Z')) {
          if (unknown == 0) unknown = c;
        }
      }

      return (unknown == 0) ? '0' : unknown;
    };

    char ra = reduce_to_bit(a);
    char rb = reduce_to_bit(b);

    // Boolean AND with X-propagation
    if ((ra == '0') || (rb == '0')) return "0";
    if ((ra == '1') && (rb == '1')) return "1";
    return "x";
  }
};

struct binary_logical_or_op final {
  constexpr static bool boolean_result = true;

  template <typename A, typename B,
            std::enable_if_t<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, bool> = true>
  inline constexpr auto operator()(A a, B b) const {
    return (a || b);
  }

  inline std::string operator()(std::string_view a, std::string_view b) const {
    bool unknown = false;

    for (char c : a) {
      if (c == '1') return "1";
      if (c != '0') unknown = true;
      if (unknown) break;
    }
    for (char c : b) {
      if (c == '1') return "1";
      if (c != '0') unknown = true;
      if (unknown) break;
    }

    return unknown ? "x" : "0";
  }
};

struct binary_xnor_op final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return ~(a ^ b);
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    if (a.size() != b.size()) return "x";  // size mismatch → unknown

    std::string result;
    result.reserve(a.size());

    for (size_t i = 0; i < a.size(); ++i) {
      char ca = a[i];
      char cb = b[i];

      if ((ca != '0') && (ca != '1')) {  // unknown
        result.push_back('x');
      } else if ((cb != '0') && (cb != '1')) {  // unknown
        result.push_back('x');
      } else {
        result.push_back((ca == cb) ? '1' : '0');
      }
    }

    return result;
  }
};

struct binary_nand_op final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return ~(a & b);
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }
  inline std::string operator()(std::string_view v1, std::string_view v2) const {
    if (v1.size() != v2.size()) return "x";  // size mismatch → unknown

    std::string result;
    result.reserve(v1.size());

    for (size_t i = 0; i < v1.size(); ++i) {
      char a = v1[i];
      char b = v2[i];

      // If either is unknown, result is unknown
      if (((a != '0') && (a != '1')) || ((b != '0') && (b != '1'))) {
        result.push_back('x');
      } else {
        // bitwise NAND: ~(a & b)
        result.push_back(((a == '1' && b == '1') ? '0' : '1'));
      }
    }

    return result;
  }
};

struct binary_nor_op final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return ~(a | b);
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }
  inline std::string operator()(std::string_view v1, std::string_view v2) const {
    if (v1.size() != v2.size()) return "x";  // size mismatch → unknown

    std::string result;
    result.reserve(v1.size());

    for (size_t i = 0; i < v1.size(); ++i) {
      char a = v1[i];
      char b = v2[i];

      const bool a_known = (a == '0') || (a == '1');
      const bool b_known = (b == '0') || (b == '1');

      // Any unknown → result is x
      if (!a_known || !b_known) {
        result.push_back('x');
      } else {
        // bitwise NOR: ~(a | b)
        char or_val = ((a == '1') || (b == '1')) ? '1' : '0';
        char nor_val = (or_val == '1') ? '0' : '1';
        result.push_back(nor_val);
      }
    }

    return result;
  }
};

// TODO:: I don't think below operator convered all use cases
// binary_imply_op
// binary_overlap_imply_op
// binary_non_overlap_imply_op
// Rest all is working, but need to check across different types

struct binary_imply_op final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr bool operator()(A a, B b) const {
    return (!a) || b;
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value && std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr bool operator()(A a, B b) const {
    return false;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const { return StrCat(a, b); }
};

struct binary_overlap_imply_op final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr bool operator()(A a, B b) const {
    return (!a) || b;
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value && std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr bool operator()(A a, B b) const {
    return false;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const { return StrCat(a, b); }
};

struct binary_non_overlap_imply_op final {
  constexpr static bool boolean_result = false;

  bool prev_a = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr bool operator()(A a, B b) {
    bool result = true;

    // If previous cycle had a=1, b must be 1 now
    if (prev_a) result = (b == 1);

    // update for next cycle
    prev_a = (a == 1);

    return result;
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value && std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr bool operator()(A a, B b) {
    return false;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const { return StrCat(a, b); }
};

struct binary_logical_lshift_op final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr A operator()(A a, B b) const {
    using V = std::make_unsigned_t<A>;
    V ua = static_cast<V>(a);

    constexpr int32_t WIDTH = sizeof(V) * 8;

    if (b >= WIDTH) return static_cast<V>(0);

    return static_cast<V>(ua << b);
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    const size_t width = a.size();

    // 1. Shift amount must be known (no X/Z)
    unsigned shift = 0;
    for (char c : b) {
      if ((c == '0') || (c == '1')) {
        shift = (shift << 1) | (c == '1');
      } else if ((c == 'x') || (c == 'z')) {
        // X/Z in shift amount → all X
        return std::string(width, 'x');
      } else if ((c == 'X') || (c == 'Z')) {
        // X/Z in shift amount → all X
        return std::string(width, 'X');
      }
    }

    // 2. Shift >= width → all zeros
    if (shift >= width) {
      return std::string(width, '0');
    }

    // 3. Logical left shift
    std::string result(width, '0');

    for (size_t i = 0; i < width - shift; ++i) {
      result[i] = a[i + shift];
    }

    return result;
  }
};

struct binary_logical_rshift_op final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr A operator()(A a, B b) const {
    using V = std::make_unsigned_t<A>;
    V ua = static_cast<V>(a);

    constexpr int32_t WIDTH = sizeof(V) * 8;
    if (b >= WIDTH) return static_cast<V>(0);

    return static_cast<V>(ua >> b);
  }

  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }

  inline std::string operator()(std::string_view a, std::string_view b) const {
    size_t width = a.size();

    // 1. Check shift amount for X/Z
    for (char c : b) {
      if ((c == 'x') || (c == 'z')) {
        return std::string(width, 'x');
      } else if ((c == 'X') || (c == 'Z')) {
        return std::string(width, 'X');
      }
    }

    // 2. Convert binary string to integer
    int32_t shift = 0;
    for (char c : b) {
      shift = (shift << 1) | (c - '0');
    }

    if (shift >= (int32_t)width) return std::string(width, '0');

    std::string result(width, '0');

    for (size_t i = 0; i < width - shift; ++i) {
      result[width - 1 - i] = a[width - 1 - i - shift];
    }

    return result;
  }
};

struct binary_arith_lshift_op final {
  constexpr static bool boolean_result = false;

  template <typename A, typename B,
            typename std::enable_if<std::is_integral<A>::value && std::is_integral<B>::value, bool>::type = true>
  inline constexpr A operator()(A a, B b) const {
    return static_cast<std::make_unsigned_t<A>>(a) << b;
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    const size_t N = a.size();

    // Binary shift amount with x/z check
    int32_t sh = 0;
    for (char c : b) {
      if ((c == 'x') || (c == 'z')) {
        return std::string(N, 'x');
      } else if ((c == 'X') || (c == 'Z')) {
        return std::string(N, 'X');
      } else {
        sh = (sh << 1) | (c - '0');
      }
    }

    if (sh <= 0) return std::string(a);
    if (static_cast<size_t>(sh) >= N) return std::string(N, '0');

    std::string out;
    out.reserve(N);

    // Drop MSBs, shift left
    out.append(a.substr(sh));  // keep a[sh .. N-1]
    out.append(sh, '0');       // append zeros at LSB

    return out;
  }
};

struct binary_arith_rshift_op final {
  constexpr static bool boolean_result = false;

  template <typename T, typename U,
            typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value, bool>::type = true>
  inline constexpr T operator()(T a, U b) const {
    using ST = std::make_signed_t<T>;
    using UT = std::make_unsigned_t<T>;

    constexpr int32_t WIDTH = sizeof(T) * 8;

    // Convert to signed for arithmetic shift
    ST sa = static_cast<ST>(a);

    // Do arithmetic right shift (sign-extended)
    ST shifted = sa >> b;

    // Mask back to original width (important!)
    UT mask = (WIDTH == 64) ? ~UT(0) : ((UT(1) << WIDTH) - 1);

    return static_cast<T>(shifted & mask);
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return a;
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    size_t width = a.size();

    // 1. Check shift amount for X/Z
    for (char c : b) {
      if ((c == 'x') || (c == 'z')) {
        return std::string(width, 'x');
      } else if ((c == 'X') || (c == 'Z')) {
        return std::string(width, 'X');
      }
    }

    // 2. Convert binary string to integer
    int32_t shift = 0;
    for (char c : b) {
      shift = (shift << 1) | (c - '0');
    }

    if (shift >= (int32_t)width) return std::string(width, '0');

    std::string result(width, '0');

    for (size_t i = 0; i < width - shift; ++i) {
      result[width - 1 - i] = a[width - 1 - i - shift];
    }

    return result;
  }
};

struct binary_power_op final {
  constexpr static bool boolean_result = false;

  template <typename T, typename U,
            typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value, bool>::type = true>
  inline constexpr T operator()(T a, U b) const {
    using R = typename std::common_type_t<T, U>;
    R base = static_cast<R>(a);
    R exp = static_cast<R>(b);

    if (exp < 0) return R(0);

    R result = 1;
    while (exp > 0) {
      if (exp & 1) result *= base;
      base *= base;
      exp >>= 1;
    }
    return result;
  }
  template <
      typename A, typename B,
      typename std::enable_if<std::is_floating_point<A>::value || std::is_floating_point<B>::value, bool>::type = true>
  inline constexpr auto operator()(A a, B b) const {
    return std::pow(a, b);
  }
  inline std::string operator()(std::string_view a, std::string_view b) const {
    const size_t N = a.size();

    // 1. Parse exponent string
    long long exp = 0;
    try {
      exp = std::stoll(std::string(b));
    } catch (...) {
      // exponent has x/z → whole result is x
      return std::string(N, 'x');
    }

    if (exp < 0) return std::string(N, '0');

    // exp == 0 case: result = ...0001
    if (exp == 0) {
      std::string out(N, '0');
      out.back() = '1';
      return out;
    }

    // 2. Detect X/Z in base
    for (char c : a) {
      if ((c == 'x') || (c == 'z')) {
        return std::string(N, 'x');
      } else if ((c == 'X') || (c == 'Z')) {
        return std::string(N, 'X');
      }
    }

    // 3. Convert 0/1 string → 64-bit integer
    unsigned long long base = 0;
    for (char c : a) base = (base << 1) | (c == '1');

    // 4. Exponentiation with early truncation
    unsigned long long result = 1;
    unsigned long long bbase = base;

    for (long long e = exp; e > 0; e >>= 1) {
      if ((e & 1) != 0) {
        result *= bbase;
        // If the result exceeds N bits, overflow → fine, SV truncates
        if (result >> N) break;
      }
      bbase *= bbase;
      if (bbase >> N) break;
    }

    // 5. Convert result to N-bit vector
    std::string out(N, '0');
    for (int32_t i = N - 1; i >= 0; --i) {
      out[i] = (result & 1) ? '1' : '0';
      result >>= 1;
    }

    return out;
  }
};

struct binary_concat_op final {
  constexpr static bool boolean_result = false;

  inline std::string operator()(std::string_view a, std::string_view b) const { return StrCat(a, b); }
};

struct binary_replicate_op final {
  constexpr static bool boolean_result = false;

  inline std::string operator()(int64_t count, std::string_view value) const {
    if (count <= 0) return {};

    const size_t n = static_cast<size_t>(count);

    std::string result;
    result.reserve(value.size() * n);

    for (size_t i = 0; i < n; ++i) {
      result.append(value);
    }

    return result;
  }
};

struct unary_replicate_extend_op final {
  constexpr static bool boolean_result = false;

  inline std::string operator()(size_t width, std::string_view value) const {
    if (value.size() == 1) {
      return std::string(width, value.front());
    }

    if (value.size() == width) {
      return std::string(value);
    }

    if (value.size() > width) {
      return std::string(value.substr(value.size() - width));
    }

    size_t missing = width - value.size();
    return StrCat(std::string(missing, value.front()), value);
  }
  inline std::string operator()(std::string_view width, std::string_view value) const { return std::string(value); }
};

struct binary_case_eq_op final {
  constexpr static bool boolean_result = false;

  inline std::string operator()(std::string_view a, std::string_view b) const {
    if (a.length() != b.length()) return "0";

    for (size_t i = 0, ni = a.length(); i < ni; ++i) {
      if (a[i] != b[i]) return "0";
    }
    return "1";
  }
};

struct binary_case_neq_op final {
  constexpr static bool boolean_result = false;

  inline std::string operator()(std::string_view a, std::string_view b) const {
    binary_case_eq_op eq;
    return (eq(a, b) == "1") ? "0" : "1";
  }
};

struct ExprEval::cast_op final {
  template <typename R>
  inline std::enable_if_t<std::is_integral<R>::value, R> operator()(const value_t &value) const {
    return std::visit(
        [](auto &&arg) -> R {
          using T = std::decay_t<decltype(arg)>;

          /* numeric → int32_t */
          if constexpr (std::is_same_v<T, nvalue_t>) {
            return std::visit([](auto &&v) -> R { return static_cast<R>(v); }, arg);
          }

          /* real → int32_t (truncate toward zero) */
          else if constexpr (std::is_same_v<T, rvalue_t>) {
            return std::visit(
                [](auto &&v) -> R {
                  if constexpr (std::is_signed_v<R>) {
                    return static_cast<R>((v >= 0.0) ? std::floor(v + 0.5) : std::ceil(v - 0.5));
                  } else {
                    return static_cast<R>(std::floor(v + 0.5));
                  }
                },
                arg);
          }

          /* string → int32_t (binary semantics) */
          else if constexpr (std::is_same_v<T, svalue_t>) {
            return std::visit(
                [](const std::string &s) -> R {
                  std::string norm = s;
                  for (char &c : norm) {
                    if ((c == 'x') || (c == 'X') || (c == 'z') || (c == 'Z') || (c == '?')) {
                      c = '0';
                    }
                  }

                  uint64_t v = 0;
                  if (!NumUtils::parseBinary(norm, &v)) return R{0};
                  return static_cast<R>(v);
                },
                arg);
          } else {
            return R{0};
          }
        },
        value);
  }

  template <typename R, typename std::enable_if<std::is_floating_point<R>::value, bool>::type = true>
  inline constexpr R operator()(const value_t &value) const {
    return std::visit(
        [](auto &&arg) -> R {
          using T = std::decay_t<decltype(arg)>;

          if constexpr (std::is_same_v<T, nvalue_t>) {
            return std::visit([](auto &&v) { return static_cast<R>(v); }, arg);
          } else if constexpr (std::is_same_v<T, rvalue_t>) {
            return std::visit([](auto &&v) { return static_cast<R>(v); }, arg);
          } else if constexpr (std::is_same_v<T, svalue_t>) {
            return std::visit(
                [](auto &&v) -> double {
                  std::string norm = v;
                  for (char &c : norm) {
                    if ((c == 'x') || (c == 'X') || (c == 'z') || (c == 'Z') || (c == '?')) {
                      c = '0';
                    }
                  }
                  uint64_t d = 0;
                  if (!NumUtils::parseBinary(norm, &d)) {
                    d = 0;  // FIXME: Need to handle failure!
                  }
                  return d;
                },
                arg);
          } else {
            return 0;
          }
        },
        value);
  }

  template <typename R, typename std::enable_if<std::is_same<R, std::string>::value, bool>::type = true>
  inline R operator()(const value_t &value) const {
    if (std::holds_alternative<svalue_t>(value)) {
      return std::get<std::string>(std::get<svalue_t>(value));
    } else if (std::holds_alternative<nvalue_t>(value)) {
      return std::visit(
          [](auto &&arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            constexpr size_t bits = sizeof(T) * 8;

            std::ostringstream oss;
            oss << std::bitset<bits>(static_cast<std::make_unsigned_t<T>>(arg));
            return oss.str();  // will be stored in svalue_t inside value_t
          },
          std::get<nvalue_t>(value));
    } else if (std::holds_alternative<rvalue_t>(value)) {
      return std::visit(
          [](auto &&arg) -> std::string {
            std::ostringstream oss;
            oss << arg;
            return oss.str();
          },
          std::get<rvalue_t>(value));
    }
    return {};
  }
};

struct ExprEval::is_signed_op final {
  inline bool operator()(const value_t &value) const {
    if (!std::holds_alternative<nvalue_t>(value)) {
      return false;
    }

    return std::visit(
        [](auto &&arg) -> bool {
          using T = std::decay_t<decltype(arg)>;
          return std::is_signed_v<T>;
        },
        std::get<nvalue_t>(value));
  }
};

struct ExprEval::is_unsigned_op final {
  inline bool operator()(const value_t &value) const {
    if (!std::holds_alternative<nvalue_t>(value)) {
      return false;
    }

    return std::visit(
        [](auto &&arg) -> bool {
          using T = std::decay_t<decltype(arg)>;
          return std::is_unsigned_v<T>;
        },
        std::get<nvalue_t>(value));
  }
};

struct ExprEval::rank_op final {
  inline uint32_t operator()(UhdmType ut) const {
    switch (ut) {
      case UhdmType::ByteTypespec: return 1;
      case UhdmType::ShortIntTypespec: return 2;
      case UhdmType::IntTypespec: return 3;
      case UhdmType::LongIntTypespec: return 4;
      case UhdmType::IntegerTypespec: return 5;
      case UhdmType::LogicTypespec: return 6;
      case UhdmType::ShortRealTypespec: return 7;
      case UhdmType::RealTypespec: return 8;
      default: return 0;
    }
  }
};

class DetectRefObj final : public UhdmVisitor {
 public:
  void visitAny(const Any *any) final {
    switch (any->getUhdmType()) {
      case UhdmType::RefObj:
      case UhdmType::BitSelect:
      case UhdmType::IndexedPartSelect:
      case UhdmType::PartSelect:
      case UhdmType::VarSelect:
      case UhdmType::HierPath:
        m_hasRefObj = true;
        requestAbort();
        break;

      default: break;
    };
  }
  bool refObjDetected() const { return m_hasRefObj; }

 private:
  bool m_hasRefObj = false;
};

ExprEval::ExprEval(ObjectProvider *provider, bool muteError /* = false */)
    : m_provider(provider), m_muteError(muteError) {}

bool ExprEval::isFullySpecified(const Typespec *tps) {
  if (tps == nullptr) return true;
  DetectRefObj detector;
  detector.visit(tps);
  return !detector.refObjDetected();
}

inline static bool isFourState(const Typespec *typespec) {
  return (typespec->getUhdmType() == UhdmType::IntegerTypespec) || (typespec->getUhdmType() == UhdmType::LogicTypespec);
}

inline static constexpr bool isConvSysFunc(std::string_view name) {
  return (name == "$rtoi") || (name == "$itor") || (name == "$signed") || (name == "$unsigned") ||
         (name == "$realtobits") || (name == "$bitstoreal") || (name == "$shortrealtobits") || (name == "$cast") ||
         (name == "$bitstoshortreal");
}

inline static constexpr bool isMathSysFunc(std::string_view name) {
  return (name == "$clog2") || (name == "$asin") || (name == "$acos") || (name == "$atan") || (name == "$ln") ||
         (name == "$log10") || (name == "$exp") || (name == "$sqrt") || (name == "$floor") || (name == "$ceil") ||
         (name == "$sin") || (name == "$cos") || (name == "$tan") || (name == "$sinh") || (name == "$cosh") ||
         (name == "$tanh") || (name == "$asinh") || (name == "$acosh") || (name == "$atanh") || (name == "$atan2") ||
         (name == "$hypot") || (name == "$pow");
}
inline static bool isDataQuerySysFunc(std::string_view name) {
  return (name == "$bits") || (name == "$isunbounded") || (name == "$typename");
}

inline static bool isArrayQuerySysFunc(std::string_view name) {
  return (name == "$unpacked_dimensions") || (name == "$dimensions") || (name == "$left") || (name == "$right") ||
         (name == "$low") || (name == "$high") || (name == "$increment") || (name == "$size");
}

inline static bool isBitVectorSysFunc(std::string_view name) {
  return (name == "$countbits") || (name == "$onehot") || (name == "$isunknown") || (name == "$countones") ||
         (name == "$onehot0");
}

template <typename T1, typename T2, typename T3>
std::variant<std::monostate, T2, T3> ExprEval::parseNumber(const T1 *typespec, std::string_view sv,
                                                           int32_t constType) const {
  std::variant<std::monostate, T2, T3> value;
  switch (constType) {
    case vpiBinaryConst: {
      if (typespec->getSigned()) {
        T2 v = 0;
        if (NumUtils::parseBinary(sv, &v)) value = v;
      } else {
        T3 v = 0;
        if (NumUtils::parseBinary(sv, &v)) value = v;
      }
    } break;

    case vpiDecConst: {
      if (typespec->getSigned()) {
        T2 v = 0;
        if (NumUtils::internal::strToNum(sv, 10, &v)) value = v;
      } else {
        const bool sign = sv.front() == '-';
        if (sign) sv.remove_prefix(1);

        T3 v = 0;
        if (NumUtils::internal::strToNum(sv, 10, &v)) {
          value = sign ? static_cast<T3>(-v) : v;
        }
      }
    } break;

    case vpiHexConst: {
      if (typespec->getSigned()) {
        T2 v = 0;
        if (NumUtils::parseHex(sv, &v)) value = v;
      } else {
        T3 v = 0;
        if (NumUtils::parseHex(sv, &v)) value = v;
      }
    } break;

    case vpiOctConst: {
      if (typespec->getSigned()) {
        T2 v = 0;
        if (NumUtils::parseOctal(sv, &v)) value = v;
      } else {
        T3 v = 0;
        if (NumUtils::parseOctal(sv, &v)) value = v;
      }
      break;
    }
    case vpiIntConst: {
      if (typespec->getSigned()) {
        T2 v = 0;
        if (NumUtils::internal::strToNum(sv, 10, &v)) value = v;
      } else {
        const bool sign = sv.front() == '-';
        if (sign) sv.remove_prefix(1);

        T3 v = 0;
        if (NumUtils::parseIntLenient(sv, 10, &v)) {
          value = sign ? static_cast<T3>(-v) : v;
        }
      }
    } break;
    case vpiUIntConst: {
      if (typespec->getSigned()) {
        T2 v = 0;
        if (NumUtils::parseIntLenient(sv, 10, &v)) {
          value = v;
        }
      } else {
        const bool sign = sv.front() == '-';
        if (sign) sv.remove_prefix(1);

        T3 v = 0;
        if (NumUtils::internal::strToNum(sv, 10, &v)) {
          value = sign ? static_cast<T3>(-v) : v;
        }
      }
    } break;

    case vpiScalar: {
      if (typespec->getSigned()) {
        T2 v = 0;
        if (NumUtils::parseBinary(sv, &v)) value = v;
      } else {
        const bool sign = sv.front() == '-';
        if (sign) sv.remove_prefix(1);

        T3 v = 0;
        if (NumUtils::parseBinary(sv, &v)) {
          value = sign ? static_cast<T3>(-v) : v;
        }
      }
    } break;

    case vpiStringConst: {
      if (sv.length() > 16) break;
      if (typespec->getSigned()) {
        T2 v = 0;
        for (uint32_t i = 0, ni = sv.size(); i < ni; ++i) {
          v += (sv[i] << ((sv.size() - (i + 1)) * 8));
        }
      } else {
        T3 v = 0;
        for (uint32_t i = 0, ni = sv.size(); i < ni; ++i) {
          v += (sv[i] << ((sv.size() - (i + 1)) * 8));
        }
      }
    } break;

    case vpiRealConst: {
      // Don't do the double precision math, leave it to client tools
    } break;

    default: break;
  }
  return value;
}

template <typename T1, typename T2, typename T3>
std::variant<std::monostate, T2, T3> ExprEval::parseNumber(const TimeTypespec *typespec, std::string_view sv,
                                                           int32_t constType) const {
  std::variant<std::monostate, T2, T3> value;
  const bool sign = sv.front() == '-';
  if (sign) sv.remove_prefix(1);

  T3 v = 0;
  if (NumUtils::internal::strToNum(sv, 10, &v)) {
    value = sign ? static_cast<T3>(-v) : v;
  }
  return value;
}

template <typename T1, typename T2, typename T3>
std::variant<std::monostate, T2, T3, std::string> ExprEval::parseBinary(const T1 *typespec, std::string_view sv,
                                                                        int32_t constType) const {
  std::variant<std::monostate, T2, T3, std::string> value;
  switch (constType) {
    case vpiBinaryConst: {
      if (typespec->getSigned()) {
        T2 v = 0;
        if (!isFourState(typespec) && (sv.length() <= (sizeof(T2) * 8)) && NumUtils::parseBinary(sv, &v)) {
          value = v;
        } else {
          value = std::move(std::string(sv));
        }
      } else {
        T3 v = 0;
        if (!isFourState(typespec) && (sv.length() <= (sizeof(T3) * 8)) && NumUtils::parseBinary(sv, &v)) {
          value = v;
        } else {
          value = std::move(std::string(sv));
        }
      }
    } break;

    case vpiOctConst:
    case vpiHexConst:
    case vpiDecConst:
    case vpiIntConst:
    case vpiUIntConst: {
      std::variant<std::monostate, T2, T3> v = parseNumber<T1, T2, T3>(typespec, sv, constType);
      if (std::holds_alternative<T2>(v)) {
        value = std::get<T2>(v);
      } else if (std::holds_alternative<T3>(v)) {
        value = std::get<T3>(v);
      }
    } break;

    default: break;
  }

  return value;
}

std::string ExprEval::parseBinary(const Expr *expr) const {
  const Typespec *const typespec = getTypespec(expr);
  if (typespec == nullptr) return "";
  if (typespec->getUhdmType() == UhdmType::EnumTypespec) return "";

  int32_t constType = 0;
  std::string_view sv;

  if (const Variable *const variable = any_cast<Variable>(expr)) {
    expr = variable->getValue();
  }

  if (const Constant *const constant = any_cast<Constant>(expr)) {
    constType = constant->getConstType();
    sv = constant->getDecompile();
  }

  if (sv.empty()) return "";

  std::string value;
  switch (typespec->getUhdmType()) {
    case UhdmType::LogicTypespec: {
      std::string v;
      if (formatBinary(any_cast<Constant>(expr), &v)) {
        value = v;
      }
    } break;

    default: break;
  }

  return value;
}

ExprEval::value_t ExprEval::promote(const value_t &value, UhdmType targetType, bool isUnsigned) const {
  cast_op castOp;
  if ((targetType == UhdmType::LogicTypespec) || (targetType == UhdmType::IntegerTypespec)) {
    return castOp.operator()<std::string>(value);
  }

  if (targetType == UhdmType::ShortRealTypespec) {
    return castOp.operator()<float>(value);
  }

  if (targetType == UhdmType::RealTypespec) {
    return castOp.operator()<double>(value);
  }

  if (std::holds_alternative<nvalue_t>(value)) {
    return std::visit(
        [&](auto &&arg) -> value_t {
          switch (targetType) {
            case UhdmType::ByteTypespec:
              return isUnsigned ? nvalue_t(static_cast<uint8_t>(arg)) : nvalue_t(static_cast<int8_t>(arg));

            case UhdmType::ShortIntTypespec:
              return isUnsigned ? nvalue_t(static_cast<uint16_t>(arg)) : nvalue_t(static_cast<int16_t>(arg));

            case UhdmType::IntTypespec:
              return isUnsigned ? nvalue_t(static_cast<uint32_t>(arg)) : nvalue_t(static_cast<int32_t>(arg));

            case UhdmType::LongIntTypespec:
              return isUnsigned ? nvalue_t(static_cast<uint64_t>(arg)) : nvalue_t(static_cast<int64_t>(arg));

            default: return value;
          }
        },
        std::get<nvalue_t>(value));
  } else if (std::holds_alternative<rvalue_t>(value)) {
    double dv = castOp.operator()<double>(value);

    switch (targetType) {
      case UhdmType::ByteTypespec:
        return isUnsigned ? nvalue_t(static_cast<uint8_t>(dv)) : nvalue_t(static_cast<int8_t>(dv));

      case UhdmType::ShortIntTypespec:
        return isUnsigned ? nvalue_t(static_cast<uint16_t>(dv)) : nvalue_t(static_cast<int16_t>(dv));

      case UhdmType::IntTypespec:
        return isUnsigned ? nvalue_t(static_cast<uint32_t>(dv)) : nvalue_t(static_cast<int32_t>(dv));

      case UhdmType::LongIntTypespec:
        return isUnsigned ? nvalue_t(static_cast<uint64_t>(dv)) : nvalue_t(static_cast<int64_t>(dv));

      default: return value;
    }
  }

  return value;
}

inline UhdmType ExprEval::promoted(UhdmType type0, value_t &value0, UhdmType type1, value_t &value1) const {
  rank_op rank;
  is_unsigned_op isUnsigned;
  const UhdmType resultType = (rank(type0) > rank(type1)) ? type0 : type1;
  const bool isUnsignedResult = isUnsigned(value0) || isUnsigned(value1);
  value0 = promote(value0, resultType, isUnsignedResult);
  value1 = promote(value1, resultType, isUnsignedResult);
  return resultType;
}

ExprEval::value_t ExprEval::parse(const Expr *expr) const {
  const Typespec *const typespec = getTypespec(expr);
  if (typespec == nullptr) return value_t();
  if (typespec->getUhdmType() == UhdmType::EnumTypespec) return value_t();

  int32_t constType = 0;
  std::string_view sv;
  size_t size = 0;

  if (const Variable *const variable = any_cast<Variable>(expr)) {
    expr = variable->getValue();
  }

  if (const Constant *const constant = any_cast<Constant>(expr)) {
    constType = constant->getConstType();
    sv = constant->getDecompile();
    size = constant->getSize();
  }

  if (sv.empty()) return value_t();

  value_t value;
  switch (typespec->getUhdmType()) {
    case UhdmType::ByteTypespec: {
      std::variant<std::monostate, int8_t, uint8_t> v =
          parseNumber<ByteTypespec, int8_t, uint8_t>(static_cast<const ByteTypespec *>(typespec), sv, constType);
      if (std::holds_alternative<int8_t>(v)) {
        value = nvalue_t(std::get<int8_t>(v));
      } else if (std::holds_alternative<uint8_t>(v)) {
        value = nvalue_t(std::get<uint8_t>(v));
      }
    } break;

    case UhdmType::ShortIntTypespec: {
      std::variant<std::monostate, int16_t, uint16_t> v = parseNumber<ShortIntTypespec, int16_t, uint16_t>(
          static_cast<const ShortIntTypespec *>(typespec), sv, constType);
      if (std::holds_alternative<int16_t>(v)) {
        value = nvalue_t(std::get<int16_t>(v));
      } else if (std::holds_alternative<uint16_t>(v)) {
        value = nvalue_t(std::get<uint16_t>(v));
      }
    } break;

    case UhdmType::IntTypespec: {
      std::variant<std::monostate, int32_t, uint32_t> v =
          parseNumber<IntTypespec, int32_t, uint32_t>(static_cast<const IntTypespec *>(typespec), sv, constType);
      if (std::holds_alternative<int32_t>(v)) {
        value = nvalue_t(std::get<int32_t>(v));
      } else if (std::holds_alternative<uint32_t>(v)) {
        value = nvalue_t(std::get<uint32_t>(v));
      }
    } break;

    case UhdmType::IntegerTypespec: {
      std::variant<std::monostate, int32_t, uint32_t, std::string> v = parseBinary<IntegerTypespec, int32_t, uint32_t>(
          static_cast<const IntegerTypespec *>(typespec), sv, constType);
      if (std::holds_alternative<int32_t>(v)) {
        value = nvalue_t(std::get<int32_t>(v));
      } else if (std::holds_alternative<uint32_t>(v)) {
        value = nvalue_t(std::get<uint32_t>(v));
      } else if (std::holds_alternative<std::string>(v)) {
        value = svalue_t(std::get<std::string>(v));
      }
    } break;

    case UhdmType::TimeTypespec: {
      std::variant<std::monostate, int64_t, uint64_t> v =
          parseNumber<TimeTypespec, int64_t, uint64_t>(static_cast<const TimeTypespec *>(typespec), sv, constType);
      if (std::holds_alternative<int64_t>(v)) {
        value = nvalue_t(std::get<int64_t>(v));
      } else if (std::holds_alternative<uint64_t>(v)) {
        value = nvalue_t(std::get<uint64_t>(v));
      }
    } break;
    case UhdmType::LongIntTypespec: {
      std::variant<std::monostate, int64_t, uint64_t> v = parseNumber<LongIntTypespec, int64_t, uint64_t>(
          static_cast<const LongIntTypespec *>(typespec), sv, constType);
      if (std::holds_alternative<int64_t>(v)) {
        value = nvalue_t(std::get<int64_t>(v));
      } else if (std::holds_alternative<uint64_t>(v)) {
        value = nvalue_t(std::get<uint64_t>(v));
      }
    } break;

    case UhdmType::LogicTypespec: {
      if (size <= 32) {
        std::variant<std::monostate, int32_t, uint32_t, std::string> v =
            parseBinary<LogicTypespec, int32_t, uint32_t>(static_cast<const LogicTypespec *>(typespec), sv, constType);
        if (std::holds_alternative<int32_t>(v)) {
          value = nvalue_t(std::get<int32_t>(v));
        } else if (std::holds_alternative<uint32_t>(v)) {
          value = nvalue_t(std::get<uint32_t>(v));
        } else if (std::holds_alternative<std::string>(v)) {
          value = svalue_t(std::get<std::string>(v));
        }
      } else {
        std::variant<std::monostate, int64_t, uint64_t, std::string> v =
            parseBinary<LogicTypespec, int64_t, uint64_t>(static_cast<const LogicTypespec *>(typespec), sv, constType);
        if (std::holds_alternative<int64_t>(v)) {
          value = nvalue_t(std::get<int64_t>(v));
        } else if (std::holds_alternative<uint64_t>(v)) {
          value = nvalue_t(std::get<uint64_t>(v));
        } else if (std::holds_alternative<std::string>(v)) {
          value = svalue_t(std::get<std::string>(v));
        }
      }
    } break;

    case UhdmType::ShortRealTypespec: {
      if (constType != vpiRealConst) break;
      float v = 0;
      if (NumUtils::parseFloat(sv, &v)) value = rvalue_t(v);
    } break;

    case UhdmType::RealTypespec: {
      if (constType != vpiRealConst) break;
      double v = 0;
      if (NumUtils::parseDouble(sv, &v)) value = rvalue_t(v);
    } break;

    default: break;
  }

  return value;
}

bool ExprEval::getInt64(const Expr *expr, int64_t *result, bool strict /* = true */) const {
  const value_t value = parse(expr);
  if (std::holds_alternative<nvalue_t>(value)) {
    const nvalue_t &nvalue = std::get<nvalue_t>(value);
    if ((nvalue.index() % 2) == 0) {
      std::visit([result](auto &&arg) { *result = arg; }, nvalue);
      return true;
    }
  }
  return false;
}

bool ExprEval::getUInt64(const Expr *expr, uint64_t *result, bool strict /* = true */) const {
  const value_t value = parse(expr);
  if (std::holds_alternative<nvalue_t>(value)) {
    const nvalue_t &nvalue = std::get<nvalue_t>(value);
    if ((nvalue.index() % 2) == 1) {
      std::visit([result](auto &&arg) { *result = arg; }, nvalue);
      return true;
    }
  }
  return false;
}

bool ExprEval::getDouble(const Expr *expr, long double *result, bool strict /* = true */) const {
  const value_t value = parse(expr);
  if (std::holds_alternative<rvalue_t>(value)) {
    const rvalue_t &rvalue = std::get<rvalue_t>(value);
    std::visit([result](auto &&arg) { *result = arg; }, rvalue);
    return true;
  }
  return false;
}

bool ExprEval::formatBinary(const Constant *constant, std::string *result) const {
  if (constant == nullptr) return false;

  const value_t value = parse(constant);
  if (std::holds_alternative<std::monostate>(value)) {
    return false;
  } else if (std::holds_alternative<nvalue_t>(value)) {
    const nvalue_t &nvalue = std::get<nvalue_t>(value);
    if ((nvalue.index() % 2) == 0) {
      int64_t v = 0;
      std::visit([&v](auto &&arg) { v = arg; }, nvalue);
      *result = NumUtils::toBinary(constant->getSize(), v);
    } else {
      uint64_t v = 0;
      std::visit([&v](auto &&arg) { v = arg; }, nvalue);
      *result = NumUtils::toBinary(constant->getSize(), v);
    }
  } else if (std::holds_alternative<rvalue_t>(value)) {
    // Don't do the double precision math, leave it to client tools
    return false;
  } else if (std::holds_alternative<svalue_t>(value)) {
    const svalue_t &svalue = std::get<svalue_t>(value);
    *result = std::get<std::string>(svalue);

    if (result->size() < constant->getSize()) {
      *result = std::string(constant->getSize() - result->size(), '0') + *result;
    }

    // if (sv.size() > 32) {
    //   return result;
    // }
    // uint64_t res = 0;
    // for (uint32_t i = 0; i < sv.size(); i++) {
    //   res += (sv[i] << ((sv.size() - (i + 1)) * 8));
    // }
    // result = NumUtils::toBinary(c->getSize(), res);
  }
  return true;
}

template <typename T>
bool ExprEval::format(T value, int32_t constType, int32_t size, std::string *result) {
  switch (constType) {
    case vpiBinaryConst: {
      *result = NumUtils::toBinary(size, value);
    } break;

    case vpiHexConst: {
      std::ostringstream oss;
      oss << std::hex << std::bitset<sizeof(T) * 8>(value);
      *result = oss.str();
    } break;

    case vpiOctConst: {
      std::ostringstream oss;
      oss << std::oct << std::bitset<sizeof(T) * 8>(value);
      *result = oss.str();
    } break;

    case vpiRealConst: {
      std::ostringstream oss;
      if constexpr (sizeof(value) == sizeof(float)) {
        oss << std::setprecision(std::numeric_limits<float>::digits10 + 1) << value;
      } else if constexpr (sizeof(value) == sizeof(double)) {
        oss << std::setprecision(std::numeric_limits<double>::digits10 + 1) << value;
      }
      *result = oss.str();
    } break;

    default: {
      if constexpr (sizeof(T) == sizeof(char)) {
        *result = StrCat(static_cast<int32_t>(value));
      } else {
        *result = StrCat(value);
      }
    } break;
  }
  return true;
}

template <typename T>
Constant *ExprEval::createConstant(T value, Serializer &serializer, UhdmType uhdmType, int32_t constType,
                                   int32_t size) const {
  Constant *const oconstant = serializer.make<Constant>();

  std::string result;
  Typespec *otypespec = nullptr;
  if (uhdmType == UhdmType::LogicTypespec) {
    format(value, vpiBinaryConst, size, &result);
    oconstant->setValue(result);
    oconstant->setDecompile(result);
    oconstant->setSize(result.length());
    oconstant->setConstType(vpiBinaryConst);

    LogicTypespec *const it = serializer.make<LogicTypespec>();
    it->setSigned(std::is_signed<T>());
    otypespec = it;
  } else if (uhdmType == UhdmType::IntegerTypespec) {
    format(value, vpiBinaryConst, size, &result);
    oconstant->setValue(result);
    oconstant->setDecompile(result);
    oconstant->setSize(result.length());
    oconstant->setConstType(vpiBinaryConst);

    IntegerTypespec *const it = serializer.make<IntegerTypespec>();
    it->setSigned(std::is_signed<T>());
    otypespec = it;
  } else if constexpr (std::is_integral<T>()) {
    if constexpr (sizeof(T) == 1) {
      format(value, vpiIntConst, size, &result);
      oconstant->setValue(result);
      oconstant->setDecompile(result);
      oconstant->setSize(8);
      oconstant->setConstType(constType);

      ByteTypespec *const it = serializer.make<ByteTypespec>();
      it->setSigned(std::is_signed<T>());
      otypespec = it;
    } else if constexpr (sizeof(T) == 2) {
      format(value, vpiIntConst, size, &result);
      oconstant->setValue(result);
      oconstant->setDecompile(result);
      oconstant->setSize(16);
      oconstant->setConstType(constType);

      ShortIntTypespec *const it = serializer.make<ShortIntTypespec>();
      it->setSigned(std::is_signed<T>());
      otypespec = it;
    } else if constexpr (sizeof(T) == 4) {
      format(value, vpiIntConst, size, &result);
      oconstant->setValue(result);
      oconstant->setDecompile(result);
      oconstant->setSize(32);
      oconstant->setConstType(constType);

      IntTypespec *const it = serializer.make<IntTypespec>();
      it->setSigned(std::is_signed<T>());
      otypespec = it;
    } else if constexpr (sizeof(T) == 8) {
      format(value, vpiIntConst, size, &result);
      oconstant->setValue(result);
      oconstant->setDecompile(result);
      oconstant->setSize(64);
      oconstant->setConstType(constType);

      LongIntTypespec *const lit = serializer.make<LongIntTypespec>();
      lit->setSigned(std::is_signed<T>());
      otypespec = lit;
    }
  } else if constexpr (std::is_floating_point<T>()) {
    if constexpr (sizeof(T) == 4) {
      format(value, vpiRealConst, size, &result);
      oconstant->setValue(result);
      oconstant->setDecompile(result);
      oconstant->setSize(32);
      oconstant->setConstType(constType);

      ShortRealTypespec *const it = serializer.make<ShortRealTypespec>();
      otypespec = it;
    } else if constexpr (sizeof(T) == 8) {
      format(value, vpiRealConst, size, &result);
      oconstant->setValue(result);
      oconstant->setDecompile(result);
      oconstant->setSize(64);
      oconstant->setConstType(constType);

      RealTypespec *const it = serializer.make<RealTypespec>();
      otypespec = it;
    }
    // } else if constexpr (std::is_same_v<
    //                          std::remove_cv_t<std::remove_reference_t<T>>,
    //                          std::string>) {
    //   oconstant->setValue(value);
    //   oconstant->setDecompile(value);
    //   oconstant->setSize(64);
    //   oconstant->setConstType(constType);
    //
    //   LongIntTypespec *const lit = serializer->make<LongIntTypespec>();
    //   lit->setSigned(true);
    //   otypespec = lit;
  }

  RefTypespec *const rt = serializer.make<RefTypespec>();
  rt->setActual(otypespec);
  oconstant->setTypespec(rt);
  return oconstant;
}

bool ExprEval::getArraySizes(const Typespec *ts, const Any *pany, uint64_t &elemWidth, uint64_t &arrLength) {
  elemWidth = 1;
  arrLength = 1;

  if (ts == nullptr) return false;

  const Typespec *curTs = ts;
  while (curTs) {
    const RangeCollection *ranges = nullptr;
    if (const ArrayTypespec *const arrTs = any_cast<const ArrayTypespec>(curTs)) {
      ranges = arrTs->getRanges();
    } else if (const LogicTypespec *const logicTs = any_cast<const LogicTypespec>(curTs)) {
      ranges = logicTs->getRanges();
    }
    if (ranges == nullptr) break;

    for (const Range *range : *ranges) {
      Expr *leftR = nullptr;
      if (!reduceExpr(range->getLeftExpr(), pany, &leftR, true)) return false;

      Expr *rightR = nullptr;
      if (!reduceExpr(range->getRightExpr(), pany, &rightR, true)) return false;

      const Constant *const lc = any_cast<const Constant>(leftR);
      const Constant *const rc = any_cast<const Constant>(rightR);
      if ((lc == nullptr) || (rc == nullptr)) return false;

      const int64_t lval = std::stoll(std::string(lc->getDecompile()));
      const int64_t rval = std::stoll(std::string(rc->getDecompile()));

      if ((curTs->getUhdmType() == UhdmType::BitTypespec) || (curTs->getUhdmType() == UhdmType::LogicTypespec)) {
        elemWidth *= (lval >= rval) ? (lval - rval + 1) : (rval - lval + 1);
      } else if (curTs->getUhdmType() == UhdmType::ArrayTypespec) {
        arrLength *= (lval >= rval) ? (lval - rval + 1) : (rval - lval + 1);
      }
    }

    curTs = getElemTypespec(curTs);
  }

  return true;
}

bool ExprEval::buildCastConstant(const value_t &value, const Typespec *targetTs, Serializer &serializer, Expr **rexpr) {
  cast_op castOp;
  switch (targetTs->getUhdmType()) {
    case UhdmType::RealTypespec: {
      double r = castOp.operator()<double>(value);
      *rexpr = createConstant<double>(r, serializer, UhdmType::RealTypespec, vpiRealConst, 64);
      return true;
    }

    case UhdmType::ShortRealTypespec: {
      float r = static_cast<float>(castOp.operator()<double>(value));
      *rexpr = createConstant<float>(r, serializer, UhdmType::ShortRealTypespec, vpiRealConst, 32);
      return true;
    }

    case UhdmType::IntTypespec:
    case UhdmType::IntegerTypespec: {
      int32_t i = castOp.operator()<int32_t>(value);
      *rexpr = createConstant<int32_t>(i, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
      return true;
    }

    case UhdmType::LongIntTypespec: {
      int64_t i = castOp.operator()<int64_t>(value);
      *rexpr = createConstant<int64_t>(i, serializer, UhdmType::LongIntTypespec, vpiIntConst, 64);
      return true;
    }

    default: return false;
  }
}

bool ExprEval::reduceCastOp(const Operation *op, const Any *pany, Expr **rexpr) {
  auto operands = op->getOperands();
  if (!operands || operands->size() != 1) return false;

  // target type comes from op->getTypespec()
  const Typespec *targetTs = nullptr;
  if (const RefTypespec *rt = op->getTypespec()) targetTs = rt->getActual();
  if (!targetTs) return false;

  Expr *srcExpr = nullptr;
  if (!reduceExpr(any_cast<Expr>((*operands)[0]), pany, &srcExpr, true)) return false;

  Constant *carg = any_cast<Constant>(srcExpr);
  if (!carg) return false;

  value_t value = parse(carg);
  if (std::holds_alternative<std::monostate>(value)) return false;

  Serializer &serializer = *op->getSerializer();
  return buildCastConstant(value, targetTs, serializer, rexpr);
}

template <typename F>
bool ExprEval::reduceUnaryOp(const Expr *iexpr, const Any *pany, F op, Expr **rexpr) {
  if (iexpr == nullptr) return false;

  Expr *expr = nullptr;
  if (!reduceExpr(iexpr, pany, &expr, true)) return false;

  const value_t value = parse(expr);
  if (std::holds_alternative<std::monostate>(value)) return false;

  const Constant *const iconstant = any_cast<Constant>(expr);
  const Typespec *const itypespec = getTypespec(iconstant);
  const int32_t iconstType = iconstant->getConstType();
  const int32_t isize = iconstant->getSize();

  Serializer &serializer = *expr->getSerializer();
  Constant *oconstant = nullptr;

  if (std::holds_alternative<nvalue_t>(value)) {
    const nvalue_t &nvalue = std::get<nvalue_t>(value);
    switch (nvalue.index()) {
      case 0: {
        const int8_t result = op(std::get<int8_t>(nvalue));
        oconstant =
            createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
      } break;

      case 1: {
        const uint8_t result = op(std::get<uint8_t>(nvalue));
        oconstant =
            createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
      } break;

      case 2: {
        const int16_t result = op(std::get<int16_t>(nvalue));
        oconstant =
            createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
      } break;

      case 3: {
        const uint16_t result = op(std::get<uint16_t>(nvalue));
        oconstant =
            createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
      } break;

      case 4: {
        const int32_t result = op(std::get<int32_t>(nvalue));
        oconstant =
            createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
      } break;

      case 5: {
        const uint32_t result = op(std::get<uint32_t>(nvalue));
        oconstant =
            createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
      } break;

      case 6: {
        const int64_t result = op(std::get<int64_t>(nvalue));
        oconstant =
            createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
      } break;

      case 7: {
        const uint64_t result = op(std::get<uint64_t>(nvalue));

        if (itypespec->getUhdmType() == UhdmType::TimeTypespec) {
          oconstant = serializer.make<Constant>();
          oconstant->setSize(isize);
          oconstant->setConstType(vpiTimeConst);

          std::string sresult;
          if (format(result, vpiTimeConst, isize, &sresult)) {
            oconstant->setValue(sresult);
            oconstant->setDecompile(sresult);
          }

          RefTypespec *const rt = serializer.make<RefTypespec>();
          oconstant->setTypespec(rt);

          TimeTypespec *const it = serializer.make<TimeTypespec>();
          rt->setActual(it);
        } else {
          oconstant =
              createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
        }
      } break;

      default: break;
    }
  } else if (std::holds_alternative<rvalue_t>(value)) {
    const rvalue_t &svalue = std::get<rvalue_t>(value);
    switch (svalue.index()) {
      case 0: {
        const float result = op(std::get<float>(svalue));
        oconstant =
            createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
      } break;

      case 1: {
        const double result = op(std::get<double>(svalue));
        oconstant =
            createConstant(result, serializer, itypespec->getUhdmType(), iconstType, op.boolean_result ? 1 : isize);
      } break;

      default: break;
    }
  } else if (std::holds_alternative<svalue_t>(value)) {
    const svalue_t &svalue = std::get<svalue_t>(value);
    const std::string &sresult = std::get<std::string>(svalue);
    const std::string result = op(std::string_view(sresult));

    oconstant = serializer.make<Constant>();
    oconstant->setValue(result);
    oconstant->setDecompile(result);
    oconstant->setSize(isize);
    oconstant->setConstType(iconstType);

    RefTypespec *const rt = serializer.make<RefTypespec>();
    oconstant->setTypespec(rt);
    if (itypespec->getUhdmType() == UhdmType::LogicTypespec) {
      LogicTypespec *const lts = serializer.make<LogicTypespec>();
      rt->setActual(lts);
      lts->setSigned(getSigned(iconstant->getTypespec()->getActual()));
    } else if (itypespec->getUhdmType() == UhdmType::IntegerTypespec) {
      IntegerTypespec *const its = serializer.make<IntegerTypespec>();
      rt->setActual(its);
      its->setSigned(getSigned(iconstant->getTypespec()->getActual()));
    }
  }

  *rexpr = oconstant;
  return (oconstant != nullptr);
}

template <typename F>
bool ExprEval::reduceBinaryOp(const Expr *iexpr0, const Expr *iexpr1, const Any *pany, F op, Expr **rexpr) {
  if ((iexpr0 == nullptr) || (iexpr1 == nullptr)) return false;

  Expr *expr0 = nullptr;
  if (!reduceExpr(iexpr0, pany, &expr0, true)) return false;

  Expr *expr1 = nullptr;
  if (!reduceExpr(iexpr1, pany, &expr1, true)) return false;

  value_t value0 = parse(expr0);
  if (std::holds_alternative<std::monostate>(value0)) return false;

  value_t value1 = parse(expr1);
  if (std::holds_alternative<std::monostate>(value1)) return false;

  const Constant *const iconstant0 = any_cast<Constant>(expr0);
  const Typespec *const itypespec0 = getTypespec(iconstant0);
  const Constant *const iconstant1 = any_cast<Constant>(expr1);
  const Typespec *const itypespec1 = getTypespec(iconstant1);

  UhdmType rtype = promoted(itypespec0->getUhdmType(), value0, itypespec1->getUhdmType(), value1);

  Serializer *const serializer = iexpr0->getSerializer();
  Constant *oconstant = nullptr;
  Typespec *otypespec = nullptr;

  if (std::holds_alternative<nvalue_t>(value0) && std::holds_alternative<nvalue_t>(value1)) {
    const nvalue_t &nvalue0 = std::get<nvalue_t>(value0);
    const nvalue_t &nvalue1 = std::get<nvalue_t>(value1);

    switch (std::max(nvalue0.index(), nvalue1.index())) {
      case 0:
      case 1: {
        const bool signed0 = (nvalue0.index() % 2) == 0;
        const bool signed1 = (nvalue1.index() % 2) == 0;

        int64_t iarg0 = 0, iarg1 = 0;
        uint64_t uarg0 = 0, uarg1 = 0;

        if (signed0) {
          std::visit([&iarg0](auto &&arg) { iarg0 = arg; }, nvalue0);
        } else {
          std::visit([&uarg0](auto &&arg) { uarg0 = arg; }, nvalue0);
        }

        if (signed1) {
          std::visit([&iarg1](auto &&arg) { iarg1 = arg; }, nvalue1);
        } else {
          std::visit([&uarg1](auto &&arg) { uarg1 = arg; }, nvalue1);
        }

        oconstant = serializer->make<Constant>();
        oconstant->setSize(8);

        ByteTypespec *const it = serializer->make<ByteTypespec>();
        otypespec = it;

        if (signed0 && signed1) {
          // Both operands are signed -> result is signed
          const int8_t iresult = static_cast<int32_t>(op(iarg0, iarg1));
          const std::string result = std::to_string(iresult);
          oconstant->setValue(result);
          oconstant->setDecompile(result);
          oconstant->setConstType(vpiIntConst);
          it->setSigned(true);
        } else if (signed0 && !signed1) {
          // Op0 is signed, Op1 is unsigned -> result is unsinged
          const uint8_t uresult = op(iarg0, uarg1);
          const std::string result = std::to_string(uresult);
          oconstant->setValue(result);
          oconstant->setDecompile(result);
          oconstant->setConstType(vpiUIntConst);
          it->setSigned(false);
        } else if (!signed0 && signed1) {
          // Op0 is unsigned, Op1 is signed -> result is unsigned
          const uint8_t uresult = op(uarg0, iarg1);
          const std::string result = std::to_string(uresult);
          oconstant->setValue(result);
          oconstant->setDecompile(result);
          oconstant->setConstType(vpiUIntConst);
          it->setSigned(false);
        } else {
          // Both operands are unsigned -> result is unsigned
          const uint8_t uresult = op(uarg0, uarg1);
          const std::string result = std::to_string(uresult);
          oconstant->setValue(result);
          oconstant->setDecompile(result);
          oconstant->setConstType(vpiUIntConst);
          it->setSigned(false);
        }
      } break;

      case 2:
      case 3: {
        const bool signed0 = (nvalue0.index() % 2) == 0;
        const bool signed1 = (nvalue1.index() % 2) == 0;

        int16_t iarg0 = 0, iarg1 = 0;
        uint16_t uarg0 = 0, uarg1 = 0;

        if (signed0) {
          std::visit([&iarg0](auto &&arg) { iarg0 = arg; }, nvalue0);
        } else {
          std::visit([&uarg0](auto &&arg) { uarg0 = arg; }, nvalue0);
        }

        if (signed1) {
          std::visit([&iarg1](auto &&arg) { iarg1 = arg; }, nvalue1);
        } else {
          std::visit([&uarg1](auto &&arg) { uarg1 = arg; }, nvalue1);
        }

        oconstant = serializer->make<Constant>();
        oconstant->setSize(16);

        ShortIntTypespec *const it = serializer->make<ShortIntTypespec>();
        otypespec = it;

        if (signed0 && signed1) {
          // Both operands are signed -> result is signed
          const int16_t iresult = op(iarg0, iarg1);
          const std::string result = std::to_string(iresult);
          oconstant->setValue(result);
          oconstant->setDecompile(result);
          oconstant->setConstType(vpiIntConst);
          it->setSigned(true);
        } else if (signed0 && !signed1) {
          // Op0 is signed, Op1 is unsigned -> result is unsinged
          const uint16_t uresult = op(iarg0, uarg1);
          const std::string result = std::to_string(uresult);
          oconstant->setValue(result);
          oconstant->setDecompile(result);
          oconstant->setConstType(vpiUIntConst);
          it->setSigned(false);
        } else if (!signed0 && signed1) {
          // Op0 is unsigned, Op1 is signed -> result is unsigned
          const uint16_t uresult = op(uarg0, iarg1);
          const std::string result = std::to_string(uresult);
          oconstant->setValue(result);
          oconstant->setDecompile(result);
          oconstant->setConstType(vpiUIntConst);
          it->setSigned(false);
        } else {
          // Both operands are unsigned -> result is unsigned
          const uint16_t uresult = op(uarg0, uarg1);
          const std::string result = std::to_string(uresult);
          oconstant->setValue(result);
          oconstant->setDecompile(result);
          oconstant->setConstType(vpiUIntConst);
          it->setSigned(false);
        }
      } break;

      case 4:
      case 5: {
        const bool signed0 = (nvalue0.index() % 2) == 0;
        const bool signed1 = (nvalue1.index() % 2) == 0;

        int32_t iarg0 = 0, iarg1 = 0;
        uint32_t uarg0 = 0, uarg1 = 0;

        if (signed0) {
          std::visit([&iarg0](auto &&arg) { iarg0 = arg; }, nvalue0);
        } else {
          std::visit([&uarg0](auto &&arg) { uarg0 = arg; }, nvalue0);
        }

        if (signed1) {
          std::visit([&iarg1](auto &&arg) { iarg1 = arg; }, nvalue1);
        } else {
          std::visit([&uarg1](auto &&arg) { uarg1 = arg; }, nvalue1);
        }
        std::string sresult;
        if (isFourState(itypespec0)) {
          oconstant = serializer->make<Constant>();
          const int32_t isize = std::max(iconstant0->getSize(), iconstant1->getSize());
          if (itypespec0->getUhdmType() == UhdmType::IntegerTypespec) {
            IntegerTypespec *const it = serializer->make<IntegerTypespec>();
            otypespec = it;

            if (signed0 && signed1) {
              // Both operands are signed -> result is signed
              const int32_t iresult = op(iarg0, iarg1);
              format(iresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(true);
            } else if (signed0 && !signed1) {
              // Op0 is signed, Op1 is unsigned -> result is unsinged
              const uint32_t uresult = op(iarg0, uarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            } else if (!signed0 && signed1) {
              // Op0 is unsigned, Op1 is signed -> result is unsigned
              const uint32_t uresult = op(uarg0, iarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            } else {
              // Both operands are unsigned -> result is unsigned
              const uint32_t uresult = op(uarg0, uarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            }
          } else {
            LogicTypespec *const it = serializer->make<LogicTypespec>();
            otypespec = it;

            if (signed0 && signed1) {
              // Both operands are signed -> result is signed
              const int32_t iresult = op(iarg0, iarg1);
              format(iresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(true);
            } else if (signed0 && !signed1) {
              // Op0 is signed, Op1 is unsigned -> result is unsinged
              const uint32_t uresult = op(iarg0, uarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            } else if (!signed0 && signed1) {
              // Op0 is unsigned, Op1 is signed -> result is unsigned
              const uint32_t uresult = op(uarg0, iarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            } else {
              // Both operands are unsigned -> result is unsigned
              const uint32_t uresult = op(uarg0, uarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            }
          }
          oconstant->setDecompile(sresult);
          oconstant->setValue(sresult);
          oconstant->setSize(sresult.length());
          oconstant->setConstType(vpiBinaryConst);

        } else {
          oconstant = serializer->make<Constant>();
          oconstant->setSize(32);

          IntTypespec *const it = serializer->make<IntTypespec>();
          otypespec = it;

          if (signed0 && signed1) {
            // Both operands are signed -> result is signed
            const int32_t iresult = op(iarg0, iarg1);
            const std::string result = std::to_string(iresult);
            oconstant->setValue(result);
            oconstant->setDecompile(result);
            oconstant->setConstType(vpiIntConst);
            it->setSigned(true);
          } else if (signed0 && !signed1) {
            // Op0 is signed, Op1 is unsigned -> result is unsinged
            const uint32_t uresult = op(iarg0, uarg1);
            const std::string result = std::to_string(uresult);
            oconstant->setValue(result);
            oconstant->setDecompile(result);
            oconstant->setConstType(vpiUIntConst);
            it->setSigned(false);
          } else if (!signed0 && signed1) {
            // Op0 is unsigned, Op1 is signed -> result is unsigned
            const uint32_t uresult = op(uarg0, iarg1);
            const std::string result = std::to_string(uresult);
            oconstant->setValue(result);
            oconstant->setDecompile(result);
            oconstant->setConstType(vpiUIntConst);
            it->setSigned(false);
          } else {
            // Both operands are unsigned -> result is unsigned
            const uint32_t uresult = op(uarg0, uarg1);
            const std::string result = std::to_string(uresult);
            oconstant->setValue(result);
            oconstant->setDecompile(result);
            oconstant->setConstType(vpiUIntConst);
            it->setSigned(false);
          }
        }

      } break;

      case 6:
      case 7: {
        const bool signed0 = (nvalue0.index() % 2) == 0;
        const bool signed1 = (nvalue1.index() % 2) == 0;

        int64_t iarg0 = 0, iarg1 = 0;
        uint64_t uarg0 = 0, uarg1 = 0;

        if (signed0) {
          std::visit([&iarg0](auto &&arg) { iarg0 = arg; }, nvalue0);
        } else {
          std::visit([&uarg0](auto &&arg) { uarg0 = arg; }, nvalue0);
        }

        if (signed1) {
          std::visit([&iarg1](auto &&arg) { iarg1 = arg; }, nvalue1);
        } else {
          std::visit([&uarg1](auto &&arg) { uarg1 = arg; }, nvalue1);
        }
        std::string sresult;
        if (isFourState(itypespec0)) {
          oconstant = serializer->make<Constant>();
          const int32_t isize = std::max(iconstant0->getSize(), iconstant1->getSize());
          if (rtype == UhdmType::IntegerTypespec) {
            IntegerTypespec *const it = serializer->make<IntegerTypespec>();
            otypespec = it;

            if (signed0 && signed1) {
              // Both operands are signed -> result is signed
              const int64_t iresult = op(iarg0, iarg1);
              format(iresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(true);
            } else if (signed0 && !signed1) {
              // Op0 is signed, Op1 is unsigned -> result is unsinged
              const uint64_t uresult = op(iarg0, uarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            } else if (!signed0 && signed1) {
              // Op0 is unsigned, Op1 is signed -> result is unsigned
              const uint64_t uresult = op(uarg0, iarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            } else {
              // Both operands are unsigned -> result is unsigned
              const uint64_t uresult = op(uarg0, uarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            }
          } else {
            LogicTypespec *const it = serializer->make<LogicTypespec>();
            otypespec = it;

            if (signed0 && signed1) {
              // Both operands are signed -> result is signed
              const int64_t iresult = op(iarg0, iarg1);
              format(iresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(true);
            } else if (signed0 && !signed1) {
              // Op0 is signed, Op1 is unsigned -> result is unsinged
              const uint64_t uresult = op(iarg0, uarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            } else if (!signed0 && signed1) {
              // Op0 is unsigned, Op1 is signed -> result is unsigned
              const uint64_t uresult = op(uarg0, iarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            } else {
              // Both operands are unsigned -> result is unsigned
              const uint64_t uresult = op(uarg0, uarg1);
              format(uresult, vpiBinaryConst, op.boolean_result ? 1 : isize, &sresult);
              it->setSigned(false);
            }
          }
          oconstant->setDecompile(sresult);
          oconstant->setValue(sresult);
          oconstant->setSize(sresult.length());
          oconstant->setConstType(vpiBinaryConst);

        } else {
          oconstant = serializer->make<Constant>();
          oconstant->setSize(64);
          if (rtype == UhdmType::LongIntTypespec) {
            LongIntTypespec *const it = serializer->make<LongIntTypespec>();
            otypespec = it;

            if (signed0 && signed1) {
              // Both operands are signed -> result is signed
              const int64_t iresult = op(iarg0, iarg1);
              const std::string result = std::to_string(iresult);
              oconstant->setValue(result);
              oconstant->setDecompile(result);
              oconstant->setConstType(vpiIntConst);
              it->setSigned(true);
            } else if (signed0 && !signed1) {
              // Op0 is signed, Op1 is unsigned -> result is unsinged
              const uint64_t uresult = op(iarg0, uarg1);
              const std::string result = std::to_string(uresult);
              oconstant->setValue(result);
              oconstant->setDecompile(result);
              oconstant->setConstType(vpiUIntConst);
              it->setSigned(false);
            } else if (!signed0 && signed1) {
              // Op0 is unsigned, Op1 is signed -> result is unsigned
              const uint64_t uresult = op(uarg0, iarg1);
              const std::string result = std::to_string(uresult);
              oconstant->setValue(result);
              oconstant->setDecompile(result);
              oconstant->setConstType(vpiUIntConst);
              it->setSigned(false);
            } else {
              // Both operands are unsigned -> result is unsigned
              const uint64_t uresult = op(uarg0, uarg1);
              const std::string result = std::to_string(uresult);
              oconstant->setValue(result);
              oconstant->setDecompile(result);
              oconstant->setConstType(vpiUIntConst);
              it->setSigned(false);
            }
          } else if (rtype == UhdmType::TimeTypespec) {
            TimeTypespec *const it = serializer->make<TimeTypespec>();
            otypespec = it;
            if (signed0 && signed1) {
              // Both operands are signed -> result is signed
              const int64_t iresult = op(iarg0, iarg1);
              const std::string result = std::to_string(iresult);
              oconstant->setValue(result);
              oconstant->setDecompile(result);
              oconstant->setConstType(vpiTimeConst);
            } else if (signed0 && !signed1) {
              // Op0 is signed, Op1 is unsigned -> result is unsinged
              const uint64_t uresult = op(iarg0, uarg1);
              const std::string result = std::to_string(uresult);
              oconstant->setValue(result);
              oconstant->setDecompile(result);
              oconstant->setConstType(vpiTimeConst);
            } else if (!signed0 && signed1) {
              // Op0 is unsigned, Op1 is signed -> result is unsigned
              const uint64_t uresult = op(uarg0, iarg1);
              const std::string result = std::to_string(uresult);
              oconstant->setValue(result);
              oconstant->setDecompile(result);
              oconstant->setConstType(vpiTimeConst);
            } else {
              // Both operands are unsigned -> result is unsigned
              const uint64_t uresult = op(uarg0, uarg1);
              const std::string result = std::to_string(uresult);
              oconstant->setValue(result);
              oconstant->setDecompile(result);
              oconstant->setConstType(vpiTimeConst);
            }
          }
        }
      } break;

      default: break;
    }
  } else if (std::holds_alternative<rvalue_t>(value0) && std::holds_alternative<rvalue_t>(value1)) {
    const rvalue_t &rvalue0 = std::get<rvalue_t>(value0);
    const rvalue_t &rvalue1 = std::get<rvalue_t>(value1);
    switch (std::max(rvalue0.index(), rvalue1.index())) {
      case 0: {
        float v0 = std::get<float>(rvalue0);
        float v1 = std::get<float>(rvalue1);
        const float result = op(v0, v1);
        std::string sresult;
        format(result, vpiRealConst, 32, &sresult);
        oconstant = serializer->make<Constant>();
        oconstant->setValue(sresult);
        oconstant->setDecompile(sresult);
        oconstant->setSize(32);
        oconstant->setConstType(vpiRealConst);

        ShortRealTypespec *const it = serializer->make<ShortRealTypespec>();
        otypespec = it;
      } break;

      case 1: {
        double v0 = std::get<double>(rvalue0);
        double v1 = std::get<double>(rvalue1);
        const double result = op(v0, v1);
        std::string sresult;
        format(result, vpiRealConst, 64, &sresult);
        oconstant = serializer->make<Constant>();
        oconstant->setValue(sresult);
        oconstant->setDecompile(sresult);
        oconstant->setSize(32);
        oconstant->setConstType(vpiRealConst);

        RealTypespec *const it = serializer->make<RealTypespec>();
        otypespec = it;
      } break;

      default: break;
    }
  } else if (std::holds_alternative<svalue_t>(value0) && std::holds_alternative<svalue_t>(value1)) {
    const svalue_t &svalue0 = std::get<svalue_t>(value0);
    const svalue_t &svalue1 = std::get<svalue_t>(value1);
    std::string v0 = std::get<std::string>(svalue0);
    std::string v1 = std::get<std::string>(svalue1);
    const std::string result = op(v0, v1);
    oconstant = serializer->make<Constant>();
    oconstant->setValue(result);
    oconstant->setDecompile(result);
    oconstant->setSize(32);
    oconstant->setConstType(vpiBinaryConst);
    if (rtype == UhdmType::LogicTypespec) {
      LogicTypespec *const lts = serializer->make<LogicTypespec>();
      lts->setSigned(getSigned(iconstant0->getTypespec()->getActual()));
      otypespec = lts;
    } else if (rtype == UhdmType::IntegerTypespec) {
      IntegerTypespec *const its = serializer->make<IntegerTypespec>();
      its->setSigned(getSigned(iconstant0->getTypespec()->getActual()));
      otypespec = its;
    }
  }

  if ((oconstant != nullptr) && (otypespec != nullptr)) {
    RefTypespec *const rt = serializer->make<RefTypespec>();
    rt->setActual(otypespec);
    oconstant->setTypespec(rt);
    *rexpr = oconstant;
    return true;
  }

  return false;
}

bool ExprEval::reduceConcatOp(const AnyCollection &operands, const Any *pany, Expr **rexpr) {
  if (operands.empty()) return false;

  std::string value;
  binary_concat_op f;
  for (const Any *op : operands) {
    Expr *expr = nullptr;
    if (!reduceExpr(any_cast<Expr>(op), pany, &expr, true)) {
      return false;
    }

    value = f(value, parseBinary(expr));
  }

  Serializer *const serializer = pany->getSerializer();
  Constant *const oconstant = serializer->make<Constant>();
  oconstant->setSize(value.length());
  oconstant->setValue(value);
  oconstant->setDecompile(value);
  oconstant->setConstType(vpiBinaryConst);

  // if (value.length() >= 32) {
  //   LongIntTypespec *const lit = serializer->make<LongIntTypespec>();
  //   otypespec = lit;
  // } else {
  //   IntTypespec *const it = serializer->make<IntTypespec>();
  //   otypespec = it;
  // }

  RefTypespec *const rt = serializer->make<RefTypespec>();
  oconstant->setTypespec(rt);

  LogicTypespec *const lt = serializer->make<LogicTypespec>();
  lt->setSigned(false);
  rt->setActual(lt);

  *rexpr = oconstant;
  return true;
}

bool ExprEval::reduceReplicationOp(const Expr *iexpr0, const Expr *iexpr1, const Any *pany, Expr **rexpr) {
  if ((iexpr0 == nullptr) || (iexpr1 == nullptr)) return false;

  Expr *expr0 = nullptr;
  if (!reduceExpr(iexpr0, pany, &expr0, true)) return false;

  Expr *expr1 = nullptr;
  if (!reduceExpr(iexpr1, pany, &expr1, true)) return false;

  const Constant *const constant = any_cast<Constant>(expr0);
  const std::string_view sv = constant->getDecompile();
  const int64_t value0 = std::stoi(std::string(sv));

  binary_replicate_op f;
  const std::string value = f(value0, parseBinary(expr1));

  Serializer *const serializer = pany->getSerializer();
  Constant *const oconstant = serializer->make<Constant>();
  oconstant->setSize(value.length());
  oconstant->setValue(value);
  oconstant->setDecompile(value);
  oconstant->setConstType(vpiBinaryConst);

  RefTypespec *const rt = serializer->make<RefTypespec>();
  oconstant->setTypespec(rt);

  LogicTypespec *const lt = serializer->make<LogicTypespec>();
  lt->setSigned(false);
  rt->setActual(lt);

  *rexpr = oconstant;
  return true;
}

bool ExprEval::reduceUnaryReplicationOp(const Expr *iexpr, const Any *pany, Expr **rexpr) {
  if (iexpr == nullptr) return false;

  Expr *expr = nullptr;
  if (!reduceExpr(iexpr, pany, &expr, true)) return false;

  const Constant *const constant = any_cast<Constant>(expr);
  if (constant == nullptr) return false;

  const std::string_view value = constant->getDecompile();
  const size_t size = constant->getSize();

  unary_replicate_extend_op f;
  const std::string result = f(size, value);

  Serializer *const serializer = iexpr->getSerializer();
  Constant *const oconstant = serializer->make<Constant>();
  oconstant->setSize(result.length());
  oconstant->setValue(result);
  oconstant->setDecompile(result);
  oconstant->setConstType(vpiBinaryConst);

  RefTypespec *const rt = serializer->make<RefTypespec>();
  oconstant->setTypespec(rt);

  LogicTypespec *const lt = serializer->make<LogicTypespec>();
  lt->setSigned(false);
  rt->setActual(lt);

  *rexpr = oconstant;
  return true;
}

bool ExprEval::reduceConditionalOp(const Expr *iexpr0, const Expr *iexpr1, const Expr *iexpr2, const Any *pany,
                                   Expr **rexpr) {
  if ((iexpr0 == nullptr) || (iexpr1 == nullptr) || (iexpr2 == nullptr)) return false;

  Expr *expr0 = nullptr;
  if (!reduceExpr(iexpr0, pany, &expr0, true)) return false;

  const Constant *const condConst = any_cast<Constant>(expr0);
  if (condConst == nullptr) return false;

  std::string_view condSv = condConst->getDecompile();
  const int64_t condValue = std::stoll(std::string(condSv));

  const Expr *const selectedExpr = (condValue != 0) ? iexpr1 : iexpr2;

  Expr *rexprSel = nullptr;
  if (!reduceExpr(selectedExpr, pany, &rexprSel, true)) return false;

  const Constant *const valueConst = any_cast<Constant>(rexprSel);
  if (valueConst == nullptr) return false;

  std::string_view value = valueConst->getDecompile();

  Serializer *const serializer = pany->getSerializer();
  Constant *const oconstant = serializer->make<Constant>();
  oconstant->setSize(value.size());
  oconstant->setValue(value);
  oconstant->setDecompile(value);
  oconstant->setConstType(vpiBinaryConst);

  RefTypespec *const rt = serializer->make<RefTypespec>();
  oconstant->setTypespec(rt);

  LogicTypespec *const lt = serializer->make<LogicTypespec>();
  lt->setSigned(false);
  rt->setActual(lt);

  *rexpr = oconstant;
  return true;
}

bool ExprEval::reduceCaseEqOp(const Expr *iexpr0, const Expr *iexpr1, const Any *pany, Expr **rexpr) {
  if ((iexpr0 == nullptr) || (iexpr1 == nullptr)) return false;

  Expr *expr0 = nullptr;
  Expr *expr1 = nullptr;

  if (!reduceExpr(iexpr0, pany, &expr0, true) || !reduceExpr(iexpr1, pany, &expr1, true)) return false;

  const std::string a = parseBinary(expr0);
  const std::string b = parseBinary(expr1);
  const std::string value = binary_case_eq_op()(a, b);

  Serializer *const serializer = pany->getSerializer();
  Constant *const oconstant = serializer->make<Constant>();
  oconstant->setValue(value);
  oconstant->setDecompile(value);
  oconstant->setConstType(vpiBinaryConst);
  oconstant->setSize(1);

  RefTypespec *const rt = serializer->make<RefTypespec>();
  oconstant->setTypespec(rt);

  LogicTypespec *const lt = serializer->make<LogicTypespec>();
  lt->setSigned(false);
  rt->setActual(lt);

  *rexpr = oconstant;
  return true;
}

bool ExprEval::reduceCaseNeqOp(const Expr *iexpr0, const Expr *iexpr1, const Any *pany, Expr **rexpr) {
  if ((iexpr0 == nullptr) || (iexpr1 == nullptr)) return false;

  Expr *expr0 = nullptr;
  Expr *expr1 = nullptr;

  if (!reduceExpr(iexpr0, pany, &expr0, true) || !reduceExpr(iexpr1, pany, &expr1, true)) return false;

  const std::string a = parseBinary(expr0);
  const std::string b = parseBinary(expr1);

  binary_case_neq_op f;
  std::string value = f(a, b);

  Serializer *const serializer = pany->getSerializer();
  Constant *const oconstant = serializer->make<Constant>();
  oconstant->setValue(value);
  oconstant->setDecompile(value);
  oconstant->setConstType(vpiBinaryConst);
  oconstant->setSize(1);

  RefTypespec *rt = serializer->make<RefTypespec>();
  oconstant->setTypespec(rt);

  LogicTypespec *lt = serializer->make<LogicTypespec>();
  lt->setSigned(false);
  rt->setActual(lt);

  *rexpr = oconstant;
  return true;
}

bool ExprEval::reduceTaggedPattern(const TaggedPattern *tp, const Any *pany, std::vector<const Any *> *result) {
  if ((tp == nullptr) || (pany == nullptr)) return false;

  Serializer *const serializer = pany->getSerializer();

  const RefTypespec *const rt = tp->getTypespec();
  if (rt == nullptr) return false;

  uint64_t elemWidth = 1;
  uint64_t arrLength = 1;
  if (!getArraySizes(rt->getActual(), pany, elemWidth, arrLength)) {
    return false;
  }

  const Expr *const valueExpr = tp->getPattern<Expr>();

  Expr *reducedValue = nullptr;
  if (!reduceExpr(valueExpr, pany, &reducedValue, true)) {
    return false;
  }

  Constant *const reducedConstant = any_cast<Constant>(reducedValue);
  if (reducedConstant == nullptr) return false;

  std::string elemBits(reducedConstant->getDecompile());
  if (elemBits.size() < elemWidth) {
    elemBits = std::string(elemWidth - elemBits.size(), '0') + elemBits;
  }

  result->clear();
  result->reserve(arrLength);

  Typespec *const typespec = const_cast<Typespec *>(rt->getActual());
  for (size_t i = 0; i < arrLength; ++i) {
    Constant *c = serializer->make<Constant>();
    c->setSize(elemWidth);
    c->setValue(elemBits);
    c->setDecompile(elemBits);
    c->setConstType(vpiBinaryConst);

    RefTypespec *const crt = serializer->make<RefTypespec>();
    crt->setActual(typespec);
    crt->setParent(c);
    c->setTypespec(crt);
    result->emplace_back(c);
  }

  return true;
}

bool ExprEval::reduceOperation(const Operation *operation, const Any *pany, Expr **rexpr, bool muteError) {
  for (auto t : m_skipOperationTypes) {
    if (operation->getOpType() == t) {
      *rexpr = const_cast<Operation *>(operation);
      return true;
    }
  }

  AnyCollection *const operandsCollection = operation->getOperands();
  if (operandsCollection == nullptr) {
    *rexpr = const_cast<Operation *>(operation);
    return true;
  }

  const AnyCollection &operands = *operandsCollection;
  for (const Any *operand : operands) {
    const UhdmType operandType = operand->getUhdmType();
    if (operandType == UhdmType::RefObj) {
      const RefObj *const ro = static_cast<const RefObj *>(operand);
      if ((ro->getName() == "default") && ro->getStructMember()) continue;

      Expr *ero = nullptr;
      if (!reduceExpr(ro, pany, &ero, true)) {
        return false;
      }

      const value_t value = parse(ero);
      if (std::holds_alternative<std::monostate>(value)) {
        return false;
      }
    } else if (operandType != UhdmType::Constant) {
      return false;
    } else if (operandType == UhdmType::Variable) {
      if (getTypespec<EnumTypespec>(operand) != nullptr) {
        return false;
      }
    }
  }

  bool succeeded = true;
  Serializer &serializer = *operation->getSerializer();

  const int32_t opType = operation->getOpType();
  if (operands.size() == 1) {
    switch (opType) {
      case vpiPostIncOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_postinc_op(), rexpr);
      } break;

      case vpiPostDecOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_postdec_op(), rexpr);
      } break;

      case vpiPreDecOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_predec_op(), rexpr);
      } break;

      case vpiPreIncOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_preinc_op(), rexpr);
      } break;

      case vpiPlusOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, identity(), rexpr);
      } break;

      case vpiMinusOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_negate(), rexpr);
      } break;

      case vpiBitNegOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_bit_not(), rexpr);
      } break;

      case vpiNotOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_logical_not(), rexpr);
        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiInsideOp: {
        Expr *op0 = nullptr;
        if (!reduceExpr(any_cast<Expr>(operands[0]), pany, &op0, muteError)) {
          succeeded = false;
          break;
        }

        int64_t val0 = 0;
        if (!getInt64(op0, &val0)) {
          succeeded = false;
          break;
        }

        for (uint32_t i = 1, ni = operands.size(); (i < ni) && succeeded; ++i) {
          Expr *opi = nullptr;
          if (!reduceExpr(any_cast<Expr>(operands[i]), pany, &opi, muteError)) {
            succeeded = false;
            break;
          }

          int64_t vali = 0;
          if (getInt64(opi, &vali) && (vali == val0)) {
            Constant *const c = serializer.make<Constant>();
            c->setValue("1");
            c->setDecompile("1");
            c->setSize(64);
            c->setConstType(vpiUIntConst);
            *rexpr = c;
            break;
          } else {
            succeeded = false;
          }
        }
      } break;

      case vpiUnaryAndOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_and_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiUnaryNandOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_nand_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiUnaryOrOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_or_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiUnaryNorOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_nor_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiUnaryXorOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_xor_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiUnaryXNorOp: {
        succeeded = reduceUnaryOp(any_cast<Expr>(operands[0]), pany, unary_xnor_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiCastOp: {
        succeeded = reduceCastOp(operation, pany, rexpr);
      } break;

      case vpiMultiConcatOp: {
        succeeded = reduceUnaryReplicationOp(any_cast<Expr>(operands[0]), pany, rexpr);
      } break;

      default: break;
    }
  } else if (operands.size() == 2) {
    switch (opType) {
      case vpiArithRShiftOp: {
        succeeded = reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany,
                                   binary_arith_rshift_op(), rexpr);
      } break;

      case vpiRShiftOp: {
        succeeded = reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany,
                                   binary_logical_rshift_op(), rexpr);
      } break;

      case vpiEqOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_equal(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiNeqOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_not_equal(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiGtOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_greater(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiGeOp: {
        succeeded = reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany,
                                   binary_greater_equal(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiLtOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_less(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiLeOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_less_equal(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiCaseEqOp: {
        succeeded = reduceCaseEqOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiCaseNeqOp: {
        succeeded = reduceCaseNeqOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiArithLShiftOp: {
        succeeded = reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany,
                                   binary_arith_lshift_op(), rexpr);
      } break;

      case vpiLShiftOp: {
        succeeded = reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany,
                                   binary_logical_lshift_op(), rexpr);
      } break;

      case vpiAddOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_plus(), rexpr);
      } break;

      case vpiBitOrOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_bit_or(), rexpr);
      } break;

      case vpiBitXorOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_bit_xor(), rexpr);
      } break;

      case vpiBitXNorOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_xnor_op(), rexpr);
      } break;

      case vpiBitAndOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_bit_and(), rexpr);
      } break;

      case vpiImplyOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_imply_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiNonOverlapImplyOp: {
        succeeded = reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany,
                                   binary_non_overlap_imply_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiOverlapImplyOp: {
        succeeded = reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany,
                                   binary_overlap_imply_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiLogOrOp: {
        succeeded = reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany,
                                   binary_logical_or_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiLogAndOp: {
        succeeded = reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany,
                                   binary_logical_and_op(), rexpr);

        if (succeeded && (*rexpr != nullptr)) {
          Constant *const c = any_cast<Constant>(*rexpr);
          c->setValue(c->getDecompile());

          LogicTypespec *const lts = serializer.make<LogicTypespec>();
          lts->setSigned(getSigned(c->getTypespec()->getActual()));
          c->getTypespec()->setActual(lts, true);
          c->setSize(1);
        }
      } break;

      case vpiSubOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_minus(), rexpr);
      } break;

      case vpiMultOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_multiplies(), rexpr);
      } break;

      case vpiUnaryNandOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_nand_op(), rexpr);
      } break;

      case vpiUnaryNorOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_nor_op(), rexpr);
      } break;

      case vpiUnaryXNorOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_xnor_op(), rexpr);
      } break;

      case vpiModOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_modulus_op(), rexpr);
      } break;

      case vpiPowerOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_power_op(), rexpr);
      } break;

      case vpiDivOp: {
        succeeded =
            reduceBinaryOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, binary_divides(), rexpr);
      } break;

      case vpiConcatOp: {
        succeeded = reduceConcatOp(operands, pany, rexpr);
      } break;

      case vpiMultiConcatOp: {
        succeeded = reduceReplicationOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]), pany, rexpr);
      } break;

      default: break;
    }

  } else if (operands.size() == 3) {
    switch (opType) {
      case vpiConditionOp: {
        succeeded = reduceConditionalOp(any_cast<Expr>(operands[0]), any_cast<Expr>(operands[1]),
                                        any_cast<Expr>(operands[2]), pany, rexpr);
      } break;

      case vpiConcatOp: {
        succeeded = reduceConcatOp(operands, pany, rexpr);
      } break;

      default: {
        succeeded = false;
      } break;
    }
  } else {
    switch (opType) {
      case vpiConcatOp: {
        succeeded = reduceConcatOp(operands, pany, rexpr);
      } break;

      case vpiMultiAssignmentPatternOp:
      case vpiAssignmentPatternOp: {
        // Don't reduce these ops
        succeeded = false;
      } break;

      default: {
        succeeded = false;
      } break;
    }
  }
  succeeded = succeeded && (*rexpr != nullptr);
  return succeeded;
}

bool ExprEval::reduceMathSysFunc(const SysFuncCall *call, const Any *pany, Expr **rexpr, bool muteError) {
  if (call->getArguments() == nullptr) return false;

  cast_op castOp;
  std::vector<double> args;
  for (const Any *arg : *call->getArguments()) {
    Constant *carg = nullptr;
    if (const Expr *argExpr = any_cast<Expr>(arg)) {
      Expr *rarg = nullptr;
      if (reduceExpr(argExpr, pany, &rarg, muteError)) {
        carg = any_cast<Constant>(rarg);
      }
    }

    if (carg == nullptr) return false;

    const value_t value = parse(carg);
    if (std::holds_alternative<std::monostate>(value)) return false;

    args.emplace_back(castOp.operator()<double>(value));
  }

  auto makeReal = [&](double v) -> bool {
    *rexpr = createConstant(v, *call->getSerializer(), UhdmType::RealTypespec, vpiRealConst, 0);
    return (*rexpr != nullptr);
  };

  std::string_view name = call->getName();

  if (args.size() == 1) {
    const double a = args[0];

    if (name == "$sin") return makeReal(std::sin(a));
    if (name == "$cos") return makeReal(std::cos(a));
    if (name == "$tan") return makeReal(std::tan(a));
    if (name == "$asin") return makeReal(std::asin(a));
    if (name == "$acos") return makeReal(std::acos(a));
    if (name == "$atan") return makeReal(std::atan(a));

    if (name == "$sinh") return makeReal(std::sinh(a));
    if (name == "$cosh") return makeReal(std::cosh(a));
    if (name == "$tanh") return makeReal(std::tanh(a));
    if (name == "$asinh") return makeReal(std::asinh(a));
    if (name == "$acosh") return makeReal(std::acosh(a));
    if (name == "$atanh") return makeReal(std::atanh(a));

    if (name == "$ln") return makeReal(std::log(a));
    if (name == "$log10") return makeReal(std::log10(a));
    if (name == "$exp") return makeReal(std::exp(a));
    if (name == "$sqrt") return makeReal(std::sqrt(a));

    if (name == "$floor") return makeReal(std::floor(a));
    if (name == "$ceil") return makeReal(std::ceil(a));
    if (name == "$clog2") return makeReal(std::ceil(std::log2(a)));
  }

  if (args.size() == 2) {
    const double a = args[0];
    const double b = args[1];

    if (name == "$pow") return makeReal(std::pow(a, b));
    if (name == "$atan2") return makeReal(std::atan2(a, b));
    if (name == "$hypot") return makeReal(std::hypot(a, b));
  }

  return false;
}

bool ExprEval::reduceConvSysFunc(const SysFuncCall *call, const Any *pany, Expr **rexpr, bool muteError) {
  const std::string_view name = call->getName();
  const AnyCollection *args = call->getArguments();
  if (!args || args->empty()) return false;

  Serializer &serializer = *call->getSerializer();
  cast_op castOp;

  /*======================================================
   * $cast : TWO arguments
   *   arg[0] -> target type
   *   arg[1] -> source value
   *======================================================*/
  if (name == "$cast") {
    if (args->size() != 2) return false;

    const Expr *targetExpr = any_cast<Expr>((*args)[0]);
    if (!targetExpr) return false;

    const Typespec *targetTs = getTypespec(targetExpr);
    if (!targetTs) return false;

    Expr *srcExpr = nullptr;
    if (!reduceExpr(any_cast<Expr>((*args)[1]), pany, &srcExpr, muteError)) {
      return false;
    }

    Constant *carg = any_cast<Constant>(srcExpr);
    if (carg == nullptr) return false;

    value_t value = parse(carg);
    if (std::holds_alternative<std::monostate>(value)) return false;
    return buildCastConstant(value, targetTs, serializer, rexpr);
  }

  Expr *arg = nullptr;
  if (!reduceExpr(any_cast<Expr>((*args)[0]), pany, &arg, muteError)) {
    return false;
  }

  Constant *const carg = any_cast<Constant>(arg);
  if (carg == nullptr) return false;

  value_t value = parse(carg);
  if (std::holds_alternative<std::monostate>(value)) return false;

  if (name == "$itor") {
    double r = castOp.operator()<double>(value);
    *rexpr = createConstant<double>(r, serializer, UhdmType::RealTypespec, vpiRealConst, 64);
    return true;
  }

  if (name == "$rtoi") {
    double r = castOp.operator()<double>(value);
    int32_t i = static_cast<int32_t>(r);
    *rexpr = createConstant<int32_t>(i, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  if ((name == "$signed") || (name == "$unsigned")) {
    Constant *const carg = any_cast<Constant>((*args)[0]);
    if (carg == nullptr) return false;

    value_t value = parse(carg);
    if (std::holds_alternative<std::monostate>(value)) return false;

    Serializer &serializer = *call->getSerializer();
    const Typespec *const inTs = getTypespec(carg);
    if (inTs == nullptr) return false;

    UhdmType inType = inTs->getUhdmType();
    int32_t size = carg->getSize();

    cast_op castOp;

    switch (inType) {
      case UhdmType::IntTypespec: {
        int32_t i = castOp.operator()<int32_t>(value);
        if (name == "$unsigned") {
          *rexpr = createConstant<uint32_t>(static_cast<uint32_t>(i), serializer, inType, vpiIntConst, size);
        } else {
          *rexpr = createConstant<int32_t>(i, serializer, inType, vpiIntConst, size);
        }
        return true;
      }

      case UhdmType::LongIntTypespec: {
        int64_t i = castOp.operator()<int64_t>(value);
        if (name == "$unsigned") {
          *rexpr = createConstant<uint64_t>(static_cast<uint64_t>(i), serializer, inType, vpiIntConst, size);
        } else {
          *rexpr = createConstant<int64_t>(i, serializer, inType, vpiIntConst, size);
        }
        return true;
      }

      case UhdmType::ShortIntTypespec: {
        int16_t i = castOp.operator()<int16_t>(value);
        if (name == "$unsigned") {
          *rexpr = createConstant<uint16_t>(static_cast<uint16_t>(i), serializer, inType, vpiIntConst, size);
        } else {
          *rexpr = createConstant<int16_t>(i, serializer, inType, vpiIntConst, size);
        }
        return true;
      }

      case UhdmType::ByteTypespec: {
        int8_t i = castOp.operator()<int8_t>(value);
        if (name == "$unsigned") {
          *rexpr = createConstant<uint8_t>(static_cast<uint8_t>(i), serializer, inType, vpiIntConst, size);
        } else {
          *rexpr = createConstant<int8_t>(i, serializer, inType, vpiIntConst, size);
        }
        return true;
      }

      case UhdmType::LogicTypespec:
      case UhdmType::BitTypespec: {
        if (name == "$unsigned") {
          uint64_t u = castOp.operator()<uint64_t>(value);
          *rexpr = createConstant<uint64_t>(u, serializer, inType, vpiBinaryConst, size);
        } else {
          int64_t i = castOp.operator()<int64_t>(value);
          *rexpr = createConstant<int64_t>(i, serializer, inType, vpiBinaryConst, size);
        }
        return true;
      }

      default: return false;
    }
  }

  if (name == "$realtobits") {
    double r = castOp.operator()<double>(value);
    uint64_t bits;
    std::memcpy(&bits, &r, sizeof(bits));
    *rexpr = createConstant<uint64_t>(bits, serializer, UhdmType::LongIntTypespec, vpiIntConst, 64);
    return true;
  }

  if (name == "$bitstoreal") {
    uint64_t bits = castOp.operator()<uint64_t>(value);
    double r;
    std::memcpy(&r, &bits, sizeof(r));
    *rexpr = createConstant<double>(r, serializer, UhdmType::RealTypespec, vpiRealConst, 64);
    return true;
  }

  if (name == "$shortrealtobits") {
    float r = static_cast<float>(castOp.operator()<double>(value));
    uint32_t bits;
    std::memcpy(&bits, &r, sizeof(bits));
    *rexpr = createConstant<uint32_t>(bits, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  if (name == "$bitstoshortreal") {
    uint32_t bits = static_cast<uint32_t>(castOp.operator()<uint64_t>(value));
    float r;
    std::memcpy(&r, &bits, sizeof(r));
    *rexpr = createConstant<float>(r, serializer, UhdmType::ShortRealTypespec, vpiRealConst, 32);
    return true;
  }

  return false;
}

static void appendRange(std::string &out, const Range *r) {
  const auto *l = any_cast<Constant>(r->getLeftExpr());
  const auto *rgt = any_cast<Constant>(r->getRightExpr());

  out += " [";
  out += l ? std::string(l->getDecompile()) : "?";
  out += ":";
  out += rgt ? std::string(rgt->getDecompile()) : "?";
  out += "]";
}

bool ExprEval::reduceDataQuerySysFunc(const SysFuncCall *call, const Any *pexpr, Expr **rexpr, bool muteError) {
  const std::string_view name = call->getName();
  const AnyCollection *args = call->getArguments();
  if (!args || args->empty()) return false;

  // Reduce argument expression
  Expr *argExpr = nullptr;
  if (!reduceExpr(any_cast<Expr>((*args)[0]), pexpr, &argExpr, muteError)) return false;

  const Typespec *ts = getTypespec(argExpr);
  if (!ts) return false;

  Serializer &serializer = *call->getSerializer();

  if (name == "$bits") {
    uint64_t bits = 0;

    if (!getWordSize(argExpr, call, &bits)) return false;

    *rexpr = createConstant<uint32_t>(static_cast<uint32_t>(bits), serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  if (name == "$isunbounded") {
    auto isUnboundedArray = [&](const Typespec *ts) -> bool {
      const Typespec *cur = ts;
      while (cur) {
        if (auto *arr = any_cast<const ArrayTypespec *>(cur)) {
          int32_t at = arr->getArrayType();
          if (at == vpiDynamicArray || at == vpiQueueArray || at == vpiAssocArray) return true;

          if (auto *rt = arr->getElemTypespec())
            cur = rt->getActual();
          else
            break;
          continue;
        }
        break;
      }
      return false;
    };
    bool unbounded = isUnboundedArray(ts);
    *rexpr = createConstant<int32_t>(unbounded ? 1 : 0, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  if (name == "$typename") {
    std::string result;

    const Typespec *cur = ts;

    switch (cur->getUhdmType()) {
      case UhdmType::IntTypespec: result = "int"; break;
      case UhdmType::ByteTypespec: result = "byte"; break;
      case UhdmType::ShortIntTypespec: result = "shortint"; break;
      case UhdmType::LongIntTypespec: result = "longint"; break;
      case UhdmType::IntegerTypespec: result = "integer"; break;
      case UhdmType::LogicTypespec: result = "logic"; break;
      case UhdmType::BitTypespec: result = "bit"; break;
      case UhdmType::StringTypespec: result = "string"; break;
      default: return false;
    }

    if (auto *lt = any_cast<const LogicTypespec *>(cur)) {
      if (auto *ranges = lt->getRanges()) {
        for (const Range *r : *ranges) {
          appendRange(result, r);
        }
      }
    }

    while (auto *arr = any_cast<const ArrayTypespec *>(cur)) {
      if (auto *ranges = arr->getRanges()) {
        for (const Range *r : *ranges) {
          appendRange(result, r);
        }
      }

      const RefTypespec *rt = arr->getElemTypespec();
      if (!rt) break;
      cur = rt->getActual();
    }

    auto *c = serializer.make<Constant>();
    c->setValue(result);
    c->setDecompile(result);
    c->setConstType(vpiStringConst);
    c->setSize(static_cast<uint32_t>(result.size() * 8));

    auto *sts = serializer.make<StringTypespec>();
    auto *rt = serializer.make<RefTypespec>();
    rt->setActual(sts);
    c->setTypespec(rt);

    *rexpr = c;
    return true;
  }

  return false;
}

struct ExprEval::DimInfo final {
  int64_t left;
  int64_t right;
  bool isPacked;
};

bool ExprEval::collectDimensions(const Typespec *ts, const Any *pexpr, std::vector<DimInfo> &dims) {
  const Typespec *cur = ts;

  /*------------- Unpacked (outer → inner) -------------*/
  while (auto *arr = any_cast<const ArrayTypespec *>(cur)) {
    if (auto *ranges = arr->getRanges()) {
      for (auto *r : *ranges) {
        Expr *l = nullptr;
        Expr *h = nullptr;

        if (!reduceExpr(r->getLeftExpr(), pexpr, &l, true)) return false;
        if (!reduceExpr(r->getRightExpr(), pexpr, &h, true)) return false;

        auto *lc = any_cast<const Constant *>(l);
        auto *rc = any_cast<const Constant *>(h);
        if (!lc || !rc) return false;

        dims.push_back(
            {std::stoll(std::string(lc->getDecompile())), std::stoll(std::string(rc->getDecompile())), false});
      }
    }

    auto *rt = arr->getElemTypespec();
    if (!rt) break;
    cur = rt->getActual();
  }

  /*------------- Packed (inner → outer) -------------*/
  if (auto *logic = any_cast<const LogicTypespec *>(cur)) {
    if (auto *ranges = logic->getRanges()) {
      for (auto *r : *ranges) {
        Expr *l = nullptr;
        Expr *h = nullptr;
        if (!reduceExpr(r->getLeftExpr(), pexpr, &l, true)) return false;
        if (!reduceExpr(r->getRightExpr(), pexpr, &h, true)) return false;

        auto *lc = any_cast<const Constant *>(l);
        auto *rc = any_cast<const Constant *>(h);
        if (!lc || !rc) return false;

        dims.push_back(
            {std::stoll(std::string(lc->getDecompile())), std::stoll(std::string(rc->getDecompile())), true});
      }
    }
  }

  return true;
}

static int64_t rangeSize(int32_t l, int32_t r) {
  return (l >= r) ? (int64_t(l) - int64_t(r) + 1) : (int64_t(r) - int64_t(l) + 1);
}

bool ExprEval::reduceArrayQuerySysFunc(const SysFuncCall *call, const Any *pexpr, Expr **rexpr, bool muteError) {
  const std::string_view name = call->getName();
  const AnyCollection *args = call->getArguments();
  if (!args || args->empty()) return false;

  Expr *argExpr = nullptr;
  if (!reduceExpr(any_cast<Expr>((*args)[0]), pexpr, &argExpr, muteError)) return false;

  const Typespec *ts = getTypespec(argExpr);
  if (!ts) return false;

  Serializer &serializer = *call->getSerializer();

  if (name == "$dimensions") {
    std::vector<DimInfo> dims;
    if (!collectDimensions(ts, pexpr, dims)) return false;

    *rexpr = createConstant<uint32_t>(dims.size(), serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  if (name == "$unpacked_dimensions") {
    std::vector<DimInfo> dims;
    if (!collectDimensions(ts, pexpr, dims)) return false;

    uint32_t count = 0;
    for (auto &d : dims)
      if (!d.isPacked) count++;

    *rexpr = createConstant<uint32_t>(count, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  auto getDimIndex = [&](int64_t &idx, std::vector<DimInfo> &dims) -> bool {
    if (args->size() < 2) return false;

    Expr *dimExpr = nullptr;
    if (!reduceExpr(any_cast<Expr>((*args)[1]), pexpr, &dimExpr, muteError)) return false;

    auto *c = any_cast<const Constant *>(dimExpr);
    if (!c) return false;

    idx = std::stoll(std::string(c->getDecompile())) - 1;
    return idx >= 0 && idx < (int64_t)dims.size();
  };

  if (name == "$size") {
    std::vector<DimInfo> dims;
    if (!collectDimensions(ts, pexpr, dims)) return false;

    int64_t idx;
    if (!getDimIndex(idx, dims)) return false;

    int32_t sz = rangeSize(dims[idx].left, dims[idx].right);

    *rexpr = createConstant<int32_t>(sz, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  if (name == "$left") {
    std::vector<DimInfo> dims;
    if (!collectDimensions(ts, pexpr, dims)) return false;

    int64_t idx;
    if (!getDimIndex(idx, dims)) return false;

    *rexpr = createConstant<int32_t>(dims[idx].left, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  if (name == "$right") {
    std::vector<DimInfo> dims;
    if (!collectDimensions(ts, pexpr, dims)) return false;

    int64_t idx;
    if (!getDimIndex(idx, dims)) return false;

    *rexpr = createConstant<int32_t>(dims[idx].right, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  if (name == "$low") {
    std::vector<DimInfo> dims;
    if (!collectDimensions(ts, pexpr, dims)) return false;

    int64_t idx;
    if (!getDimIndex(idx, dims)) return false;

    *rexpr = createConstant<int32_t>(std::min(dims[idx].left, dims[idx].right), serializer, UhdmType::IntTypespec,
                                     vpiIntConst, 32);
    return true;
  }

  if (name == "$high") {
    std::vector<DimInfo> dims;
    if (!collectDimensions(ts, pexpr, dims)) return false;

    int64_t idx;
    if (!getDimIndex(idx, dims)) return false;

    *rexpr = createConstant<int32_t>(std::max(dims[idx].left, dims[idx].right), serializer, UhdmType::IntTypespec,
                                     vpiIntConst, 32);
    return true;
  }

  if (name == "$increment") {
    std::vector<DimInfo> dims;
    if (!collectDimensions(ts, pexpr, dims)) return false;

    int64_t idx;
    if (!getDimIndex(idx, dims)) return false;

    int32_t inc = (dims[idx].left <= dims[idx].right) ? 1 : -1;

    *rexpr = createConstant<int32_t>(inc, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  return false;
}
bool ExprEval::reduceBitVectorSysFunc(const SysFuncCall *call, const Any *pexpr, Expr **rexpr, bool muteError) {
  const std::string_view name = call->getName();
  const AnyCollection *args = call->getArguments();
  if (!args || args->empty()) return false;

  // Reduce argument
  Expr *argExpr = nullptr;
  if (!reduceExpr(any_cast<Expr>((*args)[0]), pexpr, &argExpr, muteError)) return false;

  auto *c = any_cast<const Constant *>(argExpr);
  if (!c) return false;

  const Typespec *ts = getTypespec(argExpr);
  if (!ts) return false;

  if (auto *rt = any_cast<const RefTypespec *>(ts)) ts = rt->getActual();

  Serializer &serializer = *call->getSerializer();

  uint64_t width = 0;
  if (!getWordSize(argExpr, pexpr, &width)) return false;

  std::string bits = std::string(c->getDecompile());
  if (bits.size() < width) bits = std::string(width - bits.size(), '0') + bits;

  if (name == "$countones") {
    uint32_t cnt = 0;
    for (char b : bits)
      if (b == '1') cnt++;

    *rexpr = createConstant<uint32_t>(cnt, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  // --------------------------------------------------
  // $countbits(expr, mask)
  // mask is string of chars: "01xz"
  // --------------------------------------------------
  if (name == "$countbits") {
    if (args->size() < 2) return false;

    Expr *maskExpr = nullptr;
    if (!reduceExpr(any_cast<Expr>((*args)[1]), pexpr, &maskExpr, muteError)) return false;

    auto *maskC = any_cast<const Constant *>(maskExpr);
    if (!maskC) return false;

    std::string mask = std::string(maskC->getDecompile());

    uint32_t cnt = 0;
    for (char b : bits)
      if (mask.find(b) != std::string::npos) cnt++;

    *rexpr = createConstant<uint32_t>(cnt, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  // --------------------------------------------------
  // $onehot
  // exactly one bit == 1, no x/z allowed
  // --------------------------------------------------
  if (name == "$onehot") {
    int32_t ones = 0;
    bool bad = false;

    for (char b : bits) {
      if (b == 'x' || b == 'z') {
        bad = true;
        break;
      }
      if (b == '1') ones++;
    }

    uint32_t ok = (!bad && ones == 1) ? 1 : 0;

    *rexpr = createConstant<uint32_t>(ok, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  // --------------------------------------------------
  // $onehot0
  // zero or one bits == 1, no x/z allowed
  // --------------------------------------------------
  if (name == "$onehot0") {
    int32_t ones = 0;
    bool bad = false;

    for (char b : bits) {
      if (b == 'x' || b == 'z') {
        bad = true;
        break;
      }
      if (b == '1') ones++;
    }

    uint32_t ok = (!bad && (ones == 0 || ones == 1)) ? 1 : 0;

    *rexpr = createConstant<uint32_t>(ok, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  // --------------------------------------------------
  // $isunknown
  // any bit is x or z
  // --------------------------------------------------
  if (name == "$isunknown") {
    bool hasXZ = false;
    for (char b : bits) {
      if (b == 'x' || b == 'z') {
        hasXZ = true;
        break;
      }
    }

    *rexpr = createConstant<uint32_t>(hasXZ ? 1 : 0, serializer, UhdmType::IntTypespec, vpiIntConst, 32);
    return true;
  }

  return false;
}

bool ExprEval::reduceSysFuncCall(const SysFuncCall *call, const Any *pany, Expr **rexpr, bool muteError) {
  const std::string_view name = call->getName();
  if (isConvSysFunc(name)) {
    return reduceConvSysFunc(call, pany, rexpr, muteError);
  } else if (isDataQuerySysFunc(name)) {
    return reduceDataQuerySysFunc(call, pany, rexpr, muteError);
  } else if (isArrayQuerySysFunc(name)) {
    return reduceArrayQuerySysFunc(call, pany, rexpr, muteError);
  } else if (isBitVectorSysFunc(name)) {
    return reduceBitVectorSysFunc(call, pany, rexpr, muteError);
  } else if (isMathSysFunc(name)) {
    return reduceMathSysFunc(call, pany, rexpr, muteError);
  }
  return *rexpr != nullptr;
}

bool ExprEval::reduceFuncCall(const FuncCall *call, const Any *pany, Expr **rexpr, bool muteError) {
  Serializer &serializer = *call->getSerializer();

  const std::string_view name = call->getName();
  std::vector<Any *> *const args = call->getArguments();
  const Function *actual_func = nullptr;
  if (const TaskFunc *const func = getTaskFunc(name, nullptr, pany)) {
    actual_func = any_cast<Function>(func);
  }
  if (actual_func == nullptr) {
    if (!muteError && !m_muteError) {
      const std::string errMsg(name);
      serializer.getErrorHandler()(ErrorType::UHDM_UNDEFINED_USER_FUNCTION, errMsg, call, nullptr);
    }
    return false;
  }
  bool invalidValue = false;
  if (Expr *const tmp = evalFunc(actual_func, args, invalidValue, nullptr, (Any *)pany, muteError)) {
    if (!invalidValue) *rexpr = tmp;
  }

  return (*rexpr != nullptr);
}

bool ExprEval::reduceRefObj(const RefObj *ro, const Any *pany, Expr **rexpr, bool muteError) {
  const std::string_view name = ro->getName();
  if (Any *const any = getValue(name, nullptr, pany, muteError)) {
    *rexpr = any_cast<Expr>(any);
  }

  return (*rexpr != nullptr);
}

bool ExprEval::reduceHierPath(const HierPath *hp, const Any *pany, bool returnTypespec, Any **rany, bool muteError) {
  bool invalidValue = false;
  Serializer &s = *hp->getSerializer();
  std::string baseObject;
  if (!hp->getPathElems()->empty()) {
    Any *firstElem = hp->getPathElems()->at(0);
    baseObject = firstElem->getName();
  }
  const Any *object = getObject(baseObject, nullptr, pany, muteError);
  if (object) {
    if (const ParamAssign *const passign = any_cast<ParamAssign>(object)) {
      object = passign->getRhs();
    }
  }
  if (object == nullptr) {
    object = getValue(baseObject, nullptr, pany, muteError);
  }
  if (object == nullptr) return false;

  // Substitution
  if (const ParamAssign *const pass = any_cast<ParamAssign>(object)) {
    const Expr *rhs = pass->getRhs<Expr>();
    Expr *r = nullptr;
    if ((invalidValue = !reduceExpr(rhs, pany, &r, muteError))) {
      object = r;
    }
  } else if (const BitSelect *const bts = any_cast<BitSelect>(object)) {
    Expr *r = nullptr;
    if ((invalidValue = !reduceExpr(bts, pany, &r, muteError))) {
      object = r;
    }
  } else if (const RefObj *const ref = any_cast<RefObj>(object)) {
    Expr *r = nullptr;
    if ((invalidValue = !reduceExpr(ref, pany, &r, muteError))) {
      object = r;
    }
  } else if (const Constant *cons = any_cast<Constant>(object)) {
    Elaborator elaborator(&s);
    object = elaborator.clone<>(cons, nullptr);
    cons = any_cast<Constant>(object);
    if (cons->getTypespec() == nullptr) {
      RefTypespec *rt = elaborator.clone<>(hp->getTypespec(), const_cast<Any *>(object));
      const_cast<Constant *>(cons)->setTypespec(rt);
    }
  } else if (const Operation *oper = any_cast<Operation>(object)) {
    if (returnTypespec) {
      if (const RefTypespec *const rt = oper->getTypespec()) {
        object = rt->getActual();
      }
    }
  }

  std::vector<std::string> the_path;
  for (auto elem : *hp->getPathElems()) {
    std::string_view elemName = elem->getName();
    elemName = rtrim_until(elemName, '[');
    the_path.emplace_back(elemName);
    if (elem->getUhdmType() == UhdmType::BitSelect) {
      BitSelect *select = (BitSelect *)elem;
      Expr *bexpr = nullptr;
      invalidValue = !reduceExpr(select->getIndex(), pany, &bexpr, muteError);
      uint64_t baseIndex = 0;
      if (getUInt64(bexpr, &baseIndex)) {
        the_path.emplace_back("[" + std::to_string(baseIndex) + "]");
      }
    }
  }

  if (const Any *const any =
          hierarchicalSelector(the_path, 0, object, invalidValue, nullptr, pany, returnTypespec, muteError)) {
    *rany = const_cast<Any *>(any);
  }

  return (*rany != nullptr);
}

bool ExprEval::reduceBitSelect(const BitSelect *bs, const Any *pany, Expr **rexpr, bool muteError) {
  bool succeeded = false;
  Expr *result = nullptr;
  Serializer &serializer = *bs->getSerializer();

  const std::string_view name = bs->getName();
  const Expr *const index = bs->getIndex();

  Expr *rindex = nullptr;
  if (!reduceExpr(index, pany, &rindex, muteError)) return false;

  uint64_t index_val = 0;
  if (!getUInt64(rindex, &index_val)) return false;

  const Any *object = getObject(name, nullptr, pany, muteError);
  if (object != nullptr) {
    if (const ParamAssign *const passign = any_cast<ParamAssign>(object)) {
      object = passign->getRhs();
    }
  }
  if (object == nullptr) {
    object = getValue(name, nullptr, pany, muteError);
  }
  if (object && (object != result)) {
    Expr *robject = nullptr;
    if (reduceExpr(any_cast<Expr>(object), pany, &robject, muteError)) {
      object = robject;
    }
    UhdmType otype = object->getUhdmType();
    if (otype == UhdmType::Variable) {
      // PackedArrayVar *array = (PackedArrayVar *)object;
      // AnyCollection *elems = array->getElements();
      // if (elems && index_val < elems->size()) {
      //   Any *elem = elems->at(index_val);
      //   if (elem->getUhdmType() == UhdmType::EnumVar ||
      //       elem->getUhdmType() == UhdmType::StructVar ||
      //       elem->getUhdmType() == UhdmType::UnionVar ||
      //       elem->getUhdmType() == UhdmType::LogicVar) {
      //   } else {
      //     result = elems->at(index_val);
      //   }
      // }
    } else if (otype == UhdmType::ArrayExpr) {
      ArrayExpr *array = (ArrayExpr *)object;
      ExprCollection *elems = array->getExprs();
      if (index_val < elems->size()) {
        result = elems->at(index_val);
      }
    } else if (otype == UhdmType::Operation) {
      Operation *op = (Operation *)object;
      int32_t opType = op->getOpType();
      if (opType == vpiAssignmentPatternOp) {
        AnyCollection *ops = op->getOperands();
        if (ops && (index_val < ops->size())) {
          result = any_cast<Expr>(ops->at(index_val));
          if ((result != nullptr) && (result->getUhdmType() == UhdmType::Operation)) {
            if (const RefTypespec *oprt = op->getTypespec()) {
              if (const ArrayTypespec *atps = oprt->getActual<ArrayTypespec>()) {
                if (const RefTypespec *ert = atps->getElemTypespec()) {
                  if (const Typespec *ertts = ert->getActual()) {
                    Elaborator elaborator(&serializer, false, muteError);
                    RefTypespec *celrt = elaborator.clone<>(ert, const_cast<Expr *>(result));
                    celrt->setActual(const_cast<Typespec *>(ertts));
                    ((Operation *)result)->setTypespec(celrt);
                  }
                }
              }
            }
          }
        } else if (ops) {
          bool defaultTaggedPattern = false;
          for (auto op : *ops) {
            if (op->getUhdmType() == UhdmType::TaggedPattern) {
              TaggedPattern *tp = (TaggedPattern *)op;
              if (const RefTypespec *rt = tp->getTypespec()) {
                if (const Typespec *tps = rt->getActual()) {
                  if (tps->getName() == "default") {
                    defaultTaggedPattern = true;
                    break;
                  }
                }
              }
            }
          }
          if (!defaultTaggedPattern) succeeded = false;
        } else {
          succeeded = false;
        }
      } else if (opType == vpiConcatOp) {
        AnyCollection *ops = op->getOperands();
        if (ops && (index_val < ops->size())) {
          result = any_cast<Expr>(ops->at(index_val));
        } else {
          succeeded = false;
        }
      } else if (opType == vpiConditionOp) {
        Expr *exp = nullptr;
        if (reduceExpr(op, pany, &exp, muteError)) {
          return false;
        }
        UhdmType otype = exp->getUhdmType();
        if (otype == UhdmType::Operation) {
          Operation *op = (Operation *)exp;
          int32_t opType = op->getOpType();
          if (opType == vpiAssignmentPatternOp) {
            AnyCollection *ops = op->getOperands();
            if (ops && (index_val < ops->size())) {
              object = ops->at(index_val);
            } else {
              succeeded = false;
            }
          } else if (opType == vpiConcatOp) {
            AnyCollection *ops = op->getOperands();
            if (ops && (index_val < ops->size())) {
              object = ops->at(index_val);
            } else {
              succeeded = false;
            }
          }
        }
        if (object != nullptr) result = const_cast<Expr *>(any_cast<Expr>(object));
      } else if (opType == vpiMultiConcatOp) {
        Expr *c = nullptr;
        if (reduceOperation(op, pany, &c, muteError)) {
          if ((c != nullptr) && (c->getUhdmType() == UhdmType::Constant))
            succeeded = reduceConstant((Constant *)c, static_cast<uint32_t>(index_val), pany, &result, muteError);
        }
      }
    } else if (otype == UhdmType::Constant) {
      succeeded = reduceConstant((Constant *)object, static_cast<uint32_t>(index_val), pany, &result, muteError);
    }
  }

  succeeded = result != nullptr;
  *rexpr = const_cast<Expr *>(result);
  return succeeded;
}

bool ExprEval::reduceConstant(const Constant *constant, uint32_t index, const Any *pany, Expr **rexpr, bool muteError) {
  Serializer &serializer = *constant->getSerializer();

  const Typespec *const ts = getTypespec(constant);
  const RangeCollection *const rc = getRanges(ts);
  if ((rc == nullptr) || rc->empty()) return false;

  const Range *const r = rc->back();

  Expr *llimit = nullptr;
  if (!reduceExpr(r->getLeftExpr(), pany, &llimit, muteError)) return false;

  Expr *rlimit = nullptr;
  if (!reduceExpr(r->getRightExpr(), pany, &rlimit, muteError)) return false;

  uint64_t lr = 0;
  uint64_t rr = 0;
  if (!getUInt64(llimit, &lr) || !getUInt64(rlimit, &rr)) return false;

  std::string binary;
  if (!formatBinary(constant, &binary)) return false;

  uint64_t wordSize = 0;
  if (!getWordSize(constant, pany, &wordSize)) return false;

  Constant *const result = serializer.make<Constant>();
  result->setSize(static_cast<int32_t>(wordSize));
  result->setFile(constant->getFile());
  result->setStartLine(constant->getStartLine());
  result->setStartColumn(constant->getStartColumn());
  result->setEndLine(constant->getEndLine());
  result->setEndColumn(constant->getStartColumn() + 1);

  if (index < binary.size()) {
    // TODO: If Range does not start at 0
    if (lr >= rr) {
      index = static_cast<uint32_t>(binary.size() - ((index + 1) * wordSize));
    }
    std::string v;
    for (uint32_t i = 0; i < wordSize; i++) {
      if ((index + i) < binary.size()) {
        char bitv = binary[index + i];
        v += std::to_string(bitv - '0');
      }
    }
    if (v.size() > UHDM_MAX_BIT_WIDTH) {
      v = "0";
    }
    result->setValue(v);
    result->setDecompile(std::to_string(wordSize) + "'b" + v);
    result->setConstType(vpiBinaryConst);
  } else {
    result->setValue("0");
    result->setDecompile("1'b0");
    result->setConstType(vpiBinaryConst);
  }

  *rexpr = result;
  return (result != nullptr);
}

bool ExprEval::reducePartSelect(const PartSelect *ps, const Any *pany, Expr **rexpr, bool muteError) {
  const Expr *result = nullptr;
  Serializer &serializer = *ps->getSerializer();

  std::string_view name = ps->getName();
  if (name.empty()) name = ps->getDefName();
  const Any *object = getObject(name, nullptr, pany, muteError);
  if (object) {
    if (const ParamAssign *const passign = any_cast<ParamAssign>(object)) {
      object = passign->getRhs();
    }
  }
  if (object == nullptr) {
    object = getValue(name, nullptr, pany, muteError);
  }
  if (object && (object->getUhdmType() == UhdmType::Constant)) {
    Constant *co = (Constant *)object;

    Expr *lexpr = nullptr;
    if (!reduceExpr(ps->getLeftExpr(), pany, &lexpr, muteError)) return false;

    std::string binary;
    if (!formatBinary(co, &binary)) return false;

    int64_t lvalue = 0;
    if (!getInt64(lexpr, &lvalue)) return false;

    Expr *rexpr = nullptr;
    if (!reduceExpr(ps->getRightExpr(), pany, &rexpr, muteError)) return false;

    int64_t rvalue = 0;
    if (!getInt64(rexpr, &rvalue)) return false;

    std::reverse(binary.begin(), binary.end());
    std::string sub;
    if ((rvalue > (int64_t)binary.size()) || (lvalue > (int64_t)binary.size())) {
      sub = "0";
    } else {
      sub = (lvalue > rvalue) ? binary.substr(rvalue, lvalue - rvalue + 1) : binary.substr(lvalue, rvalue - lvalue + 1);
    }
    std::reverse(sub.begin(), sub.end());

    Constant *const c = serializer.make<Constant>();
    c->setValue(sub);
    c->setDecompile(sub);
    c->setSize(static_cast<int32_t>(sub.size()));
    c->setConstType(vpiBinaryConst);
    result = c;
  }

  *rexpr = const_cast<Expr *>(result);
  return (*rexpr != nullptr);
}

bool ExprEval::reduceIndexedPartSelect(const IndexedPartSelect *ips, const Any *pany, Expr **rexpr, bool muteError) {
  std::string_view name = ips->getName();
  if (name.empty()) name = ips->getDefName();

  const Any *object = getObject(name, nullptr, pany, muteError);
  if (object) {
    if (const ParamAssign *const passign = any_cast<ParamAssign>(object)) {
      object = passign->getRhs();
    }
  }
  if (object == nullptr) {
    object = getValue(name, nullptr, pany, muteError);
  }

  if (object == nullptr) return false;
  if (object->getUhdmType() != UhdmType::Constant) return false;

  Expr *bexpr = nullptr;
  if (!reduceExpr(ips->getBaseExpr(), pany, &bexpr, muteError)) return false;

  Expr *oexpr = nullptr;
  if (!reduceExpr(ips->getWidthExpr(), pany, &oexpr, muteError)) return false;

  int64_t base = 0;
  if (!getInt64(bexpr, &base)) return false;

  int64_t offset = 0;
  if (!getInt64(oexpr, &offset)) return false;

  std::string binary;
  if (!formatBinary(static_cast<const Constant *>(object), &binary)) return false;

  std::string sub;
  const uint32_t N = static_cast<uint32_t>(binary.size());
  if (ips->getIndexedPartSelectType() == vpiPosIndexed) {
    uint32_t start = N - base - offset;
    if (start > N) start = 0;
    if (start + offset > N) offset = N - start;
    sub = binary.substr(start, offset);
  } else {
    int32_t start = static_cast<int32_t>(N - base - 1);
    if (start < 0) start = 0;
    if ((uint32_t)start + offset > N) offset = N - start;
    sub = binary.substr(start, offset);
  }

  Serializer &serializer = *ips->getSerializer();
  Constant *const result = serializer.make<Constant>();
  result->setValue(sub);
  result->setDecompile(sub);
  result->setSize(static_cast<int32_t>(sub.size()));
  result->setConstType(vpiBinaryConst);

  *rexpr = result;
  return (*rexpr != nullptr);
}

bool ExprEval::reduceVarSelect(const VarSelect *vs, const Any *pany, Expr **rexpr, bool muteError) {
  const std::string_view name = vs->getName();
  const Any *object = getObject(name, nullptr, pany, muteError);
  if (object) {
    if (const ParamAssign *const passign = any_cast<ParamAssign>(object)) {
      object = passign->getRhs();
    }
  }
  if (object == nullptr) {
    object = getValue(name, nullptr, pany, muteError);
  }
  if (object == nullptr) return false;

  for (auto index : *vs->getIndexes()) {
    Expr *rindex = nullptr;
    if (!reduceExpr((Expr *)index, pany, &rindex, muteError)) {
      return false;
    }

    uint64_t index_val = 0;
    if (!getUInt64(rindex, &index_val)) return false;

    if (const Operation *const operation = any_cast<Operation>(object)) {
      int32_t opType = operation->getOpType();
      if (opType == vpiAssignmentPatternOp) {
        AnyCollection *ops = operation->getOperands();
        if (ops && (index_val < ops->size())) {
          object = ops->at(index_val);
        } else {
          object = nullptr;
        }
      } else if (opType == vpiConcatOp) {
        AnyCollection *ops = operation->getOperands();
        if (ops && (index_val < ops->size())) {
          object = ops->at(index_val);
        } else {
          object = nullptr;
        }
      } else if (opType == vpiConditionOp) {
        Expr *exp = nullptr;
        if (reduceExpr(any_cast<Expr>(object), pany, &exp, muteError)) {
          return false;
        }
        UhdmType otype = exp->getUhdmType();
        if (otype == UhdmType::Operation) {
          Operation *op = (Operation *)exp;
          int32_t opType = operation->getOpType();
          if (opType == vpiAssignmentPatternOp) {
            AnyCollection *ops = op->getOperands();
            if (ops && (index_val < ops->size())) {
              object = ops->at(index_val);
            } else {
              object = nullptr;
            }
          } else if (opType == vpiConcatOp) {
            AnyCollection *ops = operation->getOperands();
            if (ops && (index_val < ops->size())) {
              object = ops->at(index_val);
            } else {
              object = nullptr;
            }
          }
        }
      } else {
        object = nullptr;
      }
    } else {
      object = nullptr;
    }
    if (object == nullptr) return false;
  }
  *rexpr = any_cast<Expr>(const_cast<Any *>(object));
  return (*rexpr != nullptr);
}

bool ExprEval::reduceExpr(const Expr *expr, const Any *pany, Expr **rexpr, bool muteError) {
  if ((expr == nullptr) || (pany == nullptr)) return false;

  bool succeeded = false;
  Expr *result = nullptr;
  switch (expr->getUhdmType()) {
    case UhdmType::Constant: {
      result = const_cast<Expr *>(expr);
      succeeded = true;
    } break;

    case UhdmType::Operation: {
      succeeded = reduceOperation(static_cast<const Operation *>(expr), pany, &result, muteError);
    } break;

    case UhdmType::SysFuncCall: {
      succeeded = reduceSysFuncCall(static_cast<const SysFuncCall *>(expr), pany, &result, muteError);
    } break;

    case UhdmType::FuncCall: {
      succeeded = reduceFuncCall(static_cast<const FuncCall *>(expr), pany, &result, muteError);
    } break;

    case UhdmType::RefObj: {
      succeeded = reduceRefObj(static_cast<const RefObj *>(expr), pany, &result, muteError);
    } break;

    case UhdmType::HierPath: {
      Any *rany = nullptr;
      if ((succeeded = reduceHierPath(static_cast<const HierPath *>(expr), pany, false, &rany, muteError))) {
        result = any_cast<Expr>(rany);
      }
    } break;

    case UhdmType::BitSelect: {
      succeeded = reduceBitSelect(static_cast<const BitSelect *>(expr), pany, &result, muteError);
    } break;

    case UhdmType::PartSelect: {
      succeeded = reducePartSelect(static_cast<const PartSelect *>(expr), pany, &result, muteError);
    } break;

    case UhdmType::IndexedPartSelect: {
      succeeded = reduceIndexedPartSelect(static_cast<const IndexedPartSelect *>(expr), pany, &result, muteError);
    } break;

    case UhdmType::VarSelect: {
      succeeded = reduceVarSelect(static_cast<const VarSelect *>(expr), pany, &result, muteError);
    } break;

    default: {
      succeeded = false;
    } break;
  }

  if (succeeded && (result != nullptr) && (result->getUhdmType() == UhdmType::RefObj)) {
    Expr *rexpr2 = nullptr;
    if (reduceExpr(static_cast<RefObj *>(result), pany, &rexpr2, muteError)) {
      result = rexpr2;
    } else {
      result = nullptr;
      succeeded = false;
    }
  }

  *rexpr = result;
  return succeeded;
}

bool ExprEval::getWordSize(const Expr *exp, const Any *pany, uint64_t *wordSize) const {
  if (exp == nullptr) return false;

  const Typespec *typespec = getTypespec(exp);
  if (typespec == nullptr) return false;

  if (typespec->getUhdmType() == UhdmType::ArrayTypespec) {
    typespec = getElemTypespec(static_cast<const ArrayTypespec *>(typespec));
  }

  switch (typespec->getUhdmType()) {
    case UhdmType::LongIntTypespec: {
      *wordSize = 64;
    } break;

    case UhdmType::ShortIntTypespec: {
      *wordSize = 16;
    } break;

    case UhdmType::IntTypespec: {
      *wordSize = 32;
    } break;

    case UhdmType::ByteTypespec: {
      *wordSize = 8;
    } break;

    case UhdmType::IntegerTypespec: {
      *wordSize = 32;
    } break;

    case UhdmType::LogicTypespec: {
      const LogicTypespec *const lt = static_cast<const LogicTypespec *>(typespec);
      return getBitCount(lt, pany, true, wordSize);
    } break;

    case UhdmType::BitTypespec: {
      const BitTypespec *const bt = static_cast<const BitTypespec *>(typespec);
      *wordSize = 1;

      if (const RangeCollection *const ranges = bt->getRanges()) {
        if (ranges->size() > 1) {
          const Range *const r = ranges->back();

          Expr *le = nullptr;
          Expr *re = nullptr;
          ExprEval ee(m_provider);
          if (ee.reduceExpr(r->getLeftExpr(), r, &le, false) && ee.reduceExpr(r->getRightExpr(), r, &re, false)) {
            uint64_t lr = 0;
            uint64_t rr = 0;
            if (getUInt64(le, &lr) && getUInt64(re, &rr)) {
              *wordSize = ((lr > rr) ? (lr - rr) : (rr - lr)) + 1;
            }
          }
        }
      }
    } break;

    default: {
      *wordSize = 1;
    } break;
  }

  if (*wordSize == 0) *wordSize = 1;
  return true;
}

bool ExprEval::getBitCount(const Any *any, const Any *pany, bool allRanges, uint64_t *bits,
                           bool muteError /* = false */) const {
  if (any == nullptr) return 0;

  bool result = true;
  RangeCollection *ranges = nullptr;
  switch (any->getUhdmType()) {
    case UhdmType::ArrayTypespec: {
      const ArrayTypespec *const at = static_cast<const ArrayTypespec *>(any);
      ranges = at->getRanges();
      if (!allRanges) {
        *bits = 1;
      } else if (const RefTypespec *rt = at->getElemTypespec()) {
        result = getBitCount(rt->getActual(), pany, allRanges, bits, muteError);
      } else {
        result = false;
      }
    } break;

    case UhdmType::BitTypespec: {
      const BitTypespec *const bt = static_cast<const BitTypespec *>(any);
      ranges = bt->getRanges();
      *bits = 1;
    } break;

    case UhdmType::ByteTypespec: {
      *bits = 8;
    } break;

    case UhdmType::ShortIntTypespec: {
      *bits = 16;
    } break;

    case UhdmType::IntTypespec:
    case UhdmType::IntegerTypespec:
    case UhdmType::ShortRealTypespec: {
      *bits = 32;
    } break;

    case UhdmType::LongIntTypespec:
    case UhdmType::RealTypespec: {
      *bits = 64;
    } break;

    case UhdmType::LogicTypespec: {
      const LogicTypespec *const lt = static_cast<const LogicTypespec *>(any);
      ranges = lt->getRanges();
      *bits = 1;
    } break;

    case UhdmType::StringTypespec: {
      *bits = 0;
      result = false;
    } break;

    case UhdmType::UnsupportedTypespec: {
      const UnsupportedTypespec *const ut = static_cast<const UnsupportedTypespec *>(any);
      ranges = ut->getRanges();
      *bits = 1;
      result = false;
    } break;

    case UhdmType::Net: {
      *bits = 1;
      if (const Typespec *const t = uhdm::getTypespec(any)) {
        result = getBitCount(t, pany, allRanges, bits, muteError);
      } else {
        result = false;
      }
    } break;

    case UhdmType::Variable: {
      const Variable *const v = static_cast<const Variable *>(any);
      result = getBitCount(v->getTypespec(), pany, allRanges, bits, muteError);
    } break;

    case UhdmType::EnumTypespec: {
      const EnumTypespec *const et = static_cast<const EnumTypespec *>(any);
      result = getBitCount(et->getBaseTypespec(), pany, allRanges, bits, muteError);
    } break;

    case UhdmType::StructTypespec: {
      const StructTypespec *const st = static_cast<const StructTypespec *>(any);
      *bits = 0;
      if (const TypespecMemberCollection *members = st->getMembers()) {
        for (const TypespecMember *member : *members) {
          uint64_t mbits = 0;
          if (getBitCount(member->getTypespec(), pany, allRanges, &mbits, muteError)) {
            *bits += mbits;
          } else {
            result = false;
          }
        }
      } else {
        result = false;
      }
    } break;

    case UhdmType::UnionTypespec: {
      const UnionTypespec *const ut = static_cast<const UnionTypespec *>(any);
      *bits = 0;
      if (const TypespecMemberCollection *members = ut->getMembers()) {
        for (const TypespecMember *member : *members) {
          uint64_t mbits = 0;
          if (getBitCount(member->getTypespec(), pany, allRanges, &mbits, muteError)) {
            *bits = std::max(*bits, mbits);
          } else {
            result = false;
          }
        }
      } else {
        result = false;
      }
    } break;

    case UhdmType::Constant: {
      const Constant *const c = static_cast<const Constant *>(any);
      *bits = c->getSize();
    } break;

    case UhdmType::EnumConst: {
      const EnumConst *const ec = static_cast<const EnumConst *>(any);
      if (const Constant *const c = ec->getValue()) {
        return getBitCount(c, ec, allRanges, bits, muteError);
      }
    } break;

    case UhdmType::HierPath: {
      const HierPath *const hp = static_cast<const HierPath *>(any);
      ExprEval ee(m_provider);
      Any *rany = nullptr;
      result = ee.reduceHierPath(hp, pany, true, &rany, true) && getBitCount(rany, pany, allRanges, bits, muteError);
    } break;

    case UhdmType::RefObj: {
      const RefObj *const ro = static_cast<const RefObj *>(any);
      result = getBitCount(ro->getActual(), pany, allRanges, bits, muteError);
    } break;

    case UhdmType::RefTypespec: {
      const RefTypespec *const rt = static_cast<const RefTypespec *>(any);
      result = getBitCount(rt->getActual(), pany, allRanges, bits, muteError);
    } break;

    case UhdmType::Operation: {
      const Operation *const o = static_cast<const Operation *>(any);
      if (o->getOpType() == vpiConcatOp) {
        if (const AnyCollection *const operands = o->getOperands()) {
          for (const Any *operand : *operands) {
            uint64_t opbits = 0;
            if (getBitCount(operand, pany, allRanges, &opbits, muteError)) {
              bits += opbits;
            } else {
              result = false;
              break;
            }
          }
        } else {
          result = false;
        }
      } else {
        result = false;
      }
    } break;

    case UhdmType::TypespecMember: {
      const TypespecMember *const tm = static_cast<const TypespecMember *>(any);
      result = getBitCount(tm->getTypespec(), pany, allRanges, bits, muteError);
    } break;

    case UhdmType::IODecl: {
      const IODecl *const iod = static_cast<const IODecl *>(any);
      result = getBitCount(iod->getTypespec(), pany, allRanges, bits, muteError);
    } break;

    case UhdmType::BitSelect: {
      *bits = 1;
    } break;

    case UhdmType::PartSelect: {
      const PartSelect *const ps = static_cast<const PartSelect *>(any);
      const Expr *const lexpr = ps->getLeftExpr();
      const Expr *const rexpr = ps->getRightExpr();

      Expr *rlexpr = nullptr;
      Expr *rrexpr = nullptr;
      ExprEval ee(m_provider);
      if (ee.reduceExpr(lexpr, pany, &rlexpr, muteError) && ee.reduceExpr(rexpr, pany, &rrexpr, muteError)) {
        int64_t lv = 0;
        int64_t rv = 0;
        bool blv = getInt64(rlexpr, &lv);
        bool brv = getInt64(rrexpr, &rv);

        uint64_t ulv = 0;
        uint64_t urv = 0;
        if (!blv && (blv = getUInt64(rlexpr, &ulv))) lv = static_cast<int64_t>(ulv);
        if (!brv && (brv = getUInt64(rrexpr, &urv))) rv = static_cast<int64_t>(urv);

        if (blv && brv) {
          *bits = ((lv > rv) ? (lv - rv) : (rv - lv)) + 1;
        } else {
          result = false;
        }
      } else {
        result = false;
      }
    } break;

    default: {
      result = false;
    } break;
  }

  if ((ranges != nullptr) && !ranges->empty()) {
    if (allRanges) {
      for (const Range *r : *ranges) {
        const Expr *const lexpr = r->getLeftExpr();
        const Expr *const rexpr = r->getRightExpr();

        Expr *rlexpr = nullptr;
        Expr *rrexpr = nullptr;
        ExprEval ee(m_provider);
        if (ee.reduceExpr(lexpr, pany, &rlexpr, muteError) && ee.reduceExpr(rexpr, pany, &rrexpr, muteError)) {
          int64_t lv = 0;
          int64_t rv = 0;
          bool blv = getInt64(rlexpr, &lv);
          bool brv = getInt64(rrexpr, &rv);

          uint64_t ulv = 0;
          uint64_t urv = 0;
          if (!blv && (blv = getUInt64(rlexpr, &ulv))) lv = static_cast<int64_t>(ulv);
          if (!brv && (brv = getUInt64(rrexpr, &urv))) rv = static_cast<int64_t>(urv);

          if (blv && brv) {
            *bits *= (((lv > rv) ? (lv - rv) : (rv - lv)) + 1);
          } else {
            result = false;
            break;
          }
        } else {
          result = false;
          break;
        }
      }
    } else {
      const Range *const r = ranges->back();
      const Expr *const lexpr = r->getLeftExpr();
      const Expr *const rexpr = r->getRightExpr();

      Expr *rlexpr = nullptr;
      Expr *rrexpr = nullptr;
      ExprEval ee(m_provider);
      if (ee.reduceExpr(lexpr, pany, &rlexpr, muteError) && ee.reduceExpr(rexpr, pany, &rrexpr, muteError)) {
        int64_t lv = 0;
        int64_t rv = 0;
        bool blv = getInt64(rlexpr, &lv);
        bool brv = getInt64(rrexpr, &rv);

        uint64_t ulv = 0;
        uint64_t urv = 0;
        if (!blv && (blv = getUInt64(rlexpr, &ulv))) lv = static_cast<int64_t>(ulv);
        if (!brv && (brv = getUInt64(rrexpr, &urv))) rv = static_cast<int64_t>(urv);

        if (blv && brv) {
          *bits *= (((lv > rv) ? (lv - rv) : (rv - lv)) + 1);
        } else {
          result = false;
        }
      } else {
        result = false;
      }
    }
  }
  return result;
}

const TaskFunc *ExprEval::getTaskFunc(std::string_view name, const Any *pany, const Any *inst) {
  if (m_provider != nullptr) {
    if (const TaskFunc *const result = m_provider->getTaskFunc(name, inst, pany)) {
      return result;
    }
  }
  if (inst == nullptr) {
    return nullptr;
  }
  const Any *root = inst;
  const Any *tmp = inst;
  while (tmp) {
    root = tmp;
    tmp = tmp->getParent();
  }
  const Design *des = any_cast<Design>(root);
  if (des) m_design = des;
  std::string_view the_name = name;
  const Any *the_instance = inst;
  if (m_design && (name.find("::") != std::string::npos)) {
    std::vector<std::string_view> res = tokenize(name, "::");
    if (res.size() > 1) {
      const std::string_view packName = res[0];
      const std::string_view varName = res[1];
      the_name = varName;
      Package *pack = nullptr;
      if (m_design->getTopPackages()) {
        for (auto p : *m_design->getTopPackages()) {
          if (p->getName() == packName) {
            pack = p;
            break;
          }
        }
      }
      the_instance = pack;
    }
  }
  while (the_instance) {
    TaskFuncCollection *task_funcs = nullptr;
    if (the_instance->getUhdmType() == UhdmType::GenScopeArray) {
    } else if (the_instance->getUhdmType() == UhdmType::Design) {
      task_funcs = ((Design *)the_instance)->getTaskFuncs();
    } else if (const Instance *inst = any_cast<Instance>(the_instance)) {
      task_funcs = inst->getTaskFuncs();
    }

    if (task_funcs) {
      for (TaskFunc *tf : *task_funcs) {
        if (tf->getName() == the_name) {
          return tf;
        }
      }
    }

    the_instance = the_instance->getParent();
  }

  return nullptr;
}

const Any *ExprEval::getObject(std::string_view name, const Any *inst, const Any *pany, bool muteError) {
  const Any *result = nullptr;
  const Any *scope = pany;
  while (scope) {
    if (const Scope *spe = any_cast<Scope>(scope)) {
      if (spe->getVariables()) {
        for (auto o : *spe->getVariables()) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (result) break;
    if (const TaskFunc *s = any_cast<TaskFunc>(scope)) {
      if (s->getIODecls()) {
        for (auto o : *s->getIODecls()) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && s->getParamAssigns()) {
        for (auto o : *s->getParamAssigns()) {
          const std::string_view pname = o->getLhs()->getName();
          if (pname == name) {
            result = o;
            break;
          }
        }
      }
    }
    if (result) break;
    if (scope->getUhdmType() == UhdmType::ForeachStmt) {
      ForeachStmt *for_stmt = (ForeachStmt *)scope;
      if (AnyCollection *loopvars = for_stmt->getLoopVars()) {
        for (auto var : *loopvars) {
          if (var->getName() == name) {
            result = var;
            break;
          }
        }
      }
    }
    if (scope->getUhdmType() == UhdmType::ClassDefn) {
      const ClassDefn *defn = (ClassDefn *)scope;
      while (defn) {
        if (defn->getVariables()) {
          for (Variable *member : *defn->getVariables()) {
            if (member->getName() == name) {
              result = member;
              break;
            }
          }
        }
        if (result) break;

        const ClassDefn *base_defn = nullptr;
        if (const Extends *ext = defn->getExtends()) {
          if (const RefTypespec *rt = ext->getClassTypespec()) {
            if (const ClassTypespec *tp = rt->getActual<ClassTypespec>()) {
              base_defn = tp->getClassDefn();
            }
          }
        }
        defn = base_defn;
      }
    }
    if (result) break;
    scope = scope->getParent();
  }
  if (result == nullptr) {
    while (inst) {
      ParamAssignCollection *ParamAssigns = nullptr;
      VariableCollection *Variables = nullptr;
      NetCollection *nets = nullptr;
      TypespecCollection *Typespecs = nullptr;
      ScopeCollection *scopes = nullptr;
      if (inst->getUhdmType() == UhdmType::GenScopeArray) {
      } else if (inst->getUhdmType() == UhdmType::Design) {
        ParamAssigns = ((Design *)inst)->getParamAssigns();
        Typespecs = ((Design *)inst)->getTypespecs();
      } else if (const Scope *spe = any_cast<Scope>(inst)) {
        ParamAssigns = spe->getParamAssigns();
        Variables = spe->getVariables();
        Typespecs = spe->getTypespecs();
        scopes = spe->getInternalScopes();
        if (const Instance *in = any_cast<Instance>(inst)) {
          nets = in->getNets();
        }
      }
      if ((result == nullptr) && nets) {
        for (auto o : *nets) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && Variables) {
        for (auto o : *Variables) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && ParamAssigns) {
        for (auto o : *ParamAssigns) {
          const std::string_view pname = o->getLhs()->getName();
          if (pname == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && Typespecs) {
        for (auto o : *Typespecs) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) && scopes) {
        for (auto o : *scopes) {
          if (o->getName() == name) {
            result = o;
            break;
          }
        }
      }
      if ((result == nullptr) || (result && (result->getUhdmType() != UhdmType::Constant) &&
                                  (result->getUhdmType() != UhdmType::ParamAssign))) {
        if (Any *tmpresult = getValue(name, inst, pany, muteError)) {
          result = tmpresult;
        }
      }
      if (result) break;
      if (inst) {
        if (inst->getUhdmType() == UhdmType::Module) {
          break;
        } else {
          inst = inst->getParent();
        }
      }
    }
  }

  if (result && (result->getUhdmType() == UhdmType::RefObj)) {
    RefObj *ref = (RefObj *)result;
    const std::string_view refname = ref->getName();
    if (refname != name) result = getObject(refname, inst, pany, muteError);
    if (result) {
      if (const ParamAssign *const passign = any_cast<ParamAssign>(result)) {
        result = passign->getRhs();
      }
    }
  }
  if ((result == nullptr) && (m_provider != nullptr)) {
    return m_provider->getObject(name, inst, pany);
  }
  return result;
}

// ======================= Deprecated Functions ===============================

void ExprEval::resize(Expr *resizedExp, int32_t size) {
  ExprEval eval(m_provider);
  Constant *c = (Constant *)resizedExp;
  int64_t val = 0;
  if (eval.getInt64(c, &val) && (val == 1)) {
    uint64_t mask = NumUtils::getMask(size);
    c->setValue(std::to_string(mask));
    c->setDecompile(std::to_string(mask));
    c->setConstType(vpiUIntConst);
  }
}

Any *ExprEval::getValue(std::string_view name, const Any *inst, const Any *pany, bool muteError, const Any *checkLoop) {
  if ((inst == nullptr) && (pany == nullptr)) {
    return nullptr;
  }
  Any *result = nullptr;
  Serializer *tmps = nullptr;
  if (inst)
    tmps = inst->getSerializer();
  else
    tmps = pany->getSerializer();
  Serializer &s = *tmps;
  const Any *root = inst;
  const Any *tmp = inst;
  while (tmp) {
    root = tmp;
    tmp = tmp->getParent();
  }
  const Design *des = any_cast<Design>(root);
  if (des) m_design = des;
  std::string_view the_name = name;
  const Any *the_instance = inst;
  if (m_design && (name.find("::") != std::string::npos)) {
    std::vector<std::string_view> res = tokenize(name, "::");
    if (res.size() > 1) {
      const std::string_view packName = res[0];
      const std::string_view varName = res[1];
      the_name = varName;
      Package *pack = nullptr;
      if (m_design->getTopPackages()) {
        for (auto p : *m_design->getTopPackages()) {
          if (p->getName() == packName) {
            pack = p;
            break;
          }
        }
      }
      the_instance = pack;
    }
  }

  while (the_instance) {
    ParamAssignCollection *ParamAssigns = nullptr;
    TypespecCollection *Typespecs = nullptr;
    if (the_instance->getUhdmType() == UhdmType::GenScopeArray) {
    } else if (the_instance->getUhdmType() == UhdmType::Design) {
      ParamAssigns = ((Design *)the_instance)->getParamAssigns();
      Typespecs = ((Design *)the_instance)->getTypespecs();
    } else if (const Scope *spe = any_cast<Scope>(the_instance)) {
      ParamAssigns = spe->getParamAssigns();
      Typespecs = spe->getTypespecs();
    }
    if (ParamAssigns) {
      for (auto p : *ParamAssigns) {
        if (p->getLhs() && (p->getLhs()->getName() == the_name)) {
          result = (Any *)p->getRhs();
          break;
        }
      }
    }
    if ((result == nullptr) && (Typespecs != nullptr)) {
      for (auto p : *Typespecs) {
        if (p->getUhdmType() == UhdmType::EnumTypespec) {
          EnumTypespec *e = (EnumTypespec *)p;
          for (auto ec : *e->getEnumConsts()) {
            if (ec->getName() == the_name) {
              if (const Constant *const c = ec->getValue()) {
                Constant *cc = s.make<Constant>();
                cc->setValue(c->getValue());
                cc->setSize(c->getSize());
                result = cc;
                break;
              }
            }
          }
        }
      }
    }
    if (result && (result->getUhdmType() == UhdmType::Operation)) {
      Operation *op = (Operation *)result;
      if (const RefTypespec *rt = op->getTypespec()) {
        ExprEval eval(m_provider);
        if (Expr *res = eval.flattenPatternAssignments(s, rt->getActual(), (Expr *)result)) {
          if (res->getUhdmType() == UhdmType::Operation) {
            ((Operation *)result)->setOperands(((Operation *)res)->getOperands());
          }
        }
      }
    }
    if (result) break;

    the_instance = the_instance->getParent();
  }

  if (result) {
    UhdmType resultType = result->getUhdmType();
    if (resultType == UhdmType::Constant) {
    } else if (resultType == UhdmType::RefObj) {
      if (result->getName() != name) {
        if (Any *rval = getValue(result->getName(), inst, pany, muteError)) {
          result = rval;
        }
      }
    } else if ((resultType == UhdmType::Operation) || (resultType == UhdmType::HierPath) ||
               (resultType == UhdmType::BitSelect) || (resultType == UhdmType::SysFuncCall)) {
      if (checkLoop && (result == checkLoop)) {
        return nullptr;
      }
      Expr *rval = nullptr;
      if (reduceExpr(any_cast<Expr>(result), pany, &rval, muteError)) result = rval;
    }
  }
  if ((result == nullptr) && (m_provider != nullptr)) {
    result = m_provider->getValue(name, inst, pany);
  }
  return result;
}

const Any *ExprEval::hierarchicalSelector(std::vector<std::string> &select_path, uint32_t level, const Any *object,
                                          bool &invalidValue, const Any *inst, const Any *pany, bool returnTypespec,
                                          bool muteError) {
  if (object == nullptr) return nullptr;
  Serializer &s = (object) ? *object->getSerializer() : *inst->getSerializer();
  if (level >= select_path.size()) {
    if (returnTypespec) {
      if (const Typespec *const tp = any_cast<Typespec>(object)) {
        return tp;
      } else if (const Expr *ep = any_cast<Expr>(object)) {
        if (const RefTypespec *const rt = ep->getTypespec()) {
          return rt->getActual();
        }
      } else if (const IODecl *id = any_cast<IODecl>(object)) {
        if (const RefTypespec *const rt = id->getTypespec()) {
          return rt->getActual();
        }
      }
      return nullptr;
    }
    return (Expr *)object;
  }
  std::string elemName = select_path[level];
  bool lastElem = (level == select_path.size() - 1);
  if (const Variable *const var = any_cast<Variable>(object)) {
    if (const RefTypespec *rt = var->getTypespec()) {
      if (const StructTypespec *stpt = rt->getActual<StructTypespec>()) {
        for (TypespecMember *member : *stpt->getMembers()) {
          if (member->getName() == elemName) {
            if (returnTypespec) {
              if (RefTypespec *mrt = member->getTypespec()) {
                Any *res = mrt->getActual();
                if (lastElem) {
                  return res;
                } else {
                  return hierarchicalSelector(select_path, level + 1, res, invalidValue, inst, pany, returnTypespec,
                                              muteError);
                }
              }
            } else {
              return member->getDefaultValue();
            }
          }
        }
      } else if (const ClassTypespec *ctps = rt->getActual<ClassTypespec>()) {
        const ClassDefn *defn = ctps->getClassDefn();
        while (defn) {
          if (defn->getVariables()) {
            for (Variable *member : *defn->getVariables()) {
              if (member->getName() == elemName) {
                if (returnTypespec) {
                  if (RefTypespec *mrt = member->getTypespec()) {
                    return mrt->getActual();
                  }
                } else {
                  return member;
                }
              }
            }
          }
          const ClassDefn *base_defn = nullptr;
          if (const Extends *ext = defn->getExtends()) {
            if (const RefTypespec *rt = ext->getClassTypespec()) {
              if (const ClassTypespec *tp = rt->getActual<ClassTypespec>()) {
                base_defn = tp->getClassDefn();
              }
            }
          }
          defn = base_defn;
        }
      } else if (returnTypespec) {
        if (const RefTypespec *const rt = var->getTypespec()) {
          if (const ArrayTypespec *const at = rt->getActual<ArrayTypespec>()) {
            if (lastElem) {
              return at;
            } else {
              return hierarchicalSelector(select_path, level + 1, at, invalidValue, inst, pany, returnTypespec,
                                          muteError);
            }
          }
        }
      }
    }
  } else if (const StructTypespec *const stpt = any_cast<StructTypespec>(object)) {
    for (TypespecMember *member : *stpt->getMembers()) {
      if (member->getName() == elemName) {
        Any *res = nullptr;
        if (returnTypespec) {
          if (RefTypespec *mrt = member->getTypespec()) {
            Any *res = mrt->getActual();
            if (lastElem) {
              return res;
            } else {
              return hierarchicalSelector(select_path, level + 1, res, invalidValue, inst, pany, returnTypespec,
                                          muteError);
            }
          }
        } else {
          res = member->getDefaultValue();
        }
        if (lastElem) {
          return res;
        } else {
          return hierarchicalSelector(select_path, level + 1, res, invalidValue, inst, pany, returnTypespec, muteError);
        }
      }
    }
  } else if (const IODecl *const decl = any_cast<IODecl>(object)) {
    if (const Variable *exp = decl->getExpr<Variable>()) {
      if (const RefTypespec *const rt = exp->getTypespec()) {
        if (const StructTypespec *stpt = rt->getActual<StructTypespec>()) {
          for (TypespecMember *member : *stpt->getMembers()) {
            if (member->getName() == elemName) {
              if (returnTypespec) {
                if (RefTypespec *mrt = member->getTypespec()) {
                  Any *res = mrt->getActual();
                  if (lastElem) {
                    return res;
                  } else {
                    return hierarchicalSelector(select_path, level + 1, res, invalidValue, inst, pany, returnTypespec,
                                                muteError);
                  }
                }
              } else {
                return member->getDefaultValue();
              }
            }
          }
        }
      }
    }
    if (returnTypespec) {
      if (const RefTypespec *rt = decl->getTypespec()) {
        if (const Typespec *tps = rt->getActual()) {
          UhdmType ttps = tps->getUhdmType();
          if (ttps == UhdmType::StructTypespec) {
            StructTypespec *stpt = (StructTypespec *)tps;
            for (TypespecMember *member : *stpt->getMembers()) {
              if (member->getName() == elemName) {
                if (RefTypespec *mrt = member->getTypespec()) {
                  Any *res = mrt->getActual();
                  if (lastElem) {
                    return res;
                  } else {
                    return hierarchicalSelector(select_path, level + 1, res, invalidValue, inst, pany, returnTypespec,
                                                muteError);
                  }
                }
              }
            }
          } else if (ttps == UhdmType::ClassTypespec) {
            ClassTypespec *stpt = (ClassTypespec *)tps;
            const ClassDefn *defn = stpt->getClassDefn();
            while (defn) {
              if (defn->getVariables()) {
                for (Variable *member : *defn->getVariables()) {
                  if (member->getName() == elemName) {
                    if (RefTypespec *mrt = member->getTypespec()) {
                      return mrt->getActual();
                    }
                  }
                }
              }
              const ClassDefn *base_defn = nullptr;
              if (const Extends *ext = defn->getExtends()) {
                if (const RefTypespec *rt = ext->getClassTypespec()) {
                  if (const ClassTypespec *tp = rt->getActual<ClassTypespec>()) {
                    base_defn = tp->getClassDefn();
                  }
                }
              }
              defn = base_defn;
            }
          }
        }
      }
    }
  } else if (const Net *const nt = any_cast<Net>(object)) {
    TypespecMemberCollection *members = nullptr;
    if (const StructTypespec *sts = uhdm::getTypespec<StructTypespec>(nt)) {
      members = sts->getMembers();
    } else if (const UnionTypespec *uts = uhdm::getTypespec<UnionTypespec>(nt)) {
      members = uts->getMembers();
    }
    if (members) {
      for (TypespecMember *member : *members) {
        if (member->getName() == elemName) {
          if (returnTypespec) {
            if (RefTypespec *mrt = member->getTypespec()) {
              Any *res = mrt->getActual();
              if (lastElem) {
                return res;
              } else {
                return hierarchicalSelector(select_path, level + 1, res, invalidValue, inst, pany, returnTypespec,
                                            muteError);
              }
            }
          } else {
            return member->getDefaultValue();
          }
        }
      }
    }
  } else if (const Constant *cons = any_cast<Constant>(object)) {
    if (const RefTypespec *rt = cons->getTypespec()) {
      if (const Typespec *ts = rt->getActual()) {
        UhdmType ttps = ts->getUhdmType();
        if (ttps == UhdmType::StructTypespec) {
          StructTypespec *stpt = (StructTypespec *)ts;
          uint64_t from = 0;
          uint64_t width = 0;
          for (TypespecMember *member : *stpt->getMembers()) {
            if (member->getName() == elemName) {
              if (!getBitCount(member, pany, true, &width)) {
                width = 0;
                invalidValue = true;
              }
              if (cons->getSize() <= 64) {
                uint64_t iv = 0;
                invalidValue = !getUInt64(cons, &iv);

                uint64_t mask = 0;
                for (uint64_t i = from; i < uint64_t(from + width); i++) {
                  mask |= ((uint64_t)1 << i);
                }
                uint64_t res = iv & mask;
                res = res >> (from);
                if (Constant *const c = const_cast<Constant *>(cons)) {
                  c->setValue(std::to_string(res));
                  c->setSize(static_cast<int32_t>(width));
                  c->setConstType(vpiUIntConst);
                }
                return cons;
              } else {
                std::string_view val = cons->getValue();
                int32_t ctype = cons->getConstType();
                if (ctype == vpiHexConst) {
                  std::string bin = NumUtils::hexToBin(val);
                  std::string res = bin.substr(from, width);
                  if (Constant *const c = const_cast<Constant *>(cons)) {
                    c->setValue(res);
                    c->setSize(static_cast<int32_t>(width));
                    c->setConstType(vpiBinaryConst);
                  }
                  return cons;
                } else if (ctype == vpiBinaryConst) {
                  std::string_view res = val.substr(from, width);
                  if (Constant *const c = const_cast<Constant *>(cons)) {
                    c->setValue(std::string(res));
                    c->setSize(static_cast<int32_t>(width));
                    c->setConstType(vpiBinaryConst);
                  }
                  return cons;
                }
              }
            } else {
              uint64_t mwidth = 0;
              if (getBitCount(member, pany, true, &mwidth)) {
                from += mwidth;
              }
            }
          }
        }
      }
    }
  }

  int32_t selectIndex = -1;
  if (elemName.find('[') != std::string::npos) {
    std::string_view indexName = ltrim_until(elemName, '[');
    indexName = rtrim_until(indexName, ']');
    if (!NumUtils::parseInt32(indexName, &selectIndex)) {
      selectIndex = -1;
    }
    elemName.clear();
    if (const Operation *oper = any_cast<Operation>(object)) {
      int32_t opType = oper->getOpType();
      if (opType == vpiAssignmentPatternOp) {
        AnyCollection *operands = oper->getOperands();
        int32_t sInd = 0;
        for (auto operand : *operands) {
          if ((selectIndex >= 0) && (sInd == selectIndex)) {
            return hierarchicalSelector(select_path, level + 1, operand, invalidValue, inst, pany, returnTypespec,
                                        muteError);
          }
          sInd++;
        }
      }
    } else if (const LogicTypespec *ltps = any_cast<LogicTypespec>(object)) {
      RangeCollection *ranges = ltps->getRanges();
      if (ranges && (ranges->size() >= 2)) {
        LogicTypespec *tmp = s.make<LogicTypespec>();
        RangeCollection *tmpR = s.makeCollection<Range>();
        for (uint32_t i = 1; i < ranges->size(); i++) {
          tmpR->emplace_back(ranges->at(i));
        }
        tmp->setRanges(tmpR);
        return tmp;
      }
    } else if (const ArrayTypespec *ltps = any_cast<ArrayTypespec>(object)) {
      if (const RefTypespec *rt = ltps->getElemTypespec()) {
        return (Typespec *)rt->getActual();
      }
    } else if (const Constant *const c = any_cast<Constant>(object)) {
      Expr *tmp = nullptr;
      if (reduceConstant(c, selectIndex, pany, &tmp, muteError)) {
        if (returnTypespec) {
          if (RefTypespec *rt = tmp->getTypespec()) {
            return rt->getActual();
          }
          return nullptr;
        }
        return tmp;
      }
      return object;
    }
  } else if (level == 0) {
    return hierarchicalSelector(select_path, level + 1, object, invalidValue, inst, pany, returnTypespec, muteError);
  }

  if (const Operation *oper = any_cast<Operation>(object)) {
    int32_t opType = oper->getOpType();

    if (opType == vpiAssignmentPatternOp) {
      AnyCollection *operands = oper->getOperands();
      Any *defaultPattern = nullptr;
      int32_t sInd = 0;

      int32_t bIndex = -1;
      if (inst) {
        /*
        Any *baseP = nullptr;
        AnyCollection *parameters = nullptr;
        if (inst->getUhdmType() == UhdmType::GenScopeArray) {
        } else if (inst->getUhdmType() == UhdmType::Design) {
          parameters = ((Design *)inst)->Parameters();
        } else if (any_cast<scope *>(inst)) {
          parameters = ((scope *)inst)->Parameters();
        }
        if (parameters) {
          for (auto p : *parameters) {
            if (p->getName() == select_path[0]) {
              baseP = p;
              break;
            }
          }
        }
        */
        if (const Any *baseP = getObject(select_path[0], inst, pany, muteError)) {
          const Typespec *tps = nullptr;
          if (const Parameter *const p = any_cast<Parameter>(baseP)) {
            if (const RefTypespec *rt = p->getTypespec()) {
              tps = rt->getActual();
            }
          } else if (const Operation *const op = any_cast<Operation>(baseP)) {
            if (const RefTypespec *rt = op->getTypespec()) {
              tps = rt->getActual();
            }
          }

          if (tps && (tps->getUhdmType() == UhdmType::ArrayTypespec)) {
            ArrayTypespec *tmp = (ArrayTypespec *)tps;
            if (const RefTypespec *rt = tmp->getElemTypespec()) {
              tps = rt->getActual();
            }
          }
          if (tps && (tps->getUhdmType() == UhdmType::StructTypespec)) {
            StructTypespec *sts = (StructTypespec *)tps;
            if (TypespecMemberCollection *members = sts->getMembers()) {
              uint32_t i = 0;
              for (TypespecMember *member : *members) {
                if (member->getName() == elemName) {
                  bIndex = i;
                  break;
                }
                i++;
              }
            }
          }
        }
      }
      if (inst) {
        const Any *tmpInstance = inst;
        while ((bIndex == -1) && tmpInstance) {
          ParamAssignCollection *ParamAssigns = nullptr;
          if (tmpInstance->getUhdmType() == UhdmType::GenScopeArray) {
          } else if (tmpInstance->getUhdmType() == UhdmType::Design) {
            ParamAssigns = ((Design *)tmpInstance)->getParamAssigns();
          } else if (const Scope *spe = any_cast<Scope>(tmpInstance)) {
            ParamAssigns = spe->getParamAssigns();
          }
          if (ParamAssigns) {
            for (ParamAssign *param : *ParamAssigns) {
              if (param && param->getLhs()) {
                const std::string_view param_name = param->getLhs()->getName();
                if (param_name == select_path[0]) {
                  if (const Parameter *p = any_cast<Parameter>(param->getLhs())) {
                    if (const RefTypespec *rt = p->getTypespec()) {
                      if (const Typespec *tps = rt->getActual()) {
                        if (tps->getUhdmType() == UhdmType::ArrayTypespec) {
                          if (const RefTypespec *ert = ((ArrayTypespec *)tps)->getElemTypespec()) {
                            tps = ert->getActual();
                          }
                        }
                        if (tps && (tps->getUhdmType() == UhdmType::StructTypespec)) {
                          StructTypespec *sts = (StructTypespec *)tps;
                          if (TypespecMemberCollection *members = sts->getMembers()) {
                            uint32_t i = 0;
                            for (TypespecMember *member : *members) {
                              if (member->getName() == elemName) {
                                bIndex = i;
                                break;
                              }
                              i++;
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          tmpInstance = tmpInstance->getParent();
        }
      }
      for (auto operand : *operands) {
        UhdmType operandType = operand->getUhdmType();
        if (operandType == UhdmType::TaggedPattern) {
          TaggedPattern *tpatt = (TaggedPattern *)operand;
          const Typespec *tps = nullptr;
          if (const RefTypespec *rt = tpatt->getTypespec()) {
            tps = rt->getActual();
          }
          if (tps->getName() == "default") {
            defaultPattern = (Any *)tpatt->getPattern();
          }
          if (!elemName.empty() && (tps->getName() == elemName)) {
            const Any *patt = tpatt->getPattern();
            UhdmType pattType = patt->getUhdmType();
            if (pattType == UhdmType::Constant) {
              const Any *ex = nullptr;
              Expr *expr = nullptr;
              invalidValue = !reduceExpr((Expr *)patt, pany, &expr, muteError);
              ex = expr;
              if (level < select_path.size()) {
                ex = hierarchicalSelector(select_path, level + 1, ex, invalidValue, inst, pany, returnTypespec);
              }
              if (returnTypespec) {
                if (const Typespec *tp = any_cast<Typespec>(ex)) {
                  return tp;
                } else if (const Expr *ep = any_cast<Expr>(ex)) {
                  if (const RefTypespec *rt = ep->getTypespec()) {
                    return rt->getActual();
                  }
                } else if (const IODecl *id = any_cast<IODecl>(ex)) {
                  if (const RefTypespec *rt = id->getTypespec()) {
                    return rt->getActual();
                  }
                } else if (const Typespec *tp = any_cast<Typespec>(object)) {
                  return tp;
                } else if (const Expr *ep = any_cast<Expr>(object)) {
                  if (const RefTypespec *const rt = ep->getTypespec()) {
                    return rt->getActual();
                  }
                } else if (const IODecl *id = any_cast<IODecl>(object)) {
                  if (const RefTypespec *const rt = id->getTypespec()) {
                    return rt->getActual();
                  }
                }
                return nullptr;
              }
              return ex;
            } else if (pattType == UhdmType::Operation) {
              return hierarchicalSelector(select_path, level + 1, (Expr *)patt, invalidValue, inst, pany,
                                          returnTypespec);
            }
          }
        } else if (operandType == UhdmType::Constant) {
          if ((bIndex >= 0) && (bIndex == sInd)) {
            return hierarchicalSelector(select_path, level + 1, (Expr *)operand, invalidValue, inst, pany,
                                        returnTypespec);
          }
        }
        sInd++;
      }
      if (defaultPattern) {
        if (Expr *ex = any_cast<Expr>(defaultPattern)) {
          invalidValue = !reduceExpr(ex, pany, &ex, muteError);
          if (returnTypespec) {
            if (Typespec *tp = any_cast<Typespec>(ex)) {
              return tp;
            } else if (Expr *ep = any_cast<Expr>(ex)) {
              if (const RefTypespec *const rt = ep->getTypespec()) {
                return rt->getActual();
              }
            } else if (const IODecl *const id = any_cast<IODecl>(ex)) {
              if (const RefTypespec *const rt = id->getTypespec()) {
                return rt->getActual();
              }
            } else if (const Typespec *const tp = any_cast<Typespec>(object)) {
              return tp;
            } else if (const Expr *const ep = any_cast<Expr>(object)) {
              if (const RefTypespec *const rt = ep->getTypespec()) {
                return rt->getActual();
              }
            } else if (const IODecl *const id = any_cast<IODecl>(object)) {
              if (const RefTypespec *rt = id->getTypespec()) {
                return rt->getActual();
              }
            }
            return nullptr;
          }
          return ex;
        }
      }
    }
  }
  return nullptr;
}

void ExprEval::recursiveFlattening(Serializer &s, AnyCollection *flattened, const AnyCollection *ordered,
                                   std::vector<const Typespec *> fieldTypes) {
  // Flattening
  int32_t index = 0;
  for (Any *op : *ordered) {
    if (op->getUhdmType() == UhdmType::TaggedPattern) {
      TaggedPattern *tp = (TaggedPattern *)op;
      const Typespec *ttp = nullptr;
      if (const RefTypespec *rt = tp->getTypespec()) {
        ttp = rt->getActual();
      }
      UhdmType ttpt = ttp->getUhdmType();
      switch (ttpt) {
        case UhdmType::IntTypespec: {
          flattened->emplace_back(tp->getPattern());
          break;
        }
        case UhdmType::IntegerTypespec: {
          flattened->emplace_back(tp->getPattern());
          break;
        }
        case UhdmType::StringTypespec: {
          Any *sop = (Any *)tp->getPattern();
          UhdmType sopt = sop->getUhdmType();
          if (sopt == UhdmType::Operation) {
            AnyCollection *operands = ((Operation *)sop)->getOperands();
            for (auto op1 : *operands) {
              bool substituted = false;
              if (op1->getUhdmType() == UhdmType::TaggedPattern) {
                TaggedPattern *tp1 = (TaggedPattern *)op1;
                const Typespec *ttp1 = nullptr;
                if (const RefTypespec *rt = tp1->getTypespec()) {
                  ttp1 = rt->getActual();
                }
                UhdmType ttpt1 = ttp1->getUhdmType();
                if (ttpt1 == UhdmType::StringTypespec) {
                  if (ttp1->getName() == "default") {
                    const Any *patt = tp1->getPattern();
                    const Typespec *mold = fieldTypes[index];
                    Operation *subst = s.make<Operation>();
                    AnyCollection *sops = s.makeCollection<Any>();
                    subst->setOperands(sops);
                    subst->setOpType(vpiConcatOp);
                    flattened->emplace_back(subst);
                    UhdmType moldtype = mold->getUhdmType();
                    if (moldtype == UhdmType::StructTypespec) {
                      StructTypespec *molds = (StructTypespec *)mold;
                      for (auto mem : *molds->getMembers()) {
                        if (mem) sops->emplace_back((Any *)patt);
                      }
                    } else if (moldtype == UhdmType::LogicTypespec) {
                      LogicTypespec *molds = (LogicTypespec *)mold;
                      RangeCollection *ranges = molds->getRanges();
                      if (!ranges->empty()) {
                        Range *r = ranges->front();
                        uint64_t from = 0;
                        uint64_t to = 0;

                        if (getUInt64(r->getLeftExpr(), &from) && getUInt64(r->getRightExpr(), &to)) {
                          if (from > to) std::swap(from, to);
                          for (uint64_t i = from; i <= to; ++i) {
                            sops->emplace_back((Any *)patt);
                          }
                        }
                        // TODO: Multidimension
                      }
                    }
                    substituted = true;
                    break;
                  }
                }
              } else if (op1->getUhdmType() == UhdmType::Operation) {
                // recursiveFlattening(s, flattened,
                // ((Operation*)op1)->getOperands(), fieldTypes);
                // substituted = true;
              }
              if (!substituted) {
                flattened->emplace_back(sop);
                break;
              }
            }
          } else {
            flattened->emplace_back(sop);
          }
          break;
        }
        default: flattened->emplace_back(op); break;
      }
    } else {
      flattened->emplace_back(op);
    }
    index++;
  }
}

Expr *ExprEval::flattenPatternAssignments(Serializer &s, const Typespec *tps, Expr *exp) {
  Expr *result = exp;
  if ((!exp) || (!tps)) {
    return result;
  }
  // Reordering
  if (exp->getUhdmType() == UhdmType::Operation) {
    Operation *op = (Operation *)exp;
    if (op->getOpType() == vpiConditionOp) {
      AnyCollection *ops = op->getOperands();
      ops->at(1) = flattenPatternAssignments(s, tps, (Expr *)ops->at(1));
      ops->at(2) = flattenPatternAssignments(s, tps, (Expr *)ops->at(2));
      return result;
    }
    if (op->getOpType() != vpiAssignmentPatternOp) {
      return result;
    }
    if (tps->getUhdmType() == UhdmType::ArrayTypespec) {
      ArrayTypespec *atps = (ArrayTypespec *)tps;
      if (const RefTypespec *rt = atps->getElemTypespec()) {
        tps = rt->getActual();
      }
    }
    if (tps == nullptr) {
      return result;
    }
    if (tps->getUhdmType() != UhdmType::StructTypespec) {
      if (const RefTypespec *rt = op->getTypespec()) {
        tps = rt->getActual();
      }
    }
    if (tps == nullptr) {
      return result;
    }
    if (tps->getUhdmType() == UhdmType::ArrayTypespec) {
      ArrayTypespec *atps = (ArrayTypespec *)tps;
      if (const RefTypespec *rt = atps->getElemTypespec()) {
        tps = rt->getActual();
      }
    }
    if (tps->getUhdmType() != UhdmType::StructTypespec) {
      return result;
    }
    if (op->getFlattened()) {
      return result;
    }
    StructTypespec *stps = (StructTypespec *)tps;
    std::vector<std::string_view> fieldNames;
    std::vector<const Typespec *> fieldTypes;
    for (TypespecMember *memb : *stps->getMembers()) {
      if (const RefTypespec *rt = memb->getTypespec()) {
        fieldNames.emplace_back(memb->getName());
        fieldTypes.emplace_back(rt->getActual());
      }
    }
    AnyCollection *orig = op->getOperands();
    if (orig->size() == 1) {
      for (auto oper : *orig) {
        if (oper->getUhdmType() == UhdmType::Operation) {
          Operation *opi = (Operation *)oper;
          if (opi->getOpType() == vpiAssignmentPatternOp) {
            op = opi;
            orig = op->getOperands();
            break;
          }
        }
      }
    }
    AnyCollection *ordered = s.makeCollection<Any>();
    std::vector<Any *> tmp(fieldNames.size());
    Any *defaultOp = nullptr;
    int32_t index = 0;
    bool flatten = false;
    for (auto oper : *orig) {
      if (oper->getUhdmType() == UhdmType::TaggedPattern) {
        TaggedPattern *tp = (TaggedPattern *)oper;
        const Typespec *ttp = nullptr;
        if (const RefTypespec *rt = tp->getTypespec()) {
          ttp = rt->getActual();
        }
        const std::string_view tname = ttp->getName();
        bool found = false;
        if (tname == "default") {
          defaultOp = oper;
          found = true;
        }
        for (uint32_t i = 0; i < fieldNames.size(); i++) {
          if (tname == fieldNames[i]) {
            tmp[i] = oper;
            found = true;
            break;
          }
        }
        if (found == false) {
          for (uint32_t i = 0; i < fieldTypes.size(); i++) {
            if (ttp->getUhdmType() == fieldTypes[i]->getUhdmType()) {
              tmp[i] = oper;
              found = true;
              break;
            }
          }
        }
        if (found == false) {
          if (!m_muteError) {
            const std::string errMsg(tname);
            s.getErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY, errMsg, exp, nullptr);
          }
          return result;
        }
      } else if (oper->getUhdmType() == UhdmType::Operation) {
        return result;
      } else {
        if (index < (int32_t)tmp.size()) {
          tmp[index] = oper;
        } else {
          if (!m_muteError) {
            s.getErrorHandler()(ErrorType::UHDM_UNDEFINED_PATTERN_KEY, "Out of bound!", exp, nullptr);
          }
        }
      }
      index++;
    }
    index = 0;
    Elaborator elaborator(&s, false, m_muteError);
    for (auto opi : tmp) {
      if (defaultOp && (opi == nullptr)) {
        opi = elaborator.clone<>(defaultOp, defaultOp->getParent());
      }
      if (opi == nullptr) {
        if (!m_muteError) {
          const std::string errMsg(fieldNames[index]);
          s.getErrorHandler()(ErrorType::UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN, errMsg, exp, nullptr);
        }
        return result;
      }
      if (opi->getUhdmType() == UhdmType::TaggedPattern) {
        TaggedPattern *tp = (TaggedPattern *)opi;
        const Any *patt = tp->getPattern();
        if (patt->getUhdmType() == UhdmType::Constant) {
          Constant *c = (Constant *)patt;
          if (c->getSize() == -1) {
            uint64_t uval = 0;
            if (getUInt64(c, &uval) && (uval == 1)) {
              uint64_t sz = 0;
              if (!getBitCount(fieldTypes[index], exp, true, &sz)) {
                sz = 0;
              }
              uint64_t mask = NumUtils::getMask(sz);
              uval = mask;
              c->setValue(std::to_string(uval));
              c->setDecompile(std::to_string(uval));
              c->setConstType(vpiUIntConst);
              c->setSize(static_cast<int32_t>(sz));
            } else if (uval == 0) {
              uint64_t sz = 0;
              if (!getBitCount(fieldTypes[index], exp, true, &sz)) {
                sz = 0;
              }
              c->setValue(std::to_string(uval));
              c->setDecompile(std::to_string(uval));
              c->setConstType(vpiUIntConst);
              c->setSize(static_cast<int32_t>(sz));
            }
          }
        } else if (patt->getUhdmType() == UhdmType::Operation) {
          Operation *patt_op = (Operation *)patt;
          if (patt_op->getOpType() == vpiAssignmentPatternOp) {
            opi = flattenPatternAssignments(s, fieldTypes[index], patt_op);
          }
        }
      }
      ordered->emplace_back(opi);
      index++;
    }
    Operation *opres = elaborator.clone<>(op, op->getParent());
    opres->setOperands(ordered);
    if (flatten) {
      opres->setFlattened(true);
    }
    // Flattening
    AnyCollection *flattened = s.makeCollection<Any>();
    recursiveFlattening(s, flattened, ordered, fieldTypes);
    for (auto o : *flattened) o->setParent(opres);
    opres->setOperands(flattened);
    result = opres;
  }
  return result;
}

bool ExprEval::setValueInInstance(std::string_view lhs, Any *lhsexp, Expr *rhsexp, bool &invalidValue, Serializer &s,
                                  const Any *inst, const Any *scope_exp,
                                  std::map<std::string, const Typespec *, std::less<>> &local_vars, int32_t opType,
                                  bool muteError) {
  bool invalidValueI = false;
  bool invalidValueUI = false;
  bool invalidValueD = false;
  bool invalidValueB = false;
  bool opRhs = false;
  std::string_view lhsname = lhs;
  if (lhsname.empty()) lhsname = lhsexp->getName();
  Expr *rhs = nullptr;
  if (!reduceExpr(rhsexp, nullptr, &rhs, muteError)) return false;
  int64_t valI = 0;
  invalidValueI = !getInt64(rhsexp, &valI);
  uint64_t valUI = 0;
  invalidValueUI = !getUInt64(rhsexp, &valUI);
  if (rhsexp && (rhsexp->getUhdmType() == UhdmType::Constant)) {
    Constant *t = (Constant *)rhsexp;
    if (t->getConstType() != vpiBinaryConst) {
      invalidValueB = true;
    }
  }
  long double valD = 0;
  if (invalidValueI) {
    invalidValueD = !getDouble(rhsexp, &valD);
  }
  uint64_t wordSize = 1;
  const std::string_view name = lhsexp->getName();
  if (const Any *const object = getObject(name, inst, scope_exp, muteError)) {
    invalidValueI = !getWordSize(any_cast<Expr>(object), scope_exp, &wordSize);
  }
  ParamAssignCollection *ParamAssigns = nullptr;
  if (inst && inst->getUhdmType() == UhdmType::GenScopeArray) {
  } else if (inst && inst->getUhdmType() == UhdmType::Design) {
    ParamAssigns = ((Design *)inst)->getParamAssigns();
    if (ParamAssigns == nullptr) {
      ((Design *)inst)->setParamAssigns(s.makeCollection<ParamAssign>());
      ParamAssigns = ((Design *)inst)->getParamAssigns();
    }
  } else if (const Scope *spe = any_cast<Scope>(inst)) {
    ParamAssigns = spe->getParamAssigns();
    if (ParamAssigns == nullptr) {
      const_cast<Scope *>(spe)->setParamAssigns(s.makeCollection<ParamAssign>());
      ParamAssigns = spe->getParamAssigns();
    }
  }
  if (invalidValueI && invalidValueD) {
    if (ParamAssigns) {
      for (ParamAssignCollection::iterator itr = ParamAssigns->begin(); itr != ParamAssigns->end(); itr++) {
        if ((*itr)->getLhs()->getName() == lhsname) {
          ParamAssigns->erase(itr);
          break;
        }
      }
      ParamAssign *pa = s.make<ParamAssign>();
      pa->setRhs(rhsexp);
      Parameter *param = s.make<Parameter>();
      param->setName(lhsname);
      pa->setLhs(param);
      ParamAssigns->emplace_back(pa);
      if (rhsexp &&
          ((rhsexp->getUhdmType() == UhdmType::Operation) || (rhsexp->getUhdmType() == UhdmType::ArrayExpr))) {
        opRhs = true;
      }
    }
  } else if (invalidValueI) {
    if (ParamAssigns) {
      for (ParamAssignCollection::iterator itr = ParamAssigns->begin(); itr != ParamAssigns->end(); itr++) {
        if ((*itr)->getLhs()->getName() == lhsname) {
          ParamAssigns->erase(itr);
          break;
        }
      }
      Constant *c = s.make<Constant>();
      c->setValue(std::to_string((double)valD));
      c->setDecompile(std::to_string(valD));
      c->setSize(64);
      c->setConstType(vpiRealConst);
      ParamAssign *pa = s.make<ParamAssign>();
      pa->setRhs(c);
      Parameter *param = s.make<Parameter>();
      param->setName(lhsname);
      pa->setLhs(param);
      ParamAssigns->emplace_back(pa);
    }
  } else {
    if (ParamAssigns) {
      const Any *prevRhs = nullptr;
      Constant *c = any_cast<Constant>(rhsexp);
      if (c == nullptr) {
        c = s.make<Constant>();
        c->setValue(std::to_string(valI));
        c->setDecompile(std::to_string(valI));
        c->setSize(64);
        c->setConstType(vpiIntConst);
      }
      if (lhsexp->getUhdmType() == UhdmType::Operation) {
        for (ParamAssignCollection::iterator itr = ParamAssigns->begin(); itr != ParamAssigns->end(); itr++) {
          if ((*itr)->getLhs()->getName() == lhsname) {
            prevRhs = (*itr)->getRhs();
            ParamAssigns->erase(itr);
            break;
          }
        }
        Operation *op = (Operation *)lhsexp;
        if (op->getOpType() == vpiConcatOp) {
          std::string rhsbinary;
          invalidValue = !formatBinary(c, &rhsbinary);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          AnyCollection *operands = op->getOperands();
          uint64_t accumul = 0;
          for (Any *oper : *operands) {
            const std::string_view name = oper->getName();
            uint64_t si = 0;
            invalidValue = !getBitCount(oper, lhsexp, true, &si, muteError);
            std::string part;
            for (uint64_t i = accumul; i < accumul + si; i++) {
              part += rhsbinary[i];
            }
            std::reverse(part.begin(), part.end());
            Constant *c = s.make<Constant>();
            c->setValue(part);
            c->setDecompile(part);
            c->setSize(static_cast<int32_t>(part.size()));
            c->setConstType(vpiBinaryConst);
            setValueInInstance(name, oper, c, invalidValue, s, inst, lhsexp, local_vars, vpiConcatOp, muteError);
            accumul = accumul + si;
          }
        }
      } else if (lhsexp->getUhdmType() == UhdmType::IndexedPartSelect) {
        for (ParamAssignCollection::iterator itr = ParamAssigns->begin(); itr != ParamAssigns->end(); itr++) {
          if ((*itr)->getLhs()->getName() == lhsname) {
            prevRhs = (*itr)->getRhs();
            ParamAssigns->erase(itr);
            break;
          }
        }
        IndexedPartSelect *sel = (IndexedPartSelect *)lhsexp;
        const std::string_view name = lhsexp->getName();
        if (const Any *const object = getObject(name, inst, scope_exp, muteError)) {
          std::string lhsbinary;
          const Typespec *tps = nullptr;
          if (const Expr *elhs = any_cast<const Expr *>(object)) {
            if (const RefTypespec *rt = elhs->getTypespec()) {
              tps = rt->getActual();
            }
          }
          uint64_t si = 0;
          if (!getBitCount(tps, lhsexp, true, &si)) si = 0;
          if (prevRhs && prevRhs->getUhdmType() == UhdmType::Constant) {
            const Constant *prev = (Constant *)prevRhs;
            invalidValue = !formatBinary(prev, &lhsbinary);
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }
          Expr *bexpr = nullptr;
          invalidValue = !reduceExpr(sel->getBaseExpr(), lhsexp, &bexpr, muteError);
          uint64_t base = 0;
          invalidValue = !getUInt64(bexpr, &base);

          Expr *wexpr = nullptr;
          invalidValue = !reduceExpr(sel->getWidthExpr(), lhsexp, &wexpr, muteError);
          uint64_t offset = 0;
          invalidValue = !getUInt64(wexpr, &offset);
          std::string rhsbinary;
          invalidValue = !formatBinary(c, &rhsbinary);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          if (sel->getIndexedPartSelectType() == vpiPosIndexed) {
            int32_t index = 0;
            for (uint64_t i = base; i < base + offset; i++) {
              if (i < lhsbinary.size()) lhsbinary[i] = rhsbinary[index];
              index++;
            }
          } else {
            int32_t index = 0;
            for (uint64_t i = base; i > base - offset; i--) {
              if (i < lhsbinary.size()) lhsbinary[i] = rhsbinary[index];
              index++;
            }
          }
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.make<Constant>();
          c->setValue(lhsbinary);
          c->setDecompile(lhsbinary);
          c->setSize(static_cast<int32_t>(lhsbinary.size()));
          c->setConstType(vpiBinaryConst);
        }
      } else if (lhsexp->getUhdmType() == UhdmType::PartSelect) {
        for (ParamAssignCollection::iterator itr = ParamAssigns->begin(); itr != ParamAssigns->end(); itr++) {
          if ((*itr)->getLhs()->getName() == lhsname) {
            prevRhs = (*itr)->getRhs();
            ParamAssigns->erase(itr);
            break;
          }
        }
        PartSelect *sel = (PartSelect *)lhsexp;
        const std::string_view name = lhsexp->getName();
        if (const Any *const object = getObject(name, inst, scope_exp, muteError)) {
          std::string lhsbinary;
          const Typespec *tps = nullptr;
          if (const Expr *elhs = any_cast<const Expr *>(object)) {
            if (const RefTypespec *rt = elhs->getTypespec()) {
              tps = rt->getActual();
            }
          }
          uint64_t si = 0;
          if (!getBitCount(tps, lhsexp, true, &si)) {
            si = 0;
            invalidValue = true;
          }
          if (prevRhs && (prevRhs->getUhdmType() == UhdmType::Constant)) {
            const Constant *prev = (Constant *)prevRhs;
            invalidValue = !formatBinary(prev, &lhsbinary);
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            lhsbinary.append(si, 'x');
          }
          Expr *lexpr = nullptr;
          invalidValue = !reduceExpr(sel->getLeftExpr(), lhsexp, &lexpr, muteError);
          uint64_t left = 0;
          invalidValue = !getUInt64(lexpr, &left);

          Expr *rexpr = nullptr;
          invalidValue = !reduceExpr(sel->getRightExpr(), lhsexp, &rexpr, muteError);
          uint64_t right = 0;
          invalidValue = !getUInt64(rexpr, &right);
          std::string rhsbinary;
          invalidValue = !formatBinary(c, &rhsbinary);
          std::reverse(rhsbinary.begin(), rhsbinary.end());
          if (left > right) {
            int32_t index = 0;
            for (uint64_t i = right; i <= left; i++) {
              if (i < lhsbinary.size()) lhsbinary[i] = rhsbinary[index];
              index++;
            }
          } else {
            int32_t index = 0;
            for (uint64_t i = left; i <= right; i++) {
              if (i < lhsbinary.size()) lhsbinary[i] = rhsbinary[index];
              index++;
            }
          }
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.make<Constant>();
          c->setValue(lhsbinary);
          c->setDecompile(lhsbinary);
          c->setSize(static_cast<int32_t>(lhsbinary.size()));
          c->setConstType(vpiBinaryConst);
        }
      } else if (lhsexp->getUhdmType() == UhdmType::BitSelect) {
        BitSelect *sel = (BitSelect *)lhsexp;
        Expr *iexpr = nullptr;
        invalidValue = !reduceExpr(sel->getIndex(), lhsexp, &iexpr, muteError);
        uint64_t index = 0;
        invalidValue = !getUInt64(iexpr, &index);
        const std::string_view name = lhsexp->getName();
        if (const Any *const object = getObject(name, inst, scope_exp, muteError)) {
          if (object->getUhdmType() == UhdmType::ParamAssign) {
            ParamAssign *param = (ParamAssign *)object;
            if (param->getRhs()->getUhdmType() == UhdmType::ArrayExpr) {
              ArrayExpr *array = (ArrayExpr *)param->getRhs();
              ExprCollection *values = array->getExprs();
              values->resize(index + 1);
              (*values)[index] = rhsexp;
              return false;
            }
          }

          for (ParamAssignCollection::iterator itr = ParamAssigns->begin(); itr != ParamAssigns->end(); itr++) {
            if ((*itr)->getLhs()->getName() == lhsname) {
              prevRhs = (*itr)->getRhs();
              ParamAssigns->erase(itr);
              break;
            }
          }
          std::string lhsbinary;
          const Typespec *tps = nullptr;
          if (const Expr *elhs = any_cast<const Expr *>(object)) {
            if (const RefTypespec *rt = elhs->getTypespec()) {
              tps = rt->getActual();
            }
          }
          uint64_t si = 0;
          if (getBitCount(tps, lhsexp, true, &si)) {
            si = 0;
            invalidValue = true;
          }
          if (prevRhs && prevRhs->getUhdmType() == UhdmType::Constant) {
            const Constant *prev = (Constant *)prevRhs;
            if (prev->getConstType() == vpiBinaryConst) {
              std::string_view val = prev->getValue();
              lhsbinary = val;
            } else {
              uint64_t val = 0;
              invalidValue = !getUInt64(prev, &val);
              lhsbinary = NumUtils::toBinary(static_cast<int32_t>(si), val);
            }
            std::reverse(lhsbinary.begin(), lhsbinary.end());
          } else {
            for (uint32_t i = 0; i < si; i++) {
              lhsbinary += "x";
            }
          }

          int64_t size_rhs = ((Constant *)rhsexp)->getSize();
          if ((wordSize != 1) && (((int64_t)wordSize) < size_rhs)) size_rhs = wordSize;
          std::string tobinary = NumUtils::toBinary(size_rhs, valUI);
          std::reverse(tobinary.begin(), tobinary.end());
          for (int32_t i = 0; i < size_rhs; i++) {
            if ((((index * size_rhs) + i) < si) && (((index * size_rhs) + i) < lhsbinary.size())) {
              lhsbinary[(index * size_rhs) + i] = tobinary[i];
            }
          }
          std::reverse(lhsbinary.begin(), lhsbinary.end());
          c = s.make<Constant>();
          c->setValue(lhsbinary);
          c->setDecompile(lhsbinary);
          c->setSize(static_cast<int32_t>(lhsbinary.size()));
          c->setConstType(vpiBinaryConst);

          RefTypespec *rt = s.make<RefTypespec>();
          rt->setActual(const_cast<Typespec *>(tps));
          rt->setParent(c);
          c->setTypespec(rt);
        } else {
          auto itr = local_vars.find(lhs);
          if (itr != local_vars.end()) {
            if (const Typespec *tps = itr->second) {
              if (tps->getUhdmType() == UhdmType::ArrayTypespec) {
                ParamAssign *pa = s.make<ParamAssign>();
                ParamAssigns->emplace_back(pa);
                ArrayExpr *array = s.make<ArrayExpr>();
                ExprCollection *values = s.makeCollection<Expr>();
                values->resize(index + 1);
                (*values)[index] = rhsexp;
                array->setExprs(values);
                pa->setRhs(array);
                Parameter *param = s.make<Parameter>();
                param->setName(lhsname);
                pa->setLhs(param);
                return false;
              }
            }
          }
        }
      } else {
        for (ParamAssignCollection::iterator itr = ParamAssigns->begin(); itr != ParamAssigns->end(); itr++) {
          if ((*itr)->getLhs()->getName() == lhsname) {
            prevRhs = (*itr)->getRhs();
            ParamAssigns->erase(itr);
            break;
          }
        }
      }
      if (opType == vpiAddOp) {
        uint64_t prevVal = 0;
        if (!(invalidValue = !getUInt64((Expr *)prevRhs, &prevVal))) {
          uint64_t newVal = valUI + prevVal;
          c->setValue(std::to_string(newVal));
          c->setDecompile(std::to_string(newVal));
        }
        c->setConstType(vpiUIntConst);
      } else if (opType == vpiSubOp) {
        int64_t prevVal = 0;
        if (!(invalidValue = !getInt64((Expr *)prevRhs, &prevVal))) {
          int64_t newVal = prevVal - valI;
          c->setValue(std::to_string(newVal));
          c->setDecompile(std::to_string(newVal));
        }
        c->setConstType(vpiIntConst);
      } else if (opType == vpiMultOp) {
        int64_t prevVal = 0;
        if (!(invalidValue = !getInt64((Expr *)prevRhs, &prevVal))) {
          int64_t newVal = prevVal * valI;
          c->setValue(std::to_string(newVal));
          c->setDecompile(std::to_string(newVal));
        }
        c->setConstType(vpiIntConst);
      } else if (opType == vpiDivOp) {
        int64_t prevVal = 0;
        if (!(invalidValue = !getInt64((Expr *)prevRhs, &prevVal))) {
          int64_t newVal = prevVal / valI;
          c->setValue(std::to_string(newVal));
          c->setDecompile(std::to_string(newVal));
        }
        c->setConstType(vpiIntConst);
      }
      if ((c->getSize() == -1) && (c->getConstType() == vpiBinaryConst)) {
        uint64_t size = 0;
        bool tmpInvalidValue = false;
        if (getBitCount(lhsexp, scope_exp, true, &size)) {
          auto itr = local_vars.find(lhs);
          if (itr != local_vars.end()) {
            if (const Typespec *tps = itr->second) {
              if (!getBitCount(tps, scope_exp, true, &size)) {
                tmpInvalidValue = true;
              }
            }
          }
        } else {
          tmpInvalidValue = true;
        }
        if (!tmpInvalidValue) {
          std::string bval;
          if (valUI) {
            bval.append(size, '1');
          } else {
            bval = NumUtils::toBinary(size, valUI);
          }
          c->setValue(bval);
          c->setDecompile(bval);
          c->setSize(size);
        }
      }
      ParamAssign *pa = s.make<ParamAssign>();
      pa->setRhs(c);
      Parameter *param = s.make<Parameter>();
      param->setName(lhsname);
      pa->setLhs(param);
      ParamAssigns->emplace_back(pa);
    }
  }
  if (invalidValueI && invalidValueD && invalidValueB && (!opRhs)) {
    invalidValue = true;
  }
  return invalidValue;
}

void ExprEval::evalStmt(std::string_view funcName, Scopes &scopes, bool &invalidValue, bool &continue_flag,
                        bool &break_flag, bool &return_flag, const Any *inst, const Any *stmt,
                        std::map<std::string, const Typespec *, std::less<>> &local_vars, bool muteError) {
  if (invalidValue) {
    return;
  }
  Serializer &s = *inst->getSerializer();
  UhdmType stt = stmt->getUhdmType();
  switch (stt) {
    case UhdmType::CaseStmt: {
      CaseStmt *st = (CaseStmt *)stmt;
      Expr *cond = (Expr *)st->getCondition();

      Expr *sexpr = nullptr;
      invalidValue = !reduceExpr(cond, nullptr, &sexpr, muteError);
      int64_t val = 0;
      invalidValue = !getInt64(sexpr, &val);
      for (CaseItem *item : *st->getCaseItems()) {
        if (AnyCollection *exprs = item->getExprs()) {
          bool done = false;
          for (Any *exp : *exprs) {
            Expr *eexpr = nullptr;
            invalidValue = !reduceExpr(any_cast<Expr>(exp), nullptr, &eexpr, muteError);
            int64_t vexp = 0;
            invalidValue = !getInt64(eexpr, &vexp);
            if (val == vexp) {
              evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(),
                       item->getStmt(), local_vars, muteError);
              done = true;
              break;
            }
          }
          if (done) break;
        }
      }
      break;
    }
    case UhdmType::IfElse: {
      IfElse *st = (IfElse *)stmt;
      Expr *cond = (Expr *)st->getCondition();
      Expr *cexpr = nullptr;
      invalidValue = !reduceExpr(cond, nullptr, &cexpr, muteError);
      int64_t val = 0;
      invalidValue = !getInt64(cexpr, &val);
      if (val > 0) {
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(), st->getStmt(),
                 local_vars, muteError);
      } else {
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(),
                 st->getElseStmt(), local_vars, muteError);
      }
      break;
    }
    case UhdmType::IfStmt: {
      IfStmt *st = (IfStmt *)stmt;
      Expr *cond = (Expr *)st->getCondition();
      Expr *cexpr = nullptr;
      invalidValue = !reduceExpr(cond, nullptr, &cexpr, muteError);
      int64_t val = 0;
      invalidValue = !getInt64(cexpr, &val);
      if (val > 0) {
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(), st->getStmt(),
                 local_vars, muteError);
      }
      break;
    }
    case UhdmType::Begin: {
      Begin *st = (Begin *)stmt;
      if (st->getVariables()) {
        for (auto var : *st->getVariables()) {
          if (const RefTypespec *rt = var->getTypespec()) {
            local_vars.emplace(var->getName(), rt->getActual());
          }
        }
      }
      if (st->getStmts()) {
        for (auto bst : *st->getStmts()) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(), bst,
                   local_vars, muteError);
          if (continue_flag || break_flag || return_flag) {
            return;
          }
        }
      }
      break;
    }
    case UhdmType::Assignment: {
      Assignment *st = (Assignment *)stmt;
      const std::string_view lhs = st->getLhs()->getName();
      Expr *lhsexp = st->getLhs();
      const Expr *rhs = st->getRhs<Expr>();
      Expr *rhsexp = nullptr;
      invalidValue = !reduceExpr(rhs, nullptr, &rhsexp, muteError);
      invalidValue =
          setValueInInstance(lhs, lhsexp, rhsexp, invalidValue, s, inst, stmt, local_vars, st->getOpType(), muteError);
      break;
    }
    case UhdmType::AssignStmt: {
      AssignStmt *st = (AssignStmt *)stmt;
      const std::string_view lhs = st->getLhs()->getName();
      Expr *lhsexp = st->getLhs();
      const Expr *rhs = st->getRhs();
      Expr *rhsexp = nullptr;
      invalidValue = !reduceExpr(rhs, nullptr, &rhsexp, muteError);
      invalidValue = setValueInInstance(lhs, lhsexp, rhsexp, invalidValue, s, inst, stmt, local_vars, 0, muteError);
      break;
    }
    case UhdmType::Repeat: {
      Repeat *st = (Repeat *)stmt;
      const Expr *cond = st->getCondition();
      Expr *rcond1 = nullptr;
      invalidValue = !reduceExpr((Expr *)cond, nullptr, &rcond1, muteError);
      Expr *rcond2 = nullptr;
      invalidValue = !reduceExpr(rcond1, nullptr, &rcond2, muteError);
      int64_t val = 0;
      invalidValue = !getInt64(rcond2, &val);
      if (invalidValue == false) {
        for (int32_t i = 0; i < val; i++) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(), st->getStmt(),
                   local_vars, muteError);
        }
      }
      break;
    }
    case UhdmType::ForStmt: {
      ForStmt *st = (ForStmt *)stmt;
      if (const Any *stmt = st->getForInitStmt()) {
        if (stmt->getUhdmType() == UhdmType::Assignment) {
          Assignment *assign = (Assignment *)stmt;
          if (const RefTypespec *rt = assign->getLhs()->getTypespec()) {
            local_vars.emplace(assign->getLhs()->getName(), rt->getActual());
          }
        }
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(),
                 st->getForInitStmt(), local_vars, muteError);
      }
      if (st->getForInitStmts()) {
        for (auto s : *st->getForInitStmts()) {
          if (s->getUhdmType() == UhdmType::Assignment) {
            Assignment *assign = (Assignment *)s;
            if (const RefTypespec *rt = assign->getLhs()->getTypespec()) {
              local_vars.emplace(assign->getLhs()->getName(), rt->getActual());
            }
          }
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(), s, local_vars,
                   muteError);
        }
      }
      while (1) {
        Expr *cond = (Expr *)st->getCondition();
        if (cond) {
          Expr *cexpr = nullptr;
          invalidValue = !reduceExpr(cond, nullptr, &cexpr, muteError);
          int64_t val = 0;
          invalidValue = !getInt64(cexpr, &val);
          if (val == 0) {
            break;
          }
          if (invalidValue) break;
        }
        evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(), st->getStmt(),
                 local_vars, muteError);
        if (invalidValue) break;
        if (continue_flag) {
          continue_flag = false;
          continue;
        }
        if (break_flag) {
          break_flag = false;
          break;
        }
        if (return_flag) {
          break;
        }
        if (st->getForIncStmt()) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(),
                   st->getForIncStmt(), local_vars, muteError);
        }
        if (invalidValue) break;
        if (st->getForIncStmts()) {
          for (auto s : *st->getForIncStmts()) {
            evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(), s,
                     local_vars, muteError);
          }
        }
        if (invalidValue) break;
      }
      break;
    }
    case UhdmType::ReturnStmt: {
      ReturnStmt *st = (ReturnStmt *)stmt;
      if (const Expr *cond = st->getCondition()) {
        Expr *rhsexp = nullptr;
        invalidValue = !reduceExpr(cond, nullptr, &rhsexp, muteError);
        RefObj *lhsexp = s.make<RefObj>();
        lhsexp->setName(funcName);
        invalidValue =
            setValueInInstance(funcName, lhsexp, rhsexp, invalidValue, s, inst, stmt, local_vars, 0, muteError);
        return_flag = true;
      }
      break;
    }
    case UhdmType::WhileStmt: {
      WhileStmt *st = (WhileStmt *)stmt;
      if (const Expr *cond = st->getCondition()) {
        while (1) {
          Expr *cexpr = nullptr;
          invalidValue = !reduceExpr(cond, nullptr, &cexpr, muteError);
          int64_t val = 0;
          invalidValue = !getInt64(cexpr, &val);
          if (invalidValue) break;
          if (val == 0) {
            break;
          }
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(), st->getStmt(),
                   local_vars, muteError);
          if (invalidValue) break;
          if (continue_flag) {
            continue_flag = false;
            continue;
          }
          if (break_flag) {
            break_flag = false;
            break;
          }
          if (return_flag) {
            break;
          }
        }
      }
      break;
    }
    case UhdmType::DoWhile: {
      DoWhile *st = (DoWhile *)stmt;
      if (const Expr *cond = st->getCondition()) {
        while (1) {
          evalStmt(funcName, scopes, invalidValue, continue_flag, break_flag, return_flag, scopes.back(), st->getStmt(),
                   local_vars, muteError);
          if (invalidValue) break;
          if (continue_flag) {
            continue_flag = false;
            continue;
          }
          if (break_flag) {
            break_flag = false;
            break;
          }
          if (return_flag) {
            break;
          }

          Expr *cexpr = nullptr;
          invalidValue = !reduceExpr(cond, nullptr, &cexpr, muteError);
          int64_t val = 0;
          invalidValue = !getInt64(cexpr, &val);
          if (invalidValue) break;
          if (val == 0) {
            break;
          }
        }
      }
      break;
    }
    case UhdmType::ContinueStmt: {
      continue_flag = true;
      break;
    }
    case UhdmType::BreakStmt: {
      break_flag = true;
      break;
    }
    case UhdmType::Operation: {
      Operation *op = (Operation *)stmt;
      // ++, -- ops
      Expr *result = nullptr;
      if (!reduceExpr(op, nullptr, &result, muteError)) {
        invalidValue = true;
      }
      break;
    }
    default: {
      invalidValue = true;
      if (!muteError && !m_muteError) {
        const std::string errMsg(inst->getName());
        s.getErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg, stmt, nullptr);
      }
      break;
    }
  }
}

Expr *ExprEval::evalFunc(const Function *func, std::vector<Any *> *args, bool &invalidValue, const Any *inst,
                         const Any *pany, bool muteError) {
  if (func == nullptr) {
    invalidValue = true;
    return nullptr;
  }
  Serializer &s = *func->getSerializer();
  const std::string_view name = func->getName();
  // set internal scope stack
  Scopes scopes;
  Module *modinst = s.make<Module>();
  modinst->setParent((Any *)inst);
  if (const Instance *pack = func->getInstance()) {
    modinst->setTaskFuncs(pack->getTaskFuncs());
    modinst->setParameters(pack->getParameters());
  }
  ParamAssignCollection *ParamAssigns = nullptr;
  if (inst && inst->getUhdmType() == UhdmType::GenScopeArray) {
  } else if (inst && inst->getUhdmType() == UhdmType::Design) {
    ParamAssigns = ((Design *)inst)->getParamAssigns();
  } else if (const Scope *spe = any_cast<Scope>(inst)) {
    ParamAssigns = spe->getParamAssigns();
  }
  std::map<std::string, const Typespec *, std::less<>> vars;
  if (ParamAssigns) {
    modinst->setParamAssigns(s.makeCollection<ParamAssign>());
    for (auto p : *ParamAssigns) {
      Elaborator elaborator(&s, false, muteError);
      ParamAssign *pp = elaborator.clone<>(p, nullptr);
      modinst->getParamAssigns()->emplace_back(pp);
      const Typespec *tps = nullptr;
      if (const Expr *lhs = any_cast<const Expr *>(p->getLhs())) {
        if (const RefTypespec *rt = lhs->getTypespec()) {
          tps = rt->getActual();
        }
      }
      vars.emplace(std::string(p->getLhs()->getName()), tps);
    }
  }
  // set args
  if (func->getIODecls()) {
    uint32_t index = 0;
    for (auto io : *func->getIODecls()) {
      if (args && (index < args->size())) {
        const std::string_view ioname = io->getName();
        if (io->getTypespec() == nullptr) {
          RefTypespec *rt = s.make<RefTypespec>();
          rt->setParent(io);
          io->setTypespec(rt);
        }
        if (io->getTypespec()->getActual() == nullptr) {
          io->getTypespec()->setActual(s.make<LogicTypespec>());
        }
        Typespec *tps = io->getTypespec()->getActual();
        vars.emplace(ioname, tps);
        Expr *ioexp = (Expr *)args->at(index);
        Expr *exparg = nullptr;
        if (reduceExpr(ioexp, pany, &exparg, muteError)) {
          if (exparg->getTypespec() == nullptr) {
            RefTypespec *crt = s.make<RefTypespec>();
            crt->setParent(exparg);
            exparg->setTypespec(crt);
          }
          exparg->getTypespec()->setActual(tps);
          std::map<std::string, const Typespec *, std::less<>> local_vars;
          invalidValue =
              setValueInInstance(ioname, io, exparg, invalidValue, s, modinst, func, local_vars, 0, muteError);
        }
      }
      index++;
    }
  }
  if (func->getVariables()) {
    for (auto var : *func->getVariables()) {
      if (const RefTypespec *rt = var->getTypespec()) {
        vars.emplace(var->getName(), rt->getActual());
      }
    }
  }
  Typespec *funcReturnTypespec = nullptr;
  if (const RefTypespec *const rt = func->getReturn()) {
    funcReturnTypespec = const_cast<Typespec *>(rt->getActual());
  }
  if (funcReturnTypespec == nullptr) {
    funcReturnTypespec = s.make<LogicTypespec>();
  }
  Variable *var = s.make<Variable>();
  var->setName(name);
  RefTypespec *frtrt = s.make<RefTypespec>();
  frtrt->setParent(var);
  frtrt->setActual(funcReturnTypespec);
  var->setTypespec(frtrt);
  modinst->getVariables(true)->emplace_back(var);
  vars.emplace(name, funcReturnTypespec);
  scopes.emplace_back(modinst);
  if (const Any *the_stmt = func->getStmt()) {
    UhdmType stt = the_stmt->getUhdmType();
    bool return_flag = false;
    switch (stt) {
      case UhdmType::Begin: {
        Begin *st = (Begin *)the_stmt;
        bool continue_flag = false;
        bool break_flag = false;
        for (auto stmt : *st->getStmts()) {
          evalStmt(name, scopes, invalidValue, continue_flag, break_flag, return_flag, modinst, stmt, vars, muteError);
          if (return_flag) break;
          if (continue_flag || break_flag) {
            if (!muteError && !m_muteError) {
              const std::string errMsg(inst->getName());
              s.getErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg, stmt, nullptr);
            }
          }
        }
        break;
      }
      default: {
        bool continue_flag = false;
        bool break_flag = false;
        evalStmt(name, scopes, invalidValue, continue_flag, break_flag, return_flag, modinst, the_stmt, vars,
                 muteError);
        if (continue_flag || break_flag) {
          if (!muteError && !m_muteError) {
            const std::string errMsg(inst->getName());
            s.getErrorHandler()(ErrorType::UHDM_UNSUPPORTED_STMT, errMsg, the_stmt, nullptr);
          }
        }
        break;
      }
    }
  }
  // return value
  if (modinst->getParamAssigns()) {
    for (auto p : *modinst->getParamAssigns()) {
      const std::string n(p->getLhs()->getName());
      if ((!n.empty()) && (vars.find(n) == vars.end())) {
        invalidValue = true;
        return nullptr;
      }
    }
    for (auto p : *modinst->getParamAssigns()) {
      if (p->getLhs()->getName() == name) {
        if (p->getRhs() && (p->getRhs()->getUhdmType() == UhdmType::Constant)) {
          Constant *c = (Constant *)p->getRhs();
          std::string_view val = c->getValue();
          if ((val.find("X") != std::string::npos) || (val.find("x") != std::string::npos)) {
            invalidValue = true;
            return nullptr;
          }
        }
        const Typespec *tps = nullptr;
        if (const RefTypespec *rt = func->getReturn()) {
          tps = rt->getActual();
        }
        if (tps && (tps->getUhdmType() == UhdmType::LogicTypespec)) {
          LogicTypespec *ltps = (LogicTypespec *)tps;
          uint64_t si = 0;
          invalidValue = !getBitCount(tps, pany, true, &si, true);
          if (p->getRhs() && (p->getRhs()->getUhdmType() == UhdmType::Constant)) {
            Constant *c = (Constant *)p->getRhs();
            Elaborator elaborator(&s, false, muteError);
            c = elaborator.clone<>(c, nullptr);
            if (c->getConstType() == vpiBinaryConst) {
              std::string_view val = c->getValue();
              if (val.size() > si) {
                val.remove_prefix(val.size() - si);
                c->setValue(val);
                c->setDecompile(val);
              } else if (ltps->getSigned()) {
                if (val == "1") {
                  c->setValue("-1");
                  c->setDecompile("-1");
                  c->setConstType(vpiIntConst);
                }
              }
            } else {
              uint64_t mask = NumUtils::getMask(si);
              int64_t v = 0;
              invalidValue = !getInt64(c, &v);
              v = v & mask;
              c->setValue(std::to_string(v));
              c->setDecompile(std::to_string(v));
              c->setConstType(vpiUIntConst);
            }
            c->setSize(static_cast<int32_t>(si));
            return c;
          }
        }
        return (Expr *)p->getRhs();
      }
    }
  }
  invalidValue = true;
  return nullptr;
}
}  // namespace uhdm

/*
 * TODO:
 * Fix calculating size of following types.
 * 1) Structs:
 * struct {
 *   logic [3:0] a;
 *   logic [3:0] b;
 * } s;
 * $bits(s) should be 8, but currently it's size can't be computed at
 * reduction time. 2) Enums: enum {ONE,TWO,THREE} a; $bits(a) should be 32 as
 * by default enum is of type int32_t.
 */

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
