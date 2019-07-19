// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_internals_service.h"

#include "base/json/json_writer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(AutofillInternalsService, Scope) {
  LogBuffer buffer;
  buffer << LoggingScope::kContext;
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"attributes":{"class":"log-entry","scope":"Context"},)"
            R"("type":"node","value":"div"})",
            json);
}

TEST(AutofillInternalsService, Message) {
  LogBuffer buffer;
  buffer << LogMessage::kParsedForms;
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"attributes":{"class":"log-message","message":"ParsedForms"},)"
            R"("children":[{"type":"text","value":"Parsed forms:"}],)"
            R"("type":"node","value":"div"})",
            json);
}

}  // namespace autofill
