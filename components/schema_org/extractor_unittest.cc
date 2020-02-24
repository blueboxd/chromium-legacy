// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/schema_org/extractor.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/schema_org/common/metadata.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace schema_org {

using mojom::Entity;
using mojom::EntityPtr;
using mojom::Property;
using mojom::PropertyPtr;
using mojom::Values;
using mojom::ValuesPtr;

class SchemaOrgExtractorTest : public testing::Test {
 public:
  SchemaOrgExtractorTest() = default;

 protected:
  EntityPtr Extract(const std::string& text) {
    return Extractor::Extract(text);
  }

  PropertyPtr CreateStringProperty(const std::string& name,
                                   const std::string& value);

  PropertyPtr CreateBooleanProperty(const std::string& name, const bool& value);

  PropertyPtr CreateLongProperty(const std::string& name, const int64_t& value);

  PropertyPtr CreateEntityProperty(const std::string& name, EntityPtr value);
};

PropertyPtr SchemaOrgExtractorTest::CreateStringProperty(
    const std::string& name,
    const std::string& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->set_string_values({value});
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateBooleanProperty(
    const std::string& name,
    const bool& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->set_bool_values({value});
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateLongProperty(const std::string& name,
                                                       const int64_t& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->set_long_values({value});
  return property;
}

PropertyPtr SchemaOrgExtractorTest::CreateEntityProperty(
    const std::string& name,
    EntityPtr value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->set_entity_values(std::vector<EntityPtr>());
  property->values->get_entity_values().push_back(std::move(value));
  return property;
}

TEST_F(SchemaOrgExtractorTest, Empty) {
  ASSERT_TRUE(Extract("").is_null());
}

TEST_F(SchemaOrgExtractorTest, Basic) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"name\": \"a video!\"}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateStringProperty("name", "a video!"));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, booleanValue) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"requiresSubscription\": true }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(
      CreateBooleanProperty("requiresSubscription", true));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, longValue) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"position\": 111 }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateLongProperty("position", 111));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, doubleValue) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"width\": 111.5 }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateStringProperty("width", "111.5"));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, NestedEntities) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"actor\": { \"@type\": \"Person\", "
      "\"name\": \"Talented Actor\" }  }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EntityPtr nested = Entity::New();
  nested->type = "Person";
  nested->properties.push_back(CreateStringProperty("name", "Talented Actor"));

  expected->properties.push_back(
      CreateEntityProperty("actor", std::move(nested)));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, RepeatedProperty) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": [\"Movie Title\", \"The Second "
      "One\"] }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  PropertyPtr name = Property::New();
  name->name = "name";
  name->values = Values::New();
  std::vector<std::string> nameValues;
  nameValues.push_back("Movie Title");
  nameValues.push_back("The Second One");
  name->values->set_string_values(nameValues);

  expected->properties.push_back(std::move(name));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, RepeatedObject) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"actor\": [ {\"@type\": \"Person\", "
      "\"name\": \"Talented "
      "Actor\"}, {\"@type\": \"Person\", \"name\": \"Famous Actor\"} ] }");

  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  PropertyPtr actorProperty = Property::New();
  actorProperty->name = "actor";
  actorProperty->values = Values::New();
  actorProperty->values->set_entity_values(std::vector<EntityPtr>());

  EntityPtr nested1 = Entity::New();
  nested1->type = "Person";
  nested1->properties.push_back(CreateStringProperty("name", "Talented Actor"));
  actorProperty->values->get_entity_values().push_back(std::move(nested1));

  EntityPtr nested2 = Entity::New();
  nested2->type = "Person";
  nested2->properties.push_back(CreateStringProperty("name", "Famous Actor"));
  actorProperty->values->get_entity_values().push_back(std::move(nested2));

  expected->properties.push_back(std::move(actorProperty));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, TruncateLongString) {
  std::string maxLengthString = "";
  for (int i = 0; i < 200; ++i) {
    maxLengthString += "a";
  }
  std::string tooLongString;
  tooLongString.append(maxLengthString);
  tooLongString.append("a");

  EntityPtr extracted = Extract("{\"@type\": \"VideoObject\", \"name\": \"" +
                                tooLongString + "\"}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  expected->properties.push_back(CreateStringProperty("name", maxLengthString));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, EnforceTypeExists) {
  EntityPtr extracted = Extract("{\"name\": \"a video!\"}");
  ASSERT_TRUE(extracted.is_null());
}

TEST_F(SchemaOrgExtractorTest, UnhandledTypeIgnored) {
  EntityPtr extracted =
      Extract("{\"@type\": \"UnsupportedType\", \"name\": \"a video!\"}");
  ASSERT_TRUE(extracted.is_null());
}

TEST_F(SchemaOrgExtractorTest, TruncateTooManyValuesInField) {
  std::string largeRepeatedField = "[";
  for (int i = 0; i < 101; ++i) {
    largeRepeatedField += "\"a\"";
    if (i != 100) {
      largeRepeatedField += ",";
    }
  }
  largeRepeatedField += "]";

  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": " + largeRepeatedField + "}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";
  PropertyPtr name = Property::New();
  name->name = "name";
  name->values = Values::New();
  std::vector<std::string> nameValues;

  for (int i = 0; i < 100; i++) {
    nameValues.push_back("a");
  }
  name->values->set_string_values(nameValues);
  expected->properties.push_back(std::move(name));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, truncateTooManyFields) {
  std::stringstream tooManyFields;
  for (int i = 0; i < 26; ++i) {
    tooManyFields << "\"" << i << "\": \"a\"";
    if (i != 25) {
      tooManyFields << ",";
    }
  }
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\"," + tooManyFields.str() + "}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  for (int i = 0; i < 25; ++i) {
    expected->properties.push_back(
        CreateStringProperty(base::NumberToString(i), "a"));
  }

  EXPECT_EQ(expected->properties.size(), extracted->properties.size());
}

