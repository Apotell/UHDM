// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <stdlib.h>

#include "gtest/gtest.h"
#include "uhdm/constant.h"
#include "uhdm/uhdm.h"
#include "uhdm/uhdm_types.h"  // for uhdmconstant
#include "uhdm/vhpi_user.h"   // vpi_user functions.
#include "uhdm/vpi_uhdm.h"    // struct uhdm_handle

TEST(VpiGetTest, WriteReadRoundtrip) {
  uhdm::Serializer serializer;

  // Let's choose a type that is multiple levels deep to see levels
  // base-class <- expr <- Constant

  uhdm::Constant *value = serializer.make<uhdm::Constant>();
  // Base class values
  EXPECT_TRUE(value->setFile("hello.v"));
  EXPECT_TRUE(value->setStartLine(42));

  // expr values
  EXPECT_TRUE(value->setSize(12345));
  EXPECT_TRUE(value->setDecompile("decompile"));

  uhdm_handle uhdm_handle(uhdm::UhdmType::Constant, value);
  vpiHandle vpi_handle = (vpiHandle)&uhdm_handle;

  // Request all the properties set above via vpi
  EXPECT_EQ(vpi_get_str(vpiFile, vpi_handle), std::string("hello.v"));
  EXPECT_EQ(vpi_get(vpiLineNo, vpi_handle), 42);

  EXPECT_EQ(vpi_get(vpiSize, vpi_handle), 12345);
  EXPECT_EQ(vpi_get_str(vpiDecompile, vpi_handle), std::string("decompile"));
}
