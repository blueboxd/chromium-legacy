// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/match_result.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

using css_test_helpers::ParseDeclarationBlock;

class MatchResultTest : public PageTestBase {
 protected:
  void SetUp() override;

  const CSSPropertyValueSet* PropertySet(unsigned index) const {
    return property_sets->at(index).Get();
  }

 private:
  Persistent<HeapVector<Member<MutableCSSPropertyValueSet>, 8>> property_sets;
};

void MatchResultTest::SetUp() {
  PageTestBase::SetUp();
  property_sets =
      MakeGarbageCollected<HeapVector<Member<MutableCSSPropertyValueSet>, 8>>();
  for (unsigned i = 0; i < 8; i++) {
    property_sets->push_back(
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode));
  }
}

void TestMatchedPropertiesRange(const MatchedPropertiesRange& range,
                                int expected_length,
                                const CSSPropertyValueSet** expected_sets) {
  EXPECT_EQ(expected_length, range.end() - range.begin());
  for (const auto& matched_properties : range)
    EXPECT_EQ(*expected_sets++, matched_properties.properties);
}

void TestOriginInRange(const MatchedPropertiesRange& range,
                       int expected_length,
                       CascadeOrigin expected_origin) {
  EXPECT_EQ(expected_length, range.end() - range.begin());
  for (const auto& matched_properties : range)
    EXPECT_EQ(matched_properties.types_.origin, expected_origin);
}

TEST_F(MatchResultTest, UARules) {
  const CSSPropertyValueSet* ua_sets[] = {PropertySet(0), PropertySet(1)};

  MatchResult result;
  result.AddMatchedProperties(ua_sets[0]);
  result.AddMatchedProperties(ua_sets[1]);
  result.FinishAddingUARules();
  result.FinishAddingUserRules();

  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.UaRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.UserRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.AuthorRules(), 0, nullptr);

  ImportantAuthorRanges importantAuthor(result);
  EXPECT_EQ(importantAuthor.end(), importantAuthor.begin());
  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), importantUser.begin());
}

TEST_F(MatchResultTest, UserRules) {
  const CSSPropertyValueSet* user_sets[] = {PropertySet(0), PropertySet(1)};

  MatchResult result;

  result.FinishAddingUARules();
  result.AddMatchedProperties(user_sets[0]);
  result.AddMatchedProperties(user_sets[1]);
  result.FinishAddingUserRules();
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 2, user_sets);
  TestMatchedPropertiesRange(result.UaRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.UserRules(), 2, user_sets);
  TestMatchedPropertiesRange(result.AuthorRules(), 0, nullptr);

  ImportantAuthorRanges importantAuthor(result);
  EXPECT_EQ(importantAuthor.end(), importantAuthor.begin());
  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), ++importantUser.begin());
}

TEST_F(MatchResultTest, AuthorRules) {
  const CSSPropertyValueSet* author_sets[] = {PropertySet(0), PropertySet(1)};

  MatchResult result;

  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 2, author_sets);
  TestMatchedPropertiesRange(result.UaRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.UserRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.AuthorRules(), 2, author_sets);

  ImportantAuthorRanges importantAuthor(result);
  EXPECT_EQ(importantAuthor.end(), ++importantAuthor.begin());
  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), importantUser.begin());
}

TEST_F(MatchResultTest, AllRules) {
  const CSSPropertyValueSet* all_sets[] = {PropertySet(0), PropertySet(1),
                                           PropertySet(2), PropertySet(3),
                                           PropertySet(4), PropertySet(5)};
  const CSSPropertyValueSet** ua_sets = &all_sets[0];
  const CSSPropertyValueSet** user_sets = &all_sets[2];
  const CSSPropertyValueSet** author_sets = &all_sets[4];

  MatchResult result;

  result.AddMatchedProperties(ua_sets[0]);
  result.AddMatchedProperties(ua_sets[1]);
  result.FinishAddingUARules();

  result.AddMatchedProperties(user_sets[0]);
  result.AddMatchedProperties(user_sets[1]);
  result.FinishAddingUserRules();

  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 6, all_sets);
  TestMatchedPropertiesRange(result.UaRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.UserRules(), 2, user_sets);
  TestMatchedPropertiesRange(result.AuthorRules(), 2, author_sets);

  ImportantAuthorRanges importantAuthor(result);
  EXPECT_EQ(importantAuthor.end(), ++importantAuthor.begin());
  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), ++importantUser.begin());
}

