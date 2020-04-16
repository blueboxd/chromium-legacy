// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "components/schema_org/schema_org_enums.h"
#include "components/schema_org/schema_org_property_configurations.h"
#include "components/schema_org/schema_org_property_names.h"
#include "components/schema_org/validator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace schema_org {

using improved::mojom::Entity;
using improved::mojom::EntityPtr;
using improved::mojom::Property;
using improved::mojom::PropertyPtr;
using improved::mojom::Values;

class SchemaOrgValidatorTest : public testing::Test {};

TEST_F(SchemaOrgValidatorTest, InvalidEntityType) {
  EntityPtr entity = Entity::New();
  entity->type = "random entity type";

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_FALSE(validated_entity);
}

TEST_F(SchemaOrgValidatorTest, ValidStringPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kAboutPage;

  PropertyPtr property = Property::New();
  property->name = property::kAccessMode;
  property->values = Values::New();
  property->values->string_values.push_back("foo");

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

TEST_F(SchemaOrgValidatorTest, AllowsStringPropertyValueForThingType) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kCreativeWork;

  PropertyPtr property = Property::New();
  property->name = property::kAuthor;
  property->values = Values::New();
  property->values->string_values.push_back("foo");

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

TEST_F(SchemaOrgValidatorTest, InvalidStringPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kSingleFamilyResidence;

  PropertyPtr property = Property::New();
  property->name = property::kAdditionalNumberOfGuests;
  property->values = Values::New();
  property->values->string_values.push_back("foo");

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_TRUE(entity->properties.empty());
}

TEST_F(SchemaOrgValidatorTest, ValidNumberPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kSingleFamilyResidence;

  PropertyPtr property = Property::New();
  property->name = property::kAdditionalNumberOfGuests;
  property->values = Values::New();
  property->values->double_values.push_back(1.0);

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

TEST_F(SchemaOrgValidatorTest, InvalidNumberPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kAboutPage;

  PropertyPtr property = Property::New();
  property->name = property::kAbout;
  property->values = Values::New();
  property->values->double_values.push_back(1.0);

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_TRUE(entity->properties.empty());
}

TEST_F(SchemaOrgValidatorTest, ValidDateTimePropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kLodgingBusiness;

  PropertyPtr property = Property::New();
  property->name = property::kCheckinTime;
  property->values = Values::New();
  property->values->date_time_values.push_back(
      base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMilliseconds(12999772800000)));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

TEST_F(SchemaOrgValidatorTest, InvalidDateTimePropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kAboutPage;

  PropertyPtr property = Property::New();
  property->name = property::kAbout;
  property->values = Values::New();
  property->values->date_time_values.push_back(
      base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMilliseconds(12999772800000)));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_TRUE(entity->properties.empty());
}

TEST_F(SchemaOrgValidatorTest, ValidTimePropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kLodgingBusiness;

  PropertyPtr property = Property::New();
  property->name = property::kCheckinTime;
  property->values = Values::New();
  property->values->time_values.push_back(
      base::TimeDelta::FromMilliseconds(12999772800000));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

TEST_F(SchemaOrgValidatorTest, InvalidTimePropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kAboutPage;

  PropertyPtr property = Property::New();
  property->name = property::kAbout;
  property->values = Values::New();
  property->values->time_values.push_back(
      base::TimeDelta::FromMilliseconds(12999772800000));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_TRUE(entity->properties.empty());
}

TEST_F(SchemaOrgValidatorTest, ValidEntityPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kRestaurant;

  PropertyPtr property = Property::New();
  property->name = property::kAddress;
  property->values = Values::New();

  EntityPtr value = Entity::New();
  value->type = entity::kPostalAddress;
  property->values->entity_values.push_back(std::move(value));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

TEST_F(SchemaOrgValidatorTest, InvalidEntityPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kAboutPage;

  PropertyPtr property = Property::New();
  property->name = property::kAccessMode;
  property->values = Values::New();

  EntityPtr value = Entity::New();
  value->type = entity::kPostalAddress;
  property->values->entity_values.push_back(std::move(value));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_TRUE(entity->properties.empty());
}

TEST_F(SchemaOrgValidatorTest, ValidEntityPropertyValueDescendedType) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kVideoObject;

  PropertyPtr property = Property::New();
  property->name = property::kThumbnail;
  property->values = Values::New();

  // Barcode is descended from ImageObject, an acceptable type for the thumbnail
  // property. So barcode should also be accepted.
  EntityPtr value = Entity::New();
  value->type = entity::kBarcode;
  property->values->entity_values.push_back(std::move(value));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

TEST_F(SchemaOrgValidatorTest, ValidUrlPropertyValueDescendedType) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kVideoObject;

  PropertyPtr property = Property::New();
  property->name = property::kThumbnail;
  property->values = Values::New();

  // Barcode is descended from ImageObject, an acceptable type for the thumbnail
  // property. So barcode should also be accepted.
  property->values->url_values.push_back(GURL("http://schema.org/Barcode"));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

TEST_F(SchemaOrgValidatorTest, ValidRepeatedEntityPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kRestaurant;

  PropertyPtr property = Property::New();
  property->name = property::kAddress;
  property->values = Values::New();

  EntityPtr value1 = Entity::New();
  value1->type = entity::kPostalAddress;
  EntityPtr value2 = Entity::New();
  value2->type = entity::kPostalAddress;

  property->values->entity_values.push_back(std::move(value1));
  property->values->entity_values.push_back(std::move(value2));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
  EXPECT_EQ(2u, entity->properties[0]->values->entity_values.size());
}

// If one value of a repeated property is invalid but the other is not,
// validator should keep the outer property and remove only the invalid nested
// property.
TEST_F(SchemaOrgValidatorTest, MixedValidityRepeatedEntityPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kRestaurant;

  PropertyPtr property = Property::New();
  property->name = property::kAddress;
  property->values = Values::New();

  EntityPtr value1 = Entity::New();
  value1->type = entity::kPostalAddress;
  EntityPtr value2 = Entity::New();
  value2->type = "bad address";

  property->values->entity_values.push_back(std::move(value1));
  property->values->entity_values.push_back(std::move(value2));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
  EXPECT_EQ(1u, entity->properties[0]->values->entity_values.size());
}

TEST_F(SchemaOrgValidatorTest, InvalidRepeatedEntityPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kRestaurant;

  PropertyPtr property = Property::New();
  property->name = property::kAddress;
  property->values = Values::New();

  EntityPtr value1 = Entity::New();
  value1->type = "this is not a real type";
  EntityPtr value2 = Entity::New();
  value2->type = "bad address type";

  property->values->entity_values.push_back(std::move(value1));
  property->values->entity_values.push_back(std::move(value2));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_TRUE(entity->properties.empty());
}

TEST_F(SchemaOrgValidatorTest, ValidEnumPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kAction;

  PropertyPtr property = Property::New();
  property->name = property::kActionStatus;
  property->values = Values::New();
  property->values->url_values.push_back(
      GURL("http://schema.org/ActiveActionStatus"));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

TEST_F(SchemaOrgValidatorTest, InvalidEnumPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kAction;

  PropertyPtr property = Property::New();
  property->name = property::kActionStatus;
  property->values = Values::New();
  property->values->url_values.push_back(
      GURL("http://schema.org/FakeActionStatus"));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_TRUE(entity->properties.empty());
}

TEST_F(SchemaOrgValidatorTest, ValidDurationPropertyValue) {
  EntityPtr entity = Entity::New();
  entity->type = entity::kAction;

  PropertyPtr property = Property::New();
  property->name = property::kDuration;
  property->values = Values::New();
  property->values->time_values.push_back(base::TimeDelta::FromHours(2));

  entity->properties.push_back(std::move(property));

  bool validated_entity = ValidateEntity(entity.get());
  EXPECT_TRUE(validated_entity);
  EXPECT_EQ(1u, entity->properties.size());
}

}  // namespace schema_org
