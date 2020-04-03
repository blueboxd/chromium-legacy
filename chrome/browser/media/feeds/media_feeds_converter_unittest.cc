// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_converter.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/extractor.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "components/schema_org/schema_org_property_names.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media_feeds {

using mojom::MediaFeedItem;
using mojom::MediaFeedItemPtr;
using schema_org::improved::mojom::Entity;
using schema_org::improved::mojom::EntityPtr;
using schema_org::improved::mojom::Property;
using schema_org::improved::mojom::PropertyPtr;
using schema_org::improved::mojom::Values;
using schema_org::improved::mojom::ValuesPtr;

class MediaFeedsConverterTest : public testing::Test {
 public:
  MediaFeedsConverterTest()
      : extractor_({schema_org::entity::kCompleteDataFeed,
                    schema_org::entity::kMovie,
                    schema_org::entity::kWatchAction}) {}

 protected:
  Property* GetProperty(Entity* entity, const std::string& name);
  PropertyPtr CreateStringProperty(const std::string& name,
                                   const std::string& value);
  PropertyPtr CreateLongProperty(const std::string& name, long value);
  PropertyPtr CreateUrlProperty(const std::string& name, const GURL& value);
  PropertyPtr CreateTimeProperty(const std::string& name, int hours);
  PropertyPtr CreateDateTimeProperty(const std::string& name,
                                     const std::string& value);
  PropertyPtr CreateEntityProperty(const std::string& name, EntityPtr value);
  EntityPtr ConvertJSONToEntityPtr(const std::string& json);
  EntityPtr ValidWatchAction();
  EntityPtr ValidMediaFeed();
  EntityPtr ValidMediaFeedItem();
  mojom::MediaFeedItemPtr ExpectedFeedItem();
  EntityPtr AddItemToFeed(EntityPtr feed, EntityPtr item);

 private:
  schema_org::Extractor extractor_;
};

Property* MediaFeedsConverterTest::GetProperty(Entity* entity,
                                               const std::string& name) {
  auto property = std::find_if(
      entity->properties.begin(), entity->properties.end(),
      [&name](const PropertyPtr& property) { return property->name == name; });
  DCHECK(property != entity->properties.end());
  DCHECK((*property)->values);
  return property->get();
}