TEST_F(MatchResultTest, AuthorRulesMultipleScopes) {
  const CSSPropertyValueSet* author_sets[] = {PropertySet(0), PropertySet(1),
                                              PropertySet(2), PropertySet(3)};

  MatchResult result;

  result.FinishAddingUARules();
  result.FinishAddingUserRules();

  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  result.AddMatchedProperties(author_sets[2]);
  result.AddMatchedProperties(author_sets[3]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 4, author_sets);
  TestMatchedPropertiesRange(result.UaRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.UserRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.AuthorRules(), 4, author_sets);

  ImportantAuthorRanges importantAuthor(result);

  auto iter = importantAuthor.begin();
  EXPECT_NE(importantAuthor.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, &author_sets[2]);

  ++iter;
  EXPECT_NE(importantAuthor.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, author_sets);

  ++iter;
  EXPECT_EQ(importantAuthor.end(), iter);

  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), importantUser.begin());
}

TEST_F(MatchResultTest, AllRulesMultipleScopes) {
  const CSSPropertyValueSet* all_sets[] = {
      PropertySet(0), PropertySet(1), PropertySet(2), PropertySet(3),
      PropertySet(4), PropertySet(5), PropertySet(6), PropertySet(7)};
  const CSSPropertyValueSet** ua_sets = &all_sets[0];
  const CSSPropertyValueSet** user_sets = &all_sets[2];
  const CSSPropertyValueSet** author_sets = &all_sets[4];

  MatchResult result;

  result.AddMatchedProperties(ua_sets[0]);
  result.AddMatchedProperties(ua_sets[1]);
  result.FinishAddingUARules();

  result.AddMatchedProperties(user_sets[0]);
  result.AddMatchedProperties(user_sets[1]);
  result.FinishAddingUserRules();

  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  result.AddMatchedProperties(author_sets[2]);
  result.AddMatchedProperties(author_sets[3]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 8, all_sets);
  TestMatchedPropertiesRange(result.UaRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.UserRules(), 2, user_sets);
  TestMatchedPropertiesRange(result.AuthorRules(), 4, author_sets);

  ImportantAuthorRanges importantAuthor(result);

  ImportantAuthorRangeIterator iter = importantAuthor.begin();
  EXPECT_NE(importantAuthor.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, &author_sets[2]);

  ++iter;
  EXPECT_NE(importantAuthor.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, author_sets);

  ++iter;
  EXPECT_EQ(importantAuthor.end(), iter);

  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), ++importantUser.begin());
}

TEST_F(MatchResultTest, CascadeOriginUserAgent) {
  MatchResult result;
  result.AddMatchedProperties(PropertySet(0));
  result.AddMatchedProperties(PropertySet(1));
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.UaRules(), 2, CascadeOrigin::kUserAgent);
  TestOriginInRange(result.AllRules(), 2, CascadeOrigin::kUserAgent);
}

TEST_F(MatchResultTest, CascadeOriginUser) {
  MatchResult result;
  result.FinishAddingUARules();
  result.AddMatchedProperties(PropertySet(0));
  result.AddMatchedProperties(PropertySet(1));
  result.FinishAddingUserRules();
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.UserRules(), 2, CascadeOrigin::kUser);
  TestOriginInRange(result.AllRules(), 2, CascadeOrigin::kUser);
}

TEST_F(MatchResultTest, CascadeOriginAuthor) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(PropertySet(0));
  result.AddMatchedProperties(PropertySet(1));
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.AuthorRules(), 2, CascadeOrigin::kAuthor);
  TestOriginInRange(result.AllRules(), 2, CascadeOrigin::kAuthor);
}

