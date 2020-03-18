// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/schema_org/schema_org_entity_names.h"
#include "components/schema_org/schema_org_property_configurations.h"
#include "components/schema_org/schema_org_property_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace schema_org {

TEST(GenerateSchemaOrgTest, EntityName) {
  EXPECT_STREQ(entity::kAboutPage, "AboutPage");
}

TEST(GenerateSchemaOrgTest, IsValidEntityName) {
  EXPECT_TRUE(entity::IsValidEntityName(entity::kAboutPage));
  EXPECT_FALSE(entity::IsValidEntityName("a made up name"));
}

TEST(GenerateSchemaOrgTest, PropertyName) {
  EXPECT_STREQ(property::kAcceptedAnswer, "acceptedAnswer");
}

TEST(GenerateSchemaOrgCodeTest, GetPropertyConfigurationSetsText) {
  EXPECT_TRUE(property::GetPropertyConfiguration(property::kAccessCode).text);
}

TEST(GenerateSchemaOrgCodeTest, GetPropertyConfigurationSetsDate) {
  EXPECT_TRUE(property::GetPropertyConfiguration(property::kBirthDate).date);
}

TEST(GenerateSchemaOrgCodeTest, GetPropertyConfigurationSetsTime) {
  EXPECT_TRUE(property::GetPropertyConfiguration(property::kCloses).time);
}

TEST(GenerateSchemaOrgCodeTest, GetPropertyConfigurationSetsDateTime) {
  EXPECT_TRUE(property::GetPropertyConfiguration(property::kCoverageStartTime)
                  .date_time);
}

TEST(GenerateSchemaOrgCodeTest, GetPropertyConfigurationSetsNumber) {
  EXPECT_TRUE(
      property::GetPropertyConfiguration(property::kDownvoteCount).number);
}

TEST(GenerateSchemaOrgCodeTest, GetPropertyConfigurationSetsUrl) {
  EXPECT_TRUE(property::GetPropertyConfiguration(property::kContentUrl).url);
}

TEST(GenerateSchemaOrgCodeTest, GetPropertyConfigurationSetsThingType) {
  EXPECT_THAT(
      property::GetPropertyConfiguration(property::kAcceptedPaymentMethod)
          .thing_types,
      testing::UnorderedElementsAre("http://schema.org/LoanOrCredit",
                                    "http://schema.org/PaymentMethod"));
}

TEST(GenerateSchemaOrgCodeTest, GetPropertyConfigurationSetsMultipleTypes) {
  EXPECT_TRUE(property::GetPropertyConfiguration(property::kIdentifier).text);
  EXPECT_THAT(
      property::GetPropertyConfiguration(property::kIdentifier).thing_types,
      testing::UnorderedElementsAre("http://schema.org/PropertyValue"));
}

}  // namespace schema_org
