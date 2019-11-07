// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/caspian/function_signature.h"

#include <string>
#include <string_view>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

namespace {
std::tuple<std::string, std::string, std::string> PrettyDebug(
    std::tuple<std::string_view, std::string_view, std::string_view> tuple) {
  return std::make_tuple(std::string(std::get<0>(tuple)),
                         std::string(std::get<1>(tuple)),
                         std::string(std::get<2>(tuple)));
}

TEST(AnalyzeTest, StringSplit) {
  std::string input = "a//b/cd";
  std::vector<std::string_view> expected_output = {"a", "", "b", "cd"};
  EXPECT_EQ(expected_output, caspian::SplitBy(input, '/'));

  input = "a";
  expected_output = {"a"};
  EXPECT_EQ(expected_output, caspian::SplitBy(input, '/'));

  input = "";
  expected_output = {""};
  EXPECT_EQ(expected_output, caspian::SplitBy(input, '/'));

  input = "/";
  expected_output = {"", ""};
  EXPECT_EQ(expected_output, caspian::SplitBy(input, '/'));
}

TEST(AnalyzeTest, ParseJavaFunctionSignature) {
  ::std::deque<std::string> owned_strings;
  // Java method with no args
  auto do_test = [&owned_strings](std::string sig, std::string exp_full_name,
                                  std::string exp_template_name,
                                  std::string exp_name) {
    auto actual = caspian::ParseJava(sig, &owned_strings);
    EXPECT_EQ(exp_full_name, std::string(std::get<0>(actual)));
    EXPECT_EQ(exp_template_name, std::string(std::get<1>(actual)));
    EXPECT_EQ(exp_name, std::string(std::get<2>(actual)));
    // Ensure that ParseJava() is idempotent w.r.t. |full_name| output.
    EXPECT_EQ(PrettyDebug(actual), PrettyDebug(caspian::ParseJava(
                                       std::get<0>(actual), &owned_strings)));
  };
  do_test("org.ClassName java.util.List getCameraInfo()",
          "org.ClassName#getCameraInfo(): java.util.List",
          "org.ClassName#getCameraInfo", "ClassName#getCameraInfo");

  // Java method with args
  do_test("org.ClassName int readShort(int,int)",
          "org.ClassName#readShort(int,int): int", "org.ClassName#readShort",
          "ClassName#readShort");

  // Java <init> method
  do_test("org.ClassName$Inner <init>(byte[])",
          "org.ClassName$Inner#<init>(byte[])", "org.ClassName$Inner#<init>",
          "ClassName$Inner#<init>");

  // Java Class
  do_test("org.ClassName", "org.ClassName", "org.ClassName", "ClassName");

  // Java field
  do_test("org.ClassName some.Type mField", "org.ClassName#mField: some.Type",
          "org.ClassName#mField", "ClassName#mField");
}
}  // namespace