TEST_F(MatchResultTest, CascadeOriginAll) {
  MatchResult result;
  result.AddMatchedProperties(PropertySet(0));
  result.FinishAddingUARules();
  result.AddMatchedProperties(PropertySet(1));
  result.AddMatchedProperties(PropertySet(2));
  result.FinishAddingUserRules();
  result.AddMatchedProperties(PropertySet(3));
  result.AddMatchedProperties(PropertySet(4));
  result.AddMatchedProperties(PropertySet(5));
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.UaRules(), 1, CascadeOrigin::kUserAgent);
  TestOriginInRange(result.UserRules(), 2, CascadeOrigin::kUser);
  TestOriginInRange(result.AuthorRules(), 3, CascadeOrigin::kAuthor);
}

TEST_F(MatchResultTest, CascadeOriginAllExceptUserAgent) {
  MatchResult result;
  result.FinishAddingUARules();
  result.AddMatchedProperties(PropertySet(1));
  result.AddMatchedProperties(PropertySet(2));
  result.FinishAddingUserRules();
  result.AddMatchedProperties(PropertySet(3));
  result.AddMatchedProperties(PropertySet(4));
  result.AddMatchedProperties(PropertySet(5));
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.UaRules(), 0, CascadeOrigin::kUserAgent);
  TestOriginInRange(result.UserRules(), 2, CascadeOrigin::kUser);
  TestOriginInRange(result.AuthorRules(), 3, CascadeOrigin::kAuthor);
}

TEST_F(MatchResultTest, CascadeOriginAllExceptUser) {
  MatchResult result;
  result.AddMatchedProperties(PropertySet(0));
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(PropertySet(3));
  result.AddMatchedProperties(PropertySet(4));
  result.AddMatchedProperties(PropertySet(5));
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.UaRules(), 1, CascadeOrigin::kUserAgent);
  TestOriginInRange(result.UserRules(), 0, CascadeOrigin::kUser);
  TestOriginInRange(result.AuthorRules(), 3, CascadeOrigin::kAuthor);
}

TEST_F(MatchResultTest, CascadeOriginAllExceptAuthor) {
  MatchResult result;
  result.AddMatchedProperties(PropertySet(0));
  result.FinishAddingUARules();
  result.AddMatchedProperties(PropertySet(1));
  result.AddMatchedProperties(PropertySet(2));
  result.FinishAddingUserRules();
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.UaRules(), 1, CascadeOrigin::kUserAgent);
  TestOriginInRange(result.UserRules(), 2, CascadeOrigin::kUser);
  TestOriginInRange(result.AuthorRules(), 0, CascadeOrigin::kAuthor);
}

TEST_F(MatchResultTest, CascadeOriginTreeScopes) {
  MatchResult result;
  result.AddMatchedProperties(PropertySet(0));
  result.FinishAddingUARules();
  result.AddMatchedProperties(PropertySet(1));
  result.FinishAddingUserRules();
  result.AddMatchedProperties(PropertySet(2));
  result.FinishAddingAuthorRulesForTreeScope();
  result.AddMatchedProperties(PropertySet(3));
  result.AddMatchedProperties(PropertySet(4));
  result.FinishAddingAuthorRulesForTreeScope();
  result.AddMatchedProperties(PropertySet(5));
  result.AddMatchedProperties(PropertySet(6));
  result.AddMatchedProperties(PropertySet(7));
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.UaRules(), 1, CascadeOrigin::kUserAgent);
  TestOriginInRange(result.UserRules(), 1, CascadeOrigin::kUser);
  TestOriginInRange(result.AuthorRules(), 6, CascadeOrigin::kAuthor);
}

