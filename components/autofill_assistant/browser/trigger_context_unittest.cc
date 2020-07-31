// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_context.h"

#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

TEST(TriggerContextTest, Empty) {
  auto empty = TriggerContext::CreateEmpty();
  ASSERT_TRUE(empty);

  EXPECT_THAT(empty->GetParameters(), IsEmpty());
  EXPECT_EQ("", empty->experiment_ids());
}

TEST(TriggerContextTest, Create) {
  std::map<std::string, std::string> parameters = {{"key_a", "value_a"},
                                                   {"key_b", "value_b"}};
  auto context = TriggerContext::Create(parameters, "exps");
  ASSERT_TRUE(context);

  EXPECT_THAT(
      context->GetParameters(),
      UnorderedElementsAre(Pair("key_a", "value_a"), Pair("key_b", "value_b")));

  EXPECT_THAT(context->GetParameter("key_a"), Eq("value_a"));
  EXPECT_THAT(context->GetParameter("key_b"), Eq("value_b"));
  EXPECT_THAT(context->GetParameter("not_found"), Eq(base::nullopt));

  EXPECT_EQ("exps", context->experiment_ids());
}

TEST(TriggerContextTest, Merge) {
  auto context1 = TriggerContext::Create({{"key_a", "value_a"}}, "exp1");
  auto context2 = TriggerContext::Create({{"key_b", "value_b"}}, "exp2");

  // Adding empty to make sure empty contexts are properly skipped
  auto empty = TriggerContext::CreateEmpty();

  auto merged = TriggerContext::Merge(
      {empty.get(), context1.get(), context2.get(), empty.get()});

  ASSERT_TRUE(merged);

  EXPECT_THAT(
      merged->GetParameters(),
      UnorderedElementsAre(Pair("key_a", "value_a"), Pair("key_b", "value_b")));

  EXPECT_THAT(merged->GetParameter("key_a"), Eq("value_a"));
  EXPECT_THAT(merged->GetParameter("key_b"), Eq("value_b"));
  EXPECT_THAT(merged->GetParameter("not_found"), Eq(base::nullopt));

  EXPECT_EQ(merged->experiment_ids(), "exp1,exp2");
}

TEST(TriggerContextTest, CCT) {
  TriggerContextImpl context;

  EXPECT_FALSE(context.is_cct());
  context.SetCCT(true);
  EXPECT_TRUE(context.is_cct());
}

TEST(TriggerContextTest, MergeCCT) {
  auto empty = TriggerContext::CreateEmpty();

  auto all_empty = TriggerContext::Merge({empty.get(), empty.get()});
  EXPECT_FALSE(all_empty->is_cct());

  TriggerContextImpl cct_context;
  cct_context.SetCCT(true);
  auto one_with_cct =
      TriggerContext::Merge({empty.get(), &cct_context, empty.get()});

  EXPECT_TRUE(one_with_cct->is_cct());
}

TEST(TriggerContextTest, OnboardingShown) {
  TriggerContextImpl context;

  EXPECT_FALSE(context.is_onboarding_shown());
  context.SetOnboardingShown(true);
  EXPECT_TRUE(context.is_onboarding_shown());
}

TEST(TriggerContextTest, MergeOnboardingShown) {
  auto empty = TriggerContext::CreateEmpty();

  auto all_empty = TriggerContext::Merge({empty.get(), empty.get()});
  EXPECT_FALSE(all_empty->is_onboarding_shown());

  TriggerContextImpl onboarding_context;
  onboarding_context.SetOnboardingShown(true);
  auto one_with_onboarding =
      TriggerContext::Merge({empty.get(), &onboarding_context, empty.get()});

  EXPECT_TRUE(one_with_onboarding->is_onboarding_shown());
}

TEST(TriggerContextTest, DirectAction) {
  TriggerContextImpl context;

  EXPECT_FALSE(context.is_direct_action());
  context.SetDirectAction(true);
  EXPECT_TRUE(context.is_direct_action());
}

TEST(TriggerContextTest, MergeDirectAction) {
  auto empty = TriggerContext::CreateEmpty();

  auto all_empty = TriggerContext::Merge({empty.get(), empty.get()});
  EXPECT_FALSE(all_empty->is_direct_action());

  TriggerContextImpl direct_action_context;
  direct_action_context.SetDirectAction(true);
  auto one_direct_action =
      TriggerContext::Merge({empty.get(), &direct_action_context, empty.get()});

  EXPECT_TRUE(one_direct_action->is_direct_action());
}

TEST(TriggerContextTest, MergeAccountsMatchingStatusTest) {
  auto empty = TriggerContext::CreateEmpty();

  auto all_empty = TriggerContext::Merge({empty.get(), empty.get()});
  EXPECT_EQ(all_empty->get_caller_account_hash(), "");

  TriggerContextImpl context_matching;
  context_matching.SetCallerAccountHash("accountsha");
  auto one_with_accounts_matching =
      TriggerContext::Merge({empty.get(), &context_matching, empty.get()});

  EXPECT_EQ(one_with_accounts_matching->get_caller_account_hash(),
            "accountsha");
}

TEST(TriggerContextTest, HasExperimentId) {
  auto context = TriggerContext::Create({}, "1,2,3");
  ASSERT_TRUE(context);

  EXPECT_TRUE(context->HasExperimentId("2"));
  EXPECT_FALSE(context->HasExperimentId("4"));

  auto other_context = TriggerContext::Create({}, "4,5,6");
  ASSERT_TRUE(other_context);

  EXPECT_TRUE(other_context->HasExperimentId("4"));
  EXPECT_FALSE(other_context->HasExperimentId("2"));

  auto merged = TriggerContext::Merge({context.get(), other_context.get()});
  ASSERT_TRUE(merged);

  EXPECT_TRUE(merged->HasExperimentId("2"));
  EXPECT_TRUE(merged->HasExperimentId("4"));
  EXPECT_FALSE(merged->HasExperimentId("7"));

  // Double commas should not allow empty element to match.
  auto double_comma = TriggerContext::Create({}, "1,,2");
  EXPECT_TRUE(double_comma->HasExperimentId("2"));
  EXPECT_FALSE(double_comma->HasExperimentId(""));

  // Empty context should not aloow empty element to match.
  auto empty = TriggerContext::Create({}, "");
  EXPECT_FALSE(empty->HasExperimentId(""));

  // Lone comma does not create empty elements.
  auto lone_comma = TriggerContext::Create({}, ",");
  EXPECT_FALSE(lone_comma->HasExperimentId(""));

  // Single element should match.
  auto single_element = TriggerContext::Create({}, "1");
  EXPECT_TRUE(single_element->HasExperimentId("1"));
}

}  // namespace
}  // namespace autofill_assistant
