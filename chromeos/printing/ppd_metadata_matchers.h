// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains matchers useful for testing with parsed PPD
// metadata.

#include <string>

#include "testing/gmock/include/gmock/gmock-matchers.h"

#ifndef CHROMEOS_PRINTING_PPD_METADATA_MATCHERS_H_
#define CHROMEOS_PRINTING_PPD_METADATA_MATCHERS_H_

namespace chromeos {

using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::StrEq;

// Matches a ReverseIndexLeaf struct against its |manufacturer| and
// |model| members.
MATCHER_P2(ReverseIndexLeafLike,
           manufacturer,
           model,
           "is a ReverseIndexLeaf with manufacturer ``" +
               std::string(manufacturer) + "'' and model ``" +
               std::string(model) + "''") {
  return ExplainMatchResult(
             Field(&ReverseIndexLeaf::manufacturer, StrEq(manufacturer)), arg,
             result_listener) &&
         ExplainMatchResult(Field(&ReverseIndexLeaf::model, StrEq(model)), arg,
                            result_listener);
}

// Matches a ParsedPrinter struct against its
// |user_visible_printer_name| and |effective_make_and_model| members.
MATCHER_P2(ParsedPrinterLike,
           name,
           emm,
           "is a ParsedPrinter with user_visible_printer_name``" +
               std::string(name) + "'' and effective_make_and_model ``" +
               std::string(emm) + "''") {
  return ExplainMatchResult(
             Field(&ParsedPrinter::user_visible_printer_name, StrEq(name)), arg,
             result_listener) &&
         ExplainMatchResult(
             Field(&ParsedPrinter::effective_make_and_model, StrEq(emm)), arg,
             result_listener);
}

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_METADATA_MATCHERS_H_
