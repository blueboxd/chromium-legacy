// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_processing_util.h"

#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {

class HintsProcessingUtilTest : public testing::Test {
 public:
  HintsProcessingUtilTest() {}
  ~HintsProcessingUtilTest() override {}
};

TEST_F(HintsProcessingUtilTest, FindPageHintForSubstringPagePattern) {
  proto::Hint hint1;

  // Page hint for "/one/"
  proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("foo.org/*/one/");

  // Page hint for "two"
  proto::PageHint* page_hint2 = hint1.add_page_hints();
  page_hint2->set_page_pattern("two");

  // Page hint for "three.jpg"
  proto::PageHint* page_hint3 = hint1.add_page_hints();
  page_hint3->set_page_pattern("three.jpg");

  EXPECT_EQ(nullptr, FindPageHintForURL(GURL(""), &hint1));
  EXPECT_EQ(nullptr, FindPageHintForURL(GURL("https://www.foo.org/"), &hint1));
  EXPECT_EQ(nullptr,
            FindPageHintForURL(GURL("https://www.foo.org/one"), &hint1));

  EXPECT_EQ(nullptr,
            FindPageHintForURL(GURL("https://www.foo.org/one/"), &hint1));
  EXPECT_EQ(page_hint1,
            FindPageHintForURL(GURL("https://www.foo.org/pages/one/"), &hint1));
  EXPECT_EQ(page_hint1,
            FindPageHintForURL(GURL("https://www.foo.org/pages/subpages/one/"),
                               &hint1));
  EXPECT_EQ(page_hint1, FindPageHintForURL(
                            GURL("https://www.foo.org/pages/one/two"), &hint1));
  EXPECT_EQ(page_hint1,
            FindPageHintForURL(
                GURL("https://www.foo.org/pages/one/two/three.jpg"), &hint1));

  EXPECT_EQ(page_hint2,
            FindPageHintForURL(
                GURL("https://www.foo.org/pages/onetwo/three.jpg"), &hint1));
  EXPECT_EQ(page_hint2,
            FindPageHintForURL(GURL("https://www.foo.org/one/two/three.jpg"),
                               &hint1));
  EXPECT_EQ(page_hint2,
            FindPageHintForURL(GURL("https://one.two.org"), &hint1));

  EXPECT_EQ(page_hint3, FindPageHintForURL(
                            GURL("https://www.foo.org/bar/three.jpg"), &hint1));
}

}  // namespace optimization_guide
