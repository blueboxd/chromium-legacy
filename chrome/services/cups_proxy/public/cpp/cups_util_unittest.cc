// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/cups_util.h"

#include <cups/ipp.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "printing/backend/cups_jobs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cups_proxy {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

const char kEndpointPrefix[] = "/printers/";

// Generated via base::GenerateGUID.
const char kDefaultPrinterId[] = "fd4c5f2e-7549-43d5-b931-9bf4e4f1bf51";

ipp_t* GetPrinterAttributesRequest(
    std::string printer_uri = kDefaultPrinterId) {
  ipp_t* ret = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  if (!ret) {
    return nullptr;
  }

  printer_uri = printing::PrinterUriFromName(printer_uri);
  ippAddString(ret, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", nullptr,
               printer_uri.c_str());
  return ret;
}

TEST(GetPrinterIdTest, SimpleSanityTest) {
  auto printer_id = GetPrinterId(GetPrinterAttributesRequest());
  EXPECT_TRUE(printer_id);
  EXPECT_EQ(*printer_id, kDefaultPrinterId);
}

// PrinterId's must be non-empty.
TEST(GetPrinterIdTest, EmptyPrinterId) {
  EXPECT_FALSE(GetPrinterId(GetPrinterAttributesRequest("")));
}

TEST(GetPrinterIdTest, MissingPrinterUri) {
  ipp_t* ipp = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  EXPECT_FALSE(GetPrinterId(ipp));
}

// Embedded 'printer-uri' attribute must contain a '/'.
TEST(GetPrinterIdTest, MissingPathDelimiter) {
  ipp_t* ret = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  if (!ret) {
    return;
  }

  // Omitting using printing::PrinterUriFromId to correctly embed the uri.
  ippAddString(ret, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", nullptr,
               kDefaultPrinterId);
  EXPECT_FALSE(GetPrinterId(ret));
}

TEST(ParseEndpointForPrinterIdTest, SimpleSanityTest) {
  base::Optional<std::string> printer_id = ParseEndpointForPrinterId(
      std::string(kEndpointPrefix) + kDefaultPrinterId);

  EXPECT_TRUE(printer_id.has_value());
  EXPECT_THAT(*printer_id, Not(IsEmpty()));
}

// PrinterId's must be non-empty.
TEST(ParseEndpointForPrinterIdTest, EmptyPrinterId) {
  EXPECT_FALSE(ParseEndpointForPrinterId(kEndpointPrefix));
}

// Endpoints must contain a '/'.
TEST(ParseEndpointForPrinterIdTest, MissingPathDelimiter) {
  EXPECT_FALSE(ParseEndpointForPrinterId(kDefaultPrinterId));
}

}  // namespace
}  // namespace cups_proxy