TEST_F(MatchResultTest, ExpansionsRange) {
  MatchResult result;
  result.AddMatchedProperties(ParseDeclarationBlock("left:1px;all:unset"));
  result.AddMatchedProperties(ParseDeclarationBlock("color:red"));
  result.FinishAddingUARules();
  result.AddMatchedProperties(ParseDeclarationBlock("display:block"));
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("left:unset"));
  result.AddMatchedProperties(ParseDeclarationBlock("top:unset"));
  result.AddMatchedProperties(
      ParseDeclarationBlock("right:unset;bottom:unset"));
  result.FinishAddingAuthorRulesForTreeScope();

  CascadeFilter filter;

  size_t i = 0;
  size_t size = result.GetMatchedProperties().size();
  for (auto actual : result.Expansions(GetDocument(), filter)) {
    ASSERT_LT(i, size);
    CascadeExpansion expected(result.GetMatchedProperties()[i], GetDocument(),
                              filter, i);
    EXPECT_EQ(expected.Id(), actual.Id());
    EXPECT_EQ(expected.Priority(), actual.Priority());
    EXPECT_EQ(expected.Value(), actual.Value());
    ++i;
  }

  EXPECT_EQ(6u, i);
}

TEST_F(MatchResultTest, EmptyExpansionsRange) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.FinishAddingAuthorRulesForTreeScope();

  CascadeFilter filter;
  auto range = result.Expansions(GetDocument(), filter);
  EXPECT_EQ(range.end(), range.begin());
}

TEST_F(MatchResultTest, Reset) {
  MatchResult result;
  result.AddMatchedProperties(PropertySet(0));
  result.FinishAddingUARules();
  result.AddMatchedProperties(PropertySet(1));
  result.FinishAddingUserRules();
  result.AddMatchedProperties(PropertySet(2));
  result.FinishAddingAuthorRulesForTreeScope();
  result.AddMatchedProperties(PropertySet(3));
  result.FinishAddingAuthorRulesForTreeScope();
  result.AddMatchedProperties(PropertySet(4));
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.UaRules(), 1, CascadeOrigin::kUserAgent);
  TestOriginInRange(result.UserRules(), 1, CascadeOrigin::kUser);
  TestOriginInRange(result.AuthorRules(), 3, CascadeOrigin::kAuthor);

  // Check tree_order of last entry.
  EXPECT_TRUE(result.HasMatchedProperties());
  ASSERT_EQ(5u, result.GetMatchedProperties().size());
  EXPECT_EQ(2u, result.GetMatchedProperties()[4].types_.tree_order);

  EXPECT_TRUE(result.IsCacheable());
  result.SetIsCacheable(false);
  EXPECT_FALSE(result.IsCacheable());

  result.Reset();

  EXPECT_TRUE(result.UaRules().IsEmpty());
  EXPECT_TRUE(result.UserRules().IsEmpty());
  EXPECT_TRUE(result.AuthorRules().IsEmpty());
  EXPECT_TRUE(result.AllRules().IsEmpty());
  EXPECT_TRUE(result.IsCacheable());
  EXPECT_FALSE(result.GetMatchedProperties().size());
  EXPECT_FALSE(result.HasMatchedProperties());

  // Add same declarations again.
  result.AddMatchedProperties(PropertySet(0));
  result.FinishAddingUARules();
  result.AddMatchedProperties(PropertySet(1));
  result.FinishAddingUserRules();
  result.AddMatchedProperties(PropertySet(2));
  result.FinishAddingAuthorRulesForTreeScope();
  result.AddMatchedProperties(PropertySet(3));
  result.FinishAddingAuthorRulesForTreeScope();
  result.AddMatchedProperties(PropertySet(4));
  result.FinishAddingAuthorRulesForTreeScope();

  TestOriginInRange(result.UaRules(), 1, CascadeOrigin::kUserAgent);
  TestOriginInRange(result.UserRules(), 1, CascadeOrigin::kUser);
  TestOriginInRange(result.AuthorRules(), 3, CascadeOrigin::kAuthor);

  // Check tree_order of last entry.
  EXPECT_TRUE(result.HasMatchedProperties());
  ASSERT_EQ(5u, result.GetMatchedProperties().size());
  EXPECT_EQ(2u, result.GetMatchedProperties()[4].types_.tree_order);

  EXPECT_TRUE(result.IsCacheable());
}

}  // namespace blink
