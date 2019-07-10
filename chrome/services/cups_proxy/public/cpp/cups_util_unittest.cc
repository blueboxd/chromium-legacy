// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/cups_util.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cups_proxy {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

const char kPrinterIdPrefix[] = "/printers/";

// Generated via base::GenerateGUID.
const char kDefaultPrinterId[] = "fd4c5f2e-7549-43d5-b931-9bf4e4f1bf51";

TEST(ParseEndpointForPrinterIdTest, SimpleSanityTest) {
  base::Optional<std::string> printer_id = ParseEndpointForPrinterId(
      std::string(kPrinterIdPrefix) + kDefaultPrinterId);

  EXPECT_TRUE(printer_id.has_value());
  EXPECT_THAT(*printer_id, Not(IsEmpty()));
}

// PrinterId's must be non-empty.
TEST(ParseEndpointForPrinterIdTest, EmptyPrinterId) {
  EXPECT_FALSE(ParseEndpointForPrinterId(kPrinterIdPrefix));
}

// Endpoints must contain a '/'.
TEST(ParseEndpointForPrinterIdTest, MissingPathDelimeter) {
  EXPECT_FALSE(ParseEndpointForPrinterId(kDefaultPrinterId));
}

}  // namespace
}  // namespace cups_proxy
