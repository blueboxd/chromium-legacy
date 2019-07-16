// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_internals_logging.h"

#include "base/json/json_writer.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(AutofillInternalsLogging, Scope) {
  LogBuffer buffer;
  buffer << LoggingScope::kContext;
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"attributes":{"class":"log-entry","scope":"Context"},)"
            R"("type":"node","value":"div"})",
            json);
}

TEST(AutofillInternalsLogging, Message) {
  LogBuffer buffer;
  buffer << LogMessage::kParsedForms;
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"attributes":{"class":"log-message","message":"ParsedForms"},)"
            R"("children":[{"type":"text","value":"Parsed forms:"}],)"
            R"("type":"node","value":"div"})",
            json);
}

class MockAutofillInternalsLogging : public AutofillInternalsLogging::Observer {
 public:
  MOCK_METHOD1(Log, void(const base::Value&));
  MOCK_METHOD1(LogRaw, void(const base::Value&));
};

// Don't add further tests to this. AutofillInternalsLogging uses a global
// instance and if more tests are executed in parallel, these can interfere.
TEST(AutofillInternalsMessage, VerifySubmissionOnDestruction) {
  LogBuffer buffer;
  buffer << LoggingScope::kContext;
  base::Value expected = buffer.RetrieveResult();

  MockAutofillInternalsLogging logging_internals;
  EXPECT_CALL(logging_internals, LogRaw(testing::Eq(testing::ByRef(expected))));
  AutofillInternalsLogging::GetInstance()->AddObserver(&logging_internals);
  LOG_AF_INTERNALS << LoggingScope::kContext;
  AutofillInternalsLogging::GetInstance()->RemoveObserver(&logging_internals);
}

}  // namespace autofill