PropertyPtr MediaFeedsConverterTest::CreateStringProperty(
    const std::string& name,
    const std::string& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->string_values.push_back(value);
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateLongProperty(const std::string& name,
                                                        long value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->long_values.push_back(value);
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateUrlProperty(const std::string& name,
                                                       const GURL& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->url_values.push_back(value);
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateTimeProperty(const std::string& name,
                                                        int hours) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->time_values.push_back(base::TimeDelta::FromHours(hours));
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateDateTimeProperty(
    const std::string& name,
    const std::string& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  base::Time time;
  bool got_time = base::Time::FromString(value.c_str(), &time);
  DCHECK(got_time);
  property->values->date_time_values.push_back(time);
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateEntityProperty(
    const std::string& name,
    EntityPtr value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->entity_values.push_back(std::move(value));
  return property;
}

EntityPtr MediaFeedsConverterTest::ConvertJSONToEntityPtr(
    const std::string& json) {
  return extractor_.Extract(json);
}

EntityPtr MediaFeedsConverterTest::ValidWatchAction() {
  return extractor_.Extract(
      R"END(
      {
        "@type": "WatchAction",
        "target": "https://www.example.org",
        "actionStatus": "https://schema.org/ActiveActionStatus",
        "startTime": "01:00:00"
      }
    )END");
}

EntityPtr MediaFeedsConverterTest::ValidMediaFeed() {
  return extractor_.Extract(
      R"END(
        {
          "@type": "CompleteDataFeed",
          "provider": {
            "@type": "Organization",
            "name": "Media Site",
            "logo": "https://www.example.org/logo.jpg",
            "member": {
              "@type": "Person",
              "name": "Becca Hughes",
              "image": "https://www.example.org/profile_pic.jpg",
              "email": "beccahughes@chromium.org"
            }
          }
        }
      )END");
}

EntityPtr MediaFeedsConverterTest::ValidMediaFeedItem() {
  EntityPtr item = extractor_.Extract(
      R"END(
        {
          "@type": "Movie",
          "@id": "12345",
          "name": "media feed",
          "datePublished": "1970-01-01",
          "image": "https://www.example.com/image.jpg",
          "isFamilyFriendly": "https://schema.org/True"
        }
      )END");

  item->properties.push_back(CreateEntityProperty(
      schema_org::property::kPotentialAction, ValidWatchAction()));
  return item;
}

mojom::MediaFeedItemPtr MediaFeedsConverterTest::ExpectedFeedItem() {
  mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
  expected_item->type = mojom::MediaFeedItemType::kMovie;
  expected_item->name = base::ASCIIToUTF16("media feed");

  media_session::MediaImage expected_image;
  expected_image.src = GURL("https://www.example.com/image.jpg");
  expected_item->images.push_back(std::move(expected_image));

  base::Time time;
  bool got_time = base::Time::FromString("1970-01-01", &time);
  DCHECK(got_time);
  expected_item->date_published = time;

  expected_item->is_family_friendly = true;

  expected_item->action_status = mojom::MediaFeedItemActionStatus::kActive;
  expected_item->action = mojom::Action::New();
  expected_item->action->url = GURL("https://www.example.org");
  expected_item->action->start_time = base::TimeDelta::FromHours(1);
  return expected_item;
}

EntityPtr MediaFeedsConverterTest::AddItemToFeed(EntityPtr feed,
                                                 EntityPtr item) {
  PropertyPtr data_feed_items = Property::New();
  data_feed_items->name = schema_org::property::kDataFeedElement;
  data_feed_items->values = Values::New();
  data_feed_items->values->entity_values.push_back(std::move(item));
  feed->properties.push_back(std::move(data_feed_items));
  return feed;
}

TEST_F(MediaFeedsConverterTest, SucceedsOnValidCompleteDataFeed) {
  std::vector<MediaFeedItemPtr> items;

  EntityPtr entity = ValidMediaFeed();

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(MediaFeedsConverterTest, SucceedsOnValidCompleteDataFeedWithItem) {
  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), ValidMediaFeedItem());

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(ExpectedFeedItem(), result.value()[0]);
}

TEST_F(MediaFeedsConverterTest, FailsWrongType) {
  EntityPtr entity = Entity::New();
  entity->type = "something else";

  EXPECT_FALSE(GetMediaFeeds(std::move(entity)).has_value());
}

TEST_F(MediaFeedsConverterTest, FailsInvalidProviderOrganizationName) {
  EntityPtr entity = ValidMediaFeed();

  Property* organization =
      GetProperty(entity.get(), schema_org::property::kProvider);
  Property* organization_name =
      GetProperty(organization->values->entity_values[0].get(),
                  schema_org::property::kName);

  organization_name->values->string_values = {""};

  EXPECT_FALSE(GetMediaFeeds(std::move(entity)).has_value());
}

TEST_F(MediaFeedsConverterTest, FailsInvalidProviderOrganizationLogo) {
  EntityPtr entity = ValidMediaFeed();

  Property* organization =
      GetProperty(entity.get(), schema_org::property::kProvider);
  Property* organization_name =
      GetProperty(organization->values->entity_values[0].get(),
                  schema_org::property::kLogo);

  organization_name->values->url_values = {GURL("")};

  EXPECT_FALSE(GetMediaFeeds(std::move(entity)).has_value());
}

// Fails because the media feed item name is empty.
TEST_F(MediaFeedsConverterTest, FailsOnInvalidMediaFeedItemName) {
  EntityPtr item = ValidMediaFeedItem();
  auto* name = GetProperty(item.get(), schema_org::property::kName);
  name->values->string_values[0] = "";

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

// Fails because the date published is the wrong type (string instead of
// base::Time).
TEST_F(MediaFeedsConverterTest, FailsInvalidDatePublished) {
  EntityPtr item = ValidMediaFeedItem();
  auto* date_published =
      GetProperty(item.get(), schema_org::property::kDatePublished);
  auto& dates = date_published->values->date_time_values;
  dates.erase(dates.begin());
  date_published->values->string_values.push_back("1970-01-01");

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

// Fails because the value of isFamilyFriendly property is not a parseable
// boolean type.
TEST_F(MediaFeedsConverterTest, FailsInvalidIsFamilyFriendly) {
  EntityPtr item = ValidMediaFeedItem();
  auto* is_family_friendly =
      GetProperty(item.get(), schema_org::property::kIsFamilyFriendly);
  is_family_friendly->values->string_values = {"True"};
  is_family_friendly->values->bool_values.clear();

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

// Fails because an active action does not contain a start time.
TEST_F(MediaFeedsConverterTest, FailsInvalidPotentialAction) {
  EntityPtr item = ValidMediaFeedItem();
  auto* action =
      GetProperty(item.get(), schema_org::property::kPotentialAction);
  auto* start_time = GetProperty(action->values->entity_values[0].get(),
                                 schema_org::property::kStartTime);
  start_time->values->time_values = {};

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

// Succeeds with a valid author and duration on a video object. For other types
// of media, these fields are ignored, but they must be valid on video type.
TEST_F(MediaFeedsConverterTest, SucceedsItemWithAuthorAndDuration) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kVideoObject;
  EntityPtr author = Entity::New();
  author->type = schema_org::entity::kPerson;
  author->properties.push_back(
      CreateStringProperty(schema_org::property::kName, "Becca Hughes"));
  author->properties.push_back(CreateUrlProperty(
      schema_org::property::kUrl, GURL("https://www.google.com")));
  item->properties.push_back(
      CreateEntityProperty(schema_org::property::kAuthor, std::move(author)));
  item->properties.push_back(
      CreateTimeProperty(schema_org::property::kDuration, 1));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kVideo;
  expected_item->author = mojom::Author::New();
  expected_item->author->name = "Becca Hughes";
  expected_item->author->url = GURL("https://www.google.com");
  expected_item->duration = base::TimeDelta::FromHours(1);

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(expected_item, result.value()[0]);
}

// Fails because the author's name is empty.
TEST_F(MediaFeedsConverterTest, FailsInvalidAuthor) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kVideoObject;
  EntityPtr author = Entity::New();
  author->type = schema_org::entity::kPerson;
  author->properties.push_back(
      CreateStringProperty(schema_org::property::kName, ""));
  author->properties.push_back(CreateUrlProperty(
      schema_org::property::kUrl, GURL("https://www.google.com")));
  item->properties.push_back(
      CreateEntityProperty(schema_org::property::kAuthor, std::move(author)));
  item->properties.push_back(
      CreateTimeProperty(schema_org::property::kDuration, 1));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithInteractionStatistic) {
  EntityPtr item = ValidMediaFeedItem();

  EntityPtr interaction_statistic = Entity::New();
  interaction_statistic->type = schema_org::entity::kInteractionCounter;
  interaction_statistic->properties.push_back(
      CreateStringProperty(schema_org::property::kInteractionType,
                           "https://schema.org/WatchAction"));
  interaction_statistic->properties.push_back(
      CreateStringProperty(schema_org::property::kUserInteractionCount, "1"));
  item->properties.push_back(
      CreateEntityProperty(schema_org::property::kInteractionStatistic,
                           std::move(interaction_statistic)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->interaction_counters = {
      {mojom::InteractionCounterType::kWatch, 1}};

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(expected_item, result.value()[0]);
}

// Fails because the interaction statistic property has a duplicate of the watch
// interaction type.
TEST_F(MediaFeedsConverterTest, FailsInvalidInteractionStatistic) {
  EntityPtr item = ValidMediaFeedItem();

  PropertyPtr stats_property = Property::New();
  stats_property->values = Values::New();
  stats_property->name = schema_org::property::kInteractionStatistic;
  {
    EntityPtr interaction_statistic = Entity::New();
    interaction_statistic->type = schema_org::entity::kInteractionCounter;
    interaction_statistic->properties.push_back(
        CreateStringProperty(schema_org::property::kInteractionType,
                             "https://schema.org/WatchAction"));
    interaction_statistic->properties.push_back(
        CreateStringProperty(schema_org::property::kUserInteractionCount, "1"));
    stats_property->values->entity_values.push_back(
        std::move(interaction_statistic));
  }
  {
    EntityPtr interaction_statistic = Entity::New();
    interaction_statistic->type = schema_org::entity::kInteractionCounter;
    interaction_statistic->properties.push_back(
        CreateStringProperty(schema_org::property::kInteractionType,
                             "https://schema.org/WatchAction"));
    interaction_statistic->properties.push_back(
        CreateStringProperty(schema_org::property::kUserInteractionCount, "3"));

    stats_property->values->entity_values.push_back(
        std::move(interaction_statistic));
  }
  item->properties.push_back(std::move(stats_property));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithRating) {
  EntityPtr item = ValidMediaFeedItem();

  {
    EntityPtr rating = Entity::New();
    rating->type = schema_org::entity::kRating;
    rating->properties.push_back(
        CreateStringProperty(schema_org::property::kAuthor, "MPAA"));
    rating->properties.push_back(
        CreateStringProperty(schema_org::property::kRatingValue, "G"));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kContentRating, std::move(rating)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  mojom::ContentRatingPtr rating = mojom::ContentRating::New();
  rating->agency = "MPAA";
  rating->value = "G";
  expected_item->content_ratings.push_back(std::move(rating));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(expected_item, result.value()[0]);
}

// Fails because the rating property has a rating from an unknown agency.
TEST_F(MediaFeedsConverterTest, FailsInvalidRating) {
  EntityPtr item = ValidMediaFeedItem();

  EntityPtr rating = Entity::New();
  rating->type = schema_org::entity::kRating;
  rating->properties.push_back(
      CreateStringProperty(schema_org::property::kAuthor, "Google"));
  rating->properties.push_back(
      CreateStringProperty(schema_org::property::kRatingValue, "Googley"));
  item->properties.push_back(CreateEntityProperty(
      schema_org::property::kContentRating, std::move(rating)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithGenre) {
  EntityPtr item = ValidMediaFeedItem();

  item->properties.push_back(
      CreateStringProperty(schema_org::property::kGenre, "Action"));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->genre.push_back("Action");

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(expected_item, result.value()[0]);
}

TEST_F(MediaFeedsConverterTest, FailsItemWithInvalidGenre) {
  EntityPtr item = ValidMediaFeedItem();

  item->properties.push_back(
      CreateStringProperty(schema_org::property::kGenre, ""));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithLiveDetails) {
  EntityPtr item = ValidMediaFeedItem();

  EntityPtr publication = Entity::New();
  publication->type = schema_org::entity::kBroadcastEvent;
  publication->properties.push_back(
      CreateDateTimeProperty(schema_org::property::kStartDate, "2020-03-22"));
  publication->properties.push_back(
      CreateDateTimeProperty(schema_org::property::kEndDate, "2020-03-23"));
  item->properties.push_back(CreateEntityProperty(
      schema_org::property::kPublication, std::move(publication)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->live = mojom::LiveDetails::New();
  base::Time start_time, end_time;
  bool parsed_start = base::Time::FromString("2020-03-22", &start_time);
  bool parsed_end = base::Time::FromString("2020-03-23", &end_time);
  DCHECK(parsed_start && parsed_end);
  expected_item->live->start_time = start_time;
  expected_item->live->end_time = end_time;

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(expected_item, result.value()[0]);
}

// Fails because the end date is string type instead of date type.
TEST_F(MediaFeedsConverterTest, FailsItemWithInvalidLiveDetails) {
  EntityPtr item = ValidMediaFeedItem();

  EntityPtr publication = Entity::New();
  publication->type = schema_org::entity::kBroadcastEvent;
  publication->properties.push_back(
      CreateDateTimeProperty(schema_org::property::kStartTime, "2020-03-22"));
  publication->properties.push_back(
      CreateStringProperty(schema_org::property::kEndTime, "2020-03-23"));
  item->properties.push_back(CreateEntityProperty(
      schema_org::property::kPublication, std::move(publication)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithIdentifier) {
  EntityPtr item = ValidMediaFeedItem();

  {
    EntityPtr identifier = Entity::New();
    identifier->type = schema_org::entity::kPropertyValue;
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kPropertyID, "TMS_ROOT_ID"));
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kValue, "1"));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kIdentifier, std::move(identifier)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  mojom::IdentifierPtr identifier = mojom::Identifier::New();
  identifier->type = mojom::Identifier::Type::kTMSRootId;
  identifier->value = "1";
  expected_item->identifiers.push_back(std::move(identifier));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(expected_item, result.value()[0]);
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithInvalidIdentifier) {
  EntityPtr item = ValidMediaFeedItem();

  {
    EntityPtr identifier = Entity::New();
    identifier->type = schema_org::entity::kPropertyValue;
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kPropertyID, "Unknown"));
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kValue, "1"));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kPublication, std::move(identifier)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

// Successfully converts a TV episode with embedded watch action and optional
// identifiers.
TEST_F(MediaFeedsConverterTest, SucceedsItemWithTVEpisode) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;
  // Ignore the item's action field by changing the name. Use the action
  // embedded in the TV episode instead.
  GetProperty(item.get(), schema_org::property::kPotentialAction)->name =
      "not an action";
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfSeasons, 6));

  {
    EntityPtr episode = Entity::New();
    episode->type = schema_org::entity::kTVEpisode;
    episode->properties.push_back(
        CreateLongProperty(schema_org::property::kEpisodeNumber, 1));
    episode->properties.push_back(
        CreateStringProperty(schema_org::property::kName, "Pilot"));
    EntityPtr identifier = Entity::New();
    identifier->type = schema_org::entity::kPropertyValue;
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kPropertyID, "TMS_ROOT_ID"));
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kValue, "1"));
    episode->properties.push_back(CreateEntityProperty(
        schema_org::property::kIdentifier, std::move(identifier)));
    episode->properties.push_back(CreateEntityProperty(
        schema_org::property::kPotentialAction, ValidWatchAction()));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kEpisode, std::move(episode)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kTVSeries;
  expected_item->tv_episode = mojom::TVEpisode::New();
  expected_item->tv_episode->episode_number = 1;
  expected_item->tv_episode->name = "Pilot";
  mojom::IdentifierPtr identifier = mojom::Identifier::New();
  identifier->type = mojom::Identifier::Type::kTMSRootId;
  identifier->value = "1";
  expected_item->tv_episode->identifiers.push_back(std::move(identifier));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(expected_item, result.value()[0]);
}

// Fails because TV episode is present, but TV episode name is empty.
TEST_F(MediaFeedsConverterTest, FailsItemWithInvalidTVEpisode) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfSeasons, 6));

  EntityPtr episode = Entity::New();
  episode->type = schema_org::entity::kTVEpisode;
  episode->properties.push_back(
      CreateLongProperty(schema_org::property::kEpisodeNumber, 1));
  episode->properties.push_back(
      CreateStringProperty(schema_org::property::kName, ""));
  episode->properties.push_back(CreateEntityProperty(
      schema_org::property::kPotentialAction, ValidWatchAction()));
  item->properties.push_back(
      CreateEntityProperty(schema_org::property::kEpisode, std::move(episode)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithTVSeason) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfSeasons, 6));

  {
    EntityPtr season = Entity::New();
    season->type = schema_org::entity::kTVSeason;
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kSeasonNumber, 1));
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kContainsSeason, std::move(season)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kTVSeries;
  expected_item->tv_episode = mojom::TVEpisode::New();
  expected_item->tv_episode->season_number = 1;

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(expected_item, result.value()[0]);
}

TEST_F(MediaFeedsConverterTest, FailsItemWithInvalidTVSeason) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfSeasons, 6));

  {
    EntityPtr season = Entity::New();
    season->type = schema_org::entity::kTVSeason;
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kSeasonNumber, 1));
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kNumberOfEpisodes, -1));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kContainsSeason, std::move(season)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetMediaFeeds(std::move(entity));

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().empty());
}

}  // namespace media_feeds
