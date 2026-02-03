// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <stdlib.h>

#include <iostream>
#include <memory>

#include "gtest/gtest.h"
#include "uhdm/vhpi_user.h"  // vpi_user functions.
#include "uhdm/vpi_uhdm.h"   // struct UhdmHandle
#include "uhdm/sv_vpi_user.h"

TEST(VpiValue, ToString) {
  s_vpi_value value;

  value.format = vpiIntVal;
  value.value.integer = 42;
  EXPECT_EQ(VpiValue2String(&value), "42");

  value.format = vpiScalarVal;
  value.value.integer = vpiX;
  EXPECT_EQ(VpiValue2String(&value), "X");

  value.format = vpiStringVal;
  value.value.str = (PLI_BYTE8 *)"helloworld";
  EXPECT_EQ(VpiValue2String(&value), "helloworld");

  value.format = vpiHexStrVal;
  value.value.str = (PLI_BYTE8 *)"FEEDCAFE";
  EXPECT_EQ(VpiValue2String(&value), "FEEDCAFE");

  value.format = vpiOctStrVal;
  value.value.str = (PLI_BYTE8 *)"007";
  EXPECT_EQ(VpiValue2String(&value), "007");

  value.format = vpiBinStrVal;
  value.value.str = (PLI_BYTE8 *)"101010";
  EXPECT_EQ(VpiValue2String(&value), "101010");

  value.format = vpiRealVal;
  value.value.real = 3.141592;
  EXPECT_EQ(VpiValue2String(&value), "3.141592");
}

static std::string ParseAndRegenerateString(std::string_view str, int32_t constType) {
  s_vpi_value val;
  String2VpiValue(str, constType, &val);
  const std::string result = VpiValue2String(&val);
  VpiDestroyValue(val);
  return result;
}

static bool ParseConvertBackRoundtrip(std::string_view str, int32_t constType) {
  return ParseAndRegenerateString(str, constType) == str;
}

TEST(VpiValue, ParseValueFindPrefix) {
  EXPECT_EQ(ParseAndRegenerateString("42", vpiIntConst), "42");

  // With Whitespace in front.
  EXPECT_EQ(ParseAndRegenerateString("  42", vpiIntConst), "42");

  // .. or with whitespace at end.
  EXPECT_EQ(ParseAndRegenerateString("42  ", vpiIntConst), "42");

  // .. or at both ends.
  EXPECT_EQ(ParseAndRegenerateString("  42  ", vpiIntConst), "42");
}

TEST(VpiValue, ParseScalarValue) {
  // Zero and one are represented as integers
  EXPECT_EQ(ParseAndRegenerateString("0", vpiScalarConst), "0");
  EXPECT_EQ(ParseAndRegenerateString("1", vpiScalarConst), "1");

  // Symbolic scalar values
  EXPECT_EQ(ParseAndRegenerateString("Z", vpiScalarConst), "Z");
  EXPECT_EQ(ParseAndRegenerateString("X", vpiScalarConst), "X");
  EXPECT_EQ(ParseAndRegenerateString("H", vpiScalarConst), "H");
  EXPECT_EQ(ParseAndRegenerateString("L", vpiScalarConst), "L");
  EXPECT_EQ(ParseAndRegenerateString("W", vpiScalarConst), "DontCare");

  // The longer symbols are case-insensitive
  EXPECT_EQ(ParseAndRegenerateString("DontCare", vpiScalarConst), "DontCare");
  EXPECT_EQ(ParseAndRegenerateString("dontcare", vpiScalarConst), "DontCare");
  EXPECT_EQ(ParseAndRegenerateString("NoChange", vpiScalarConst), "NoChange");
  EXPECT_EQ(ParseAndRegenerateString("nochange", vpiScalarConst), "NoChange");

  // Also parse numeric values
  EXPECT_EQ(ParseAndRegenerateString("2", vpiScalarConst), "Z");
  EXPECT_EQ(ParseAndRegenerateString("3", vpiScalarConst), "X");
  EXPECT_EQ(ParseAndRegenerateString("4", vpiScalarConst), "H");
  EXPECT_EQ(ParseAndRegenerateString("5", vpiScalarConst), "L");
  EXPECT_EQ(ParseAndRegenerateString("6", vpiScalarConst), "DontCare");
  EXPECT_EQ(ParseAndRegenerateString("7", vpiScalarConst), "NoChange");

  // (Q: What is the difference between X and DontCare ?)
  EXPECT_EQ(ParseAndRegenerateString("6", vpiScalarConst), "DontCare");
  EXPECT_EQ(ParseAndRegenerateString("7", vpiScalarConst), "NoChange");
}

// Some smoke testing to see if a string we parse and regenerated is the same
TEST(VpiValue, roundtrip) {
  EXPECT_TRUE(ParseConvertBackRoundtrip("42", vpiIntConst));

  EXPECT_TRUE(ParseConvertBackRoundtrip("1", vpiScalarConst));

  EXPECT_TRUE(ParseConvertBackRoundtrip("X", vpiScalarConst));
  EXPECT_TRUE(ParseConvertBackRoundtrip("Z", vpiScalarConst));
  EXPECT_TRUE(ParseConvertBackRoundtrip("H", vpiScalarConst));
  EXPECT_TRUE(ParseConvertBackRoundtrip("L", vpiScalarConst));

  EXPECT_TRUE(ParseConvertBackRoundtrip("hello", vpiStringConst));
  EXPECT_TRUE(ParseConvertBackRoundtrip("AFFE", vpiHexConst));
  EXPECT_TRUE(ParseConvertBackRoundtrip("0123", vpiOctConst));
  EXPECT_TRUE(ParseConvertBackRoundtrip("11111", vpiBinaryConst));

  EXPECT_TRUE(ParseConvertBackRoundtrip("3.141590", vpiRealConst));
}
