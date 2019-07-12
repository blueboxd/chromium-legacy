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

TEST(AutofillInternalsBuffer, EscapeStrings) {
  AutofillInternalsBuffer buffer;
  buffer << "<foo><!--\"";
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"type":"text","value":"&lt;foo&gt;&lt;!--&quot;"})", json);
}

TEST(AutofillInternalsBuffer, EscapeUtf16Strings) {
  AutofillInternalsBuffer buffer;
  buffer << base::ASCIIToUTF16("<foo><!--\"");
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"type":"text","value":"&lt;foo&gt;&lt;!--&quot;"})", json);
}

TEST(AutofillInternalsBuffer, SupportNumbers) {
  AutofillInternalsBuffer buffer;
  buffer << 42;
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"type":"text","value":"42"})", json);
}

TEST(AutofillInternalsBuffer, SanitizeURLs) {
  AutofillInternalsBuffer buffer;
  buffer << GURL("https://user:pw@www.example.com:80/foo?bar=1#foo");
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  // Verify that the url gets scrubbed.
  EXPECT_EQ(R"({"type":"text","value":"https://www.example.com:80/"})", json);
}

TEST(AutofillInternalsBuffer, Empty) {
  AutofillInternalsBuffer buffer;
  EXPECT_EQ(base::Value(), buffer.RetrieveResult());
}

TEST(AutofillInternalsBuffer, UnclosedTag) {
  AutofillInternalsBuffer buffer;
  buffer << Tag{"foo"};
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"type":"node","value":"foo"})", json);
}

TEST(AutofillInternalsBuffer, ClosedTag) {
  AutofillInternalsBuffer buffer;
  buffer << Tag{"foo"} << CTag{};
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"type":"node","value":"foo"})", json);
}

TEST(AutofillInternalsBuffer, NestedTag) {
  AutofillInternalsBuffer buffer;
  buffer << Tag{"foo"} << Tag{"bar"} << CTag{} << CTag{};
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"children":[{"type":"node","value":"bar"}],)"
            R"("type":"node","value":"foo"})",
            json);
}

TEST(AutofillInternalsBuffer, NestedTagClosingTooOften) {
  AutofillInternalsBuffer buffer;
  buffer << Tag{"foo"} << Tag{"bar"} << CTag{} << CTag{} << CTag{};
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"children":[{"type":"node","value":"bar"}],)"
            R"("type":"node","value":"foo"})",
            json);
}

TEST(AutofillInternalsBuffer, NestedTagClosingNotAtAll) {
  AutofillInternalsBuffer buffer;
  buffer << Tag{"foo"} << Tag{"bar"};
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"children":[{"type":"node","value":"bar"}],)"
            R"("type":"node","value":"foo"})",
            json);
}

TEST(AutofillInternalsBuffer, NestedTagWithAttributes) {
  AutofillInternalsBuffer buffer;
  buffer << Tag{"foo"} << Tag{"bar"} << Attrib{"b1", "1"} << Attrib{"b2", "2"}
         << CTag{} << Attrib{"f1", "1"};
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"attributes":{"f1":"1"},"children":[)"
            R"({"attributes":{"b1":"1","b2":"2"},"type":"node","value":"bar"})"
            R"(],"type":"node","value":"foo"})",
            json);
}

TEST(AutofillInternalsBuffer, DivWithBr) {
  AutofillInternalsBuffer buffer;
  buffer << Tag{"div"} << "foo" << Br{} << "bar" << CTag{};
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"children":[{"type":"text","value":"foo"},)"
            R"({"type":"node","value":"br"},{"type":"text","value":"bar"}],)"
            R"("type":"node","value":"div"})",
            json);
}

TEST(AutofillInternalsBuffer, Scope) {
  AutofillInternalsBuffer buffer;
  buffer << LoggingScope::kContext;
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"attributes":{"class":"log-entry","scope":"Context"},)"
            R"("type":"node","value":"div"})",
            json);
}

TEST(AutofillInternalsBuffer, Message) {
  AutofillInternalsBuffer buffer;
  buffer << LogMessage::kParsedForms;
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(R"({"attributes":{"class":"log-message","message":"ParsedForms"},)"
            R"("children":[{"type":"text","value":"Parsed forms:"}],)"
            R"("type":"node","value":"div"})",
            json);
}

struct SampleObject {
  int x;
  std::string y;
};

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    const SampleObject& value) {
  buf << Tag{"table"};
  buf << Tag{"tr"} << Tag{"td"} << "x" << CTag{"td"} << Tag{"td"} << value.x
      << CTag{"td"} << CTag{"tr"};
  buf << Tag{"tr"} << Tag{"td"} << "y" << CTag{"td"} << Tag{"td"} << value.y
      << CTag{"td"} << CTag{"tr"};
  buf << CTag{"table"};
  return buf;
}

TEST(AutofillInternalsBuffer, CanStreamCustomObjects) {
  AutofillInternalsBuffer buffer;
  SampleObject o{42, "foobar<!--"};
  buffer << o;
  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(buffer.RetrieveResult(), &json));
  EXPECT_EQ(
      R"({"children":[)"                                      // table
      /**/ R"({"children":[)"                                 // tr
      /****/ R"({"children":[{"type":"text","value":"x"}],)"  // td
      /******/ R"("type":"node","value":"td"},)"
      /****/ R"({"children":[{"type":"text","value":"42"}],)"  // td
      /******/ R"("type":"node","value":"td"}],)"
      /****/ R"("type":"node","value":"tr"},)"  // continuation of tr
      /**/ R"({"children":[)"                   // tr
      /****/ R"({"children":[{"type":"text","value":"y"}],)"
      /******/ R"("type":"node","value":"td"},)"
      /****/ R"({"children":[{"type":"text","value":"foobar&lt;!--"}],)"
      /******/ R"("type":"node","value":"td"}],)"
      /**/ R"("type":"node","value":"tr"}],"type":"node","value":"table"})",
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
  AutofillInternalsBuffer buffer;
  buffer << LoggingScope::kContext;
  base::Value expected = buffer.RetrieveResult();

  MockAutofillInternalsLogging logging_internals;
  EXPECT_CALL(logging_internals, LogRaw(testing::Eq(testing::ByRef(expected))));
  AutofillInternalsLogging::GetInstance()->AddObserver(&logging_internals);
  LOG_AF_INTERNALS << LoggingScope::kContext;
  AutofillInternalsLogging::GetInstance()->RemoveObserver(&logging_internals);
}

}  // namespace autofill