TEST_F(SchemaOrgExtractorTest, IgnorePropertyWithEmptyArray) {
  EntityPtr extracted = Extract("{\"@type\": \"VideoObject\", \"name\": [] }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, IgnorePropertyWithMixedTypes) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"name\": [\"Name\", 1] }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, IgnorePropertyWithNestedArray) {
  EntityPtr extracted =
      Extract("{\"@type\": \"VideoObject\", \"name\": [[\"Name\"]] }");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, EnforceMaxNestingDepth) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": \"a video!\","
      "\"1\": {"
      "  \"2\": {"
      "    \"3\": {"
      "      \"4\": {"
      "        \"5\": {"
      "          \"6\": 7"
      "        }"
      "      }"
      "    }"
      "  }"
      "}"
      "}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EntityPtr entity1 = Entity::New();
  entity1->type = "Thing";
  EntityPtr entity2 = Entity::New();
  entity2->type = "Thing";
  EntityPtr entity3 = Entity::New();
  entity3->type = "Thing";
  EntityPtr entity4 = Entity::New();
  entity4->type = "Thing";

  entity3->properties.push_back(CreateEntityProperty("4", std::move(entity4)));
  entity2->properties.push_back(CreateEntityProperty("3", std::move(entity3)));
  entity1->properties.push_back(CreateEntityProperty("2", std::move(entity2)));
  expected->properties.push_back(CreateEntityProperty("1", std::move(entity1)));
  expected->properties.push_back(CreateStringProperty("name", "a video!"));

  EXPECT_EQ(expected, extracted);
}

TEST_F(SchemaOrgExtractorTest, MaxNestingDepthWithTerminalProperty) {
  EntityPtr extracted = Extract(
      "{\"@type\": \"VideoObject\", \"name\": \"a video!\","
      "\"1\": {"
      "  \"2\": {"
      "    \"3\": {"
      "      \"4\": {"
      "        \"5\": 6"
      "         }"
      "      }"
      "    }"
      "  }"
      "}");
  ASSERT_FALSE(extracted.is_null());

  EntityPtr expected = Entity::New();
  expected->type = "VideoObject";

  EntityPtr entity1 = Entity::New();
  entity1->type = "Thing";
  EntityPtr entity2 = Entity::New();
  entity2->type = "Thing";
  EntityPtr entity3 = Entity::New();
  entity3->type = "Thing";
  EntityPtr entity4 = Entity::New();
  entity4->type = "Thing";

  entity4->properties.push_back(CreateLongProperty("5", 6));
  entity3->properties.push_back(CreateEntityProperty("4", std::move(entity4)));
  entity2->properties.push_back(CreateEntityProperty("3", std::move(entity3)));
  entity1->properties.push_back(CreateEntityProperty("2", std::move(entity2)));

  expected->properties.push_back(CreateEntityProperty("1", std::move(entity1)));
  expected->properties.push_back(CreateStringProperty("name", "a video!"));

  EXPECT_EQ(expected, extracted);
}

}  // namespace schema_org
