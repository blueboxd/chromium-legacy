// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_converter.h"

#include <numeric>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "components/autofill/core/browser/validation.h"
#include "components/schema_org/common/improved_metadata.mojom-forward.h"
#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "components/schema_org/schema_org_enums.h"
#include "components/schema_org/schema_org_property_names.h"

namespace media_feeds {

using schema_org::improved::mojom::Entity;
using schema_org::improved::mojom::EntityPtr;
using schema_org::improved::mojom::Property;
using schema_org::improved::mojom::PropertyPtr;
using schema_org::improved::mojom::ValuesPtr;

static int constexpr kMaxRatings = 5;
static int constexpr kMaxGenres = 3;
static int constexpr kMaxInteractionStatistics = 3;
static int constexpr kMaxImages = 5;

// Gets the property of entity with corresponding name. May be null if not found
// or if the property has no values.
Property* GetProperty(Entity* entity, const std::string& name) {
  auto property = std::find_if(
      entity->properties.begin(), entity->properties.end(),
      [&name](const PropertyPtr& property) { return property->name == name; });
  if (property == entity->properties.end())
    return nullptr;
  if (!(*property)->values)
    return nullptr;
  return property->get();
}

// Converts a property named property_name and store the result in the
// converted_item struct using the convert_property callback. Returns true only
// is the conversion was successful. If is_required is set, the property must be
// found and be valid. If is_required is not set, returns false only if the
// property is found and is invalid.
template <typename T>
bool ConvertProperty(
    Entity* entity,
    T* converted_item,
    const std::string& property_name,
    bool is_required,
    base::OnceCallback<bool(const Property& property, T*)> convert_property) {
  auto* property = GetProperty(entity, property_name);
  if (!property)
    return !is_required;
  return std::move(convert_property).Run(*property, converted_item);
}

// Validates a property identified by name using the provided callback. Returns
// true only if the property is valid.
bool ValidateProperty(
    Entity* entity,
    const std::string& name,
    base::OnceCallback<bool(const Property& property)> property_is_valid) {
  auto property = std::find_if(
      entity->properties.begin(), entity->properties.end(),
      [&name](const PropertyPtr& property) { return property->name == name; });
  if (property == entity->properties.end())
    return false;
  if (!(*property)->values)
    return false;
  return std::move(property_is_valid).Run(**property);
}

// Checks that the property contains at least one URL and that all URLs it
// contains are valid.
bool IsUrl(const Property& property) {
  return !property.values->url_values.empty() &&
         std::accumulate(property.values->url_values.begin(),
                         property.values->url_values.end(), true,
                         [](auto& accumulation, auto& url_value) {
                           return accumulation && url_value.is_valid();
                         });
}

// Checks that the property contains at least positive integer and that all
// numbers it contains are positive integers.
bool IsPositiveInteger(const Property& property) {
  return !property.values->long_values.empty() &&
         std::accumulate(property.values->long_values.begin(),
                         property.values->long_values.end(), true,
                         [](auto& accumulation, auto& long_value) {
                           return accumulation && long_value > 0;
                         });
}

// Checks that the property contains at least one non-empty string and that all
// strings it contains are non-empty.
bool IsNonEmptyString(const Property& property) {
  return (!property.values->string_values.empty() &&
          std::accumulate(property.values->string_values.begin(),
                          property.values->string_values.end(), true,
                          [](auto& accumulation, auto& string_value) {
                            return accumulation && !string_value.empty();
                          }));
}

// Checks that the property contains at least one valid email address.
bool IsEmail(const Property& email) {
  if (email.values->string_values.empty())
    return false;

  return autofill::IsValidEmailAddress(
      base::ASCIIToUTF16(email.values->string_values[0]));
}

// Checks whether the media item type is supported.
bool IsMediaItemType(const std::string& type) {
  static const base::NoDestructor<base::flat_set<base::StringPiece>>
      kSupportedTypes(base::flat_set<base::StringPiece>(
          {schema_org::entity::kVideoObject, schema_org::entity::kMovie,
           schema_org::entity::kTVSeries}));
  return kSupportedTypes->find(type) != kSupportedTypes->end();
}

// Checks that the property contains at least one valid date / date-time.
bool IsDateOrDateTime(const Property& property) {
  return !property.values->date_time_values.empty();
}

// Gets a number from the property which may be stored either as a long or a
// string.
base::Optional<uint64_t> GetNumber(const Property& property) {
  if (!property.values->long_values.empty())
    return property.values->long_values[0];
  if (!property.values->string_values.empty()) {
    uint64_t number;
    bool parsed_number =
        base::StringToUint64(property.values->string_values[0], &number);
    if (parsed_number)
      return number;
  }
  return base::nullopt;
}

// Gets a list of media images from the property. The property should have at
// least one media image and no more than kMaxImages. A media image is either a
// valid URL string or an ImageObject entity containing a width, height, and
// URL.
base::Optional<std::vector<media_session::MediaImage>> GetMediaImage(
    const Property& property) {
  if (property.values->url_values.empty() &&
      property.values->entity_values.empty()) {
    return base::nullopt;
  }

  std::vector<media_session::MediaImage> images;

  for (const auto& url : property.values->url_values) {
    if (!url.is_valid())
      continue;
    media_session::MediaImage image;
    image.src = url;

    images.push_back(std::move(image));
    if (images.size() == kMaxImages)
      return images;
  }

  for (const auto& image_object : property.values->entity_values) {
    if (image_object->type != schema_org::entity::kImageObject)
      continue;

    auto* width = GetProperty(image_object.get(), schema_org::property::kWidth);
    if (!width || !IsPositiveInteger(*width))
      continue;

    auto* height =
        GetProperty(image_object.get(), schema_org::property::kWidth);
    if (!height || !IsPositiveInteger(*height))
      continue;

    auto* url = GetProperty(image_object.get(), schema_org::property::kUrl);
    if (!url)
      url = GetProperty(image_object.get(), schema_org::property::kEmbedUrl);
    if (!IsUrl(*url))
      continue;

    media_session::MediaImage image;
    image.src = url->values->url_values[0];
    image.sizes.push_back(gfx::Size(width->values->long_values[0],
                                    height->values->long_values[0]));

    images.push_back(std::move(image));
    if (images.size() == kMaxImages)
      return images;
  }
  return images;
}

// Validates the provider property of an entity. Outputs the name and images
// properties.
bool ValidateProvider(const Property& provider,
                      std::string* display_name,
                      std::vector<media_session::MediaImage>* images) {
  if (provider.values->entity_values.empty())
    return false;

  auto organization = std::find_if(
      provider.values->entity_values.begin(),
      provider.values->entity_values.end(), [](const EntityPtr& value) {
        return value->type == schema_org::entity::kOrganization;
      });

  if (organization == provider.values->entity_values.end())
    return false;

  auto* name = GetProperty(organization->get(), schema_org::property::kName);
  if (!name || !IsNonEmptyString(*name))
    return false;
  *display_name = name->values->string_values[0];

  auto* logo = GetProperty(organization->get(), schema_org::property::kLogo);
  if (!logo)
    return false;
  auto maybe_images = GetMediaImage(*logo);
  if (!maybe_images.has_value() || maybe_images.value().empty())
    return false;
  *images = maybe_images.value();

  return true;
}

// Gets the author property and stores the result in item. Returns true if the
// author was valid.
bool GetMediaItemAuthor(const Property& author, mojom::MediaFeedItem* item) {
  item->author = mojom::Author::New();

  if (IsNonEmptyString(author)) {
    item->author->name = author.values->string_values[0];
    return true;
  }

  if (author.values->entity_values.empty())
    return false;

  auto person = std::find_if(
      author.values->entity_values.begin(), author.values->entity_values.end(),
      [](const EntityPtr& value) {
        return value->type == schema_org::entity::kPerson;
      });

  auto* name = GetProperty(person->get(), schema_org::property::kName);
  if (!name || !IsNonEmptyString(*name))
    return false;
  item->author->name = name->values->string_values[0];

  auto* url = GetProperty(person->get(), schema_org::property::kUrl);
  if (url) {
    if (!IsUrl(*url))
      return false;
    item->author->url = url->values->url_values[0];
  }

  return true;
}

// Gets the ratings property and stores the result in item. Returns true if the
// ratings were valid.
bool GetContentRatings(const Property& property, mojom::MediaFeedItem* item) {
  if (property.values->entity_values.empty() ||
      property.values->entity_values.size() > kMaxRatings)
    return false;

  for (const auto& rating : property.values->entity_values) {
    mojom::ContentRatingPtr converted_rating = mojom::ContentRating::New();

    if (rating->type != schema_org::entity::kRating)
      return false;

    auto* author = GetProperty(rating.get(), schema_org::property::kAuthor);
    if (!author || !IsNonEmptyString(*author))
      return false;

    static const base::NoDestructor<base::flat_set<base::StringPiece>>
        kRatingAgencies(base::flat_set<base::StringPiece>(
            {"TVPG", "MPAA", "BBFC", "CSA", "AGCOM", "FSK", "SETSI", "ICAA",
             "NA", "EIRIN", "KMRB", "CLASSIND", "MKRF", "CBFC", "KPI", "LSF",
             "RTC"}));
    if (!kRatingAgencies->contains(author->values->string_values[0]))
      return false;
    converted_rating->agency = author->values->string_values[0];

    auto* rating_value =
        GetProperty(rating.get(), schema_org::property::kRatingValue);
    if (!rating_value || !IsNonEmptyString(*rating_value))
      return false;
    converted_rating->value = rating_value->values->string_values[0];

    item->content_ratings.push_back(std::move(converted_rating));
  }
  return true;
}

// Gets the identifiers property and stores the result in item. Item should be a
// struct with an identifiers field. Returns true if the identifiers were valid.
template <typename T>
bool GetIdentifiers(const Property& property, T* item) {
  if (property.values->entity_values.empty())
    return false;

  std::vector<mojom::IdentifierPtr> identifiers;

  for (const auto& identifier : property.values->entity_values) {
    mojom::IdentifierPtr converted_identifier = mojom::Identifier::New();

    if (identifier->type != schema_org::entity::kPropertyValue)
      return false;

    auto* property_id =
        GetProperty(identifier.get(), schema_org::property::kPropertyID);
    if (!property_id || !IsNonEmptyString(*property_id))
      return false;
    std::string property_id_str = property_id->values->string_values[0];
    if (property_id_str == "TMS_ROOT_ID") {
      converted_identifier->type = mojom::Identifier::Type::kTMSRootId;
    } else if (property_id_str == "TMS_ID") {
      converted_identifier->type = mojom::Identifier::Type::kTMSId;
    } else if (property_id_str == "_PARTNER_ID_") {
      converted_identifier->type = mojom::Identifier::Type::kPartnerId;
    } else {
      return false;
    }

    auto* value = GetProperty(identifier.get(), schema_org::property::kValue);
    if (!value || !IsNonEmptyString(*value))
      return false;
    converted_identifier->value = value->values->string_values[0];

    item->identifiers.push_back(std::move(converted_identifier));
  }
  return true;
}

// Gets the interaction type from a property containing an interaction type
// string.
base::Optional<mojom::InteractionCounterType> GetInteractionType(
    const Property& property) {
  if (property.values->string_values.empty())
    return base::nullopt;
  GURL type = GURL(property.values->string_values[0]);
  if (!type.SchemeIsHTTPOrHTTPS() || type.host() != "schema.org")
    return base::nullopt;

  std::string type_path = type.path().substr(1);
  if (type_path == schema_org::entity::kWatchAction) {
    return mojom::InteractionCounterType::kWatch;
  } else if (type_path == schema_org::entity::kLikeAction) {
    return mojom::InteractionCounterType::kLike;
  } else if (type_path == schema_org::entity::kDislikeAction) {
    return mojom::InteractionCounterType::kDislike;
  }
  return base::nullopt;
}

// Gets the interaction statistics property and stores the result in item.
// Returns true if the statistics were valid.
bool GetInteractionStatistics(const Property& property,
                              mojom::MediaFeedItem* item) {
  if (property.values->entity_values.empty() ||
      property.values->entity_values.size() > kMaxInteractionStatistics) {
    return false;
  }
  for (const auto& stat : property.values->entity_values) {
    if (stat->type != schema_org::entity::kInteractionCounter)
      return false;

    auto* interaction_type =
        GetProperty(stat.get(), schema_org::property::kInteractionType);
    if (!interaction_type)
      return false;
    auto type = GetInteractionType(*interaction_type);
    if (!type.has_value() || item->interaction_counters.count(type.value()) > 0)
      return false;

    auto* user_interaction_count =
        GetProperty(stat.get(), schema_org::property::kUserInteractionCount);
    if (!user_interaction_count)
      return false;
    base::Optional<uint64_t> count = GetNumber(*user_interaction_count);
    if (!count.has_value())
      return false;
    item->interaction_counters.insert(
        std::pair<mojom::InteractionCounterType, uint64_t>(type.value(),
                                                           count.value()));
  }
  if (item->interaction_counters.empty())
    return false;

  return true;
}

base::Optional<mojom::MediaFeedItemType> GetMediaItemType(
    const std::string& schema_org_type) {
  if (schema_org_type == schema_org::entity::kVideoObject) {
    return mojom::MediaFeedItemType::kVideo;
  } else if (schema_org_type == schema_org::entity::kMovie) {
    return mojom::MediaFeedItemType::kMovie;
  } else if (schema_org_type == schema_org::entity::kTVSeries) {
    return mojom::MediaFeedItemType::kTVSeries;
  }
  return base::nullopt;
}

// Gets the isFamilyFriendly property and stores the result in item.
bool GetIsFamilyFriendly(const Property& property, mojom::MediaFeedItem* item) {
  if (property.values->bool_values.empty()) {
    return false;
  }

  item->is_family_friendly = property.values->bool_values[0];
  return true;
}

// Gets the watchAction and actionStatus properties from an embedded entity and
// stores the result in item. Returns true if both the action and the action
// status were valid.
bool GetActionAndStatus(const Property& property, mojom::MediaFeedItem* item) {
  if (property.values->entity_values.empty())
    return false;

  EntityPtr& action = property.values->entity_values[0];
  if (action->type != schema_org::entity::kWatchAction)
    return false;

  item->action = mojom::Action::New();

  auto* target = GetProperty(action.get(), schema_org::property::kTarget);
  if (!target || !IsUrl(*target))
    return false;
  item->action->url = target->values->url_values[0];

  auto* action_status =
      GetProperty(action.get(), schema_org::property::kActionStatus);
  if (action_status) {
    if (!IsUrl(*action_status))
      return false;

    auto status = schema_org::enums::CheckValidEnumString(
        "http://schema.org/ActionStatusType",
        action_status->values->url_values[0]);
    if (status == base::nullopt) {
      return false;
    } else if (status.value() ==
               static_cast<int>(
                   schema_org::enums::ActionStatusType::kActiveActionStatus)) {
      item->action_status = mojom::MediaFeedItemActionStatus::kActive;
      auto* start_time =
          GetProperty(action.get(), schema_org::property::kStartTime);
      if (!start_time || start_time->values->time_values.empty())
        return false;
      item->action->start_time = start_time->values->time_values[0];
    } else if (status.value() ==
               static_cast<int>(schema_org::enums::ActionStatusType::
                                    kPotentialActionStatus)) {
      item->action_status = mojom::MediaFeedItemActionStatus::kPotential;
    } else if (status.value() ==
               static_cast<int>(schema_org::enums::ActionStatusType::
                                    kCompletedActionStatus)) {
      item->action_status = mojom::MediaFeedItemActionStatus::kCompleted;
    }
  }
  return true;
}

// Gets the TV episode stored in an embedded entity and stores the result in
// item. Returns true if the TV episode was valid.
bool GetEpisode(const Property& property, mojom::MediaFeedItem* item) {
  if (property.values->entity_values.empty())
    return false;

  EntityPtr& episode = property.values->entity_values[0];
  if (episode->type != schema_org::entity::kTVEpisode)
    return false;

  if (!item->tv_episode)
    item->tv_episode = mojom::TVEpisode::New();

  auto* episode_number =
      GetProperty(episode.get(), schema_org::property::kEpisodeNumber);
  if (!episode_number || !IsPositiveInteger(*episode_number))
    return false;
  item->tv_episode->episode_number = episode_number->values->long_values[0];

  auto* name = GetProperty(episode.get(), schema_org::property::kName);
  if (!name || !IsNonEmptyString(*name))
    return false;
  item->tv_episode->name = name->values->string_values[0];

  if (!ConvertProperty<mojom::TVEpisode>(
          episode.get(), item->tv_episode.get(),
          schema_org::property::kIdentifier, false,
          base::BindOnce(&GetIdentifiers<mojom::TVEpisode>))) {
    return false;
  }

  auto* image = GetProperty(episode.get(), schema_org::property::kImage);
  if (image) {
    auto converted_images = GetMediaImage(*image);
    if (!converted_images.has_value())
      return false;
    // TODO(sgbowen): Add an images field to TV episodes and store the converted
    // images here.
  }

  if (!ConvertProperty<mojom::MediaFeedItem>(
          episode.get(), item, schema_org::property::kPotentialAction, true,
          base::BindOnce(&GetActionAndStatus))) {
    return false;
  }

  return true;
}

// Gets the TV season stored in an embedded entity and stores the result in
// item. Returns true if the TV season was valid.
bool GetSeason(const Property& property, mojom::MediaFeedItem* item) {
  if (property.values->entity_values.empty())
    return false;

  EntityPtr& season = property.values->entity_values[0];
  if (season->type != schema_org::entity::kTVSeason)
    return false;

  if (!item->tv_episode)
    item->tv_episode = mojom::TVEpisode::New();

  auto* season_number =
      GetProperty(season.get(), schema_org::property::kSeasonNumber);
  if (!season_number || !IsPositiveInteger(*season_number))
    return false;
  item->tv_episode->season_number = season_number->values->long_values[0];

  auto* number_episodes =
      GetProperty(season.get(), schema_org::property::kNumberOfEpisodes);
  if (!number_episodes || !IsPositiveInteger(*number_episodes))
    return false;

  if (!ConvertProperty<mojom::MediaFeedItem>(
          season.get(), item, schema_org::property::kEpisode, false,
          base::BindOnce(&GetIdentifiers<mojom::MediaFeedItem>))) {
    return false;
  }

  return true;
}

// Gets the broadcastEvent entity from the property and store the result in item
// as LiveDetails. Returns true if the broadcastEven was valid.
bool GetLiveDetails(const Property& property, mojom::MediaFeedItem* item) {
  if (property.values->entity_values.empty())
    return false;

  EntityPtr& publication = property.values->entity_values[0];
  if (publication->type != schema_org::entity::kBroadcastEvent)
    return false;

  item->live = mojom::LiveDetails::New();

  auto* start_date =
      GetProperty(publication.get(), schema_org::property::kStartDate);
  if (!start_date || !IsDateOrDateTime(*start_date))
    return false;
  item->live->start_time = start_date->values->date_time_values[0];

  auto* end_date =
      GetProperty(publication.get(), schema_org::property::kEndDate);
  if (end_date) {
    if (!IsDateOrDateTime(*end_date))
      return false;
    item->live->end_time = end_date->values->date_time_values[0];
  }

  return true;
}

// Gets the duration from the property and store the result in item. Returns
// true if the duration was valid.
bool GetDuration(const Property& property, mojom::MediaFeedItem* item) {
  if (property.values->time_values.empty())
    return false;

  item->duration = property.values->time_values[0];
  return true;
}

// Given the schema.org data_feed_items, iterate through and convert all feed
// items into MediaFeedItemPtr types. Store the converted items in
// converted_feed_items. Skips invalid feed items.
void GetDataFeedItems(
    const PropertyPtr& data_feed_items,
    std::vector<mojom::MediaFeedItemPtr>* converted_feed_items) {
  if (data_feed_items->values->entity_values.empty())
    return;

  base::flat_set<std::string> item_ids;

  for (const auto& item : data_feed_items->values->entity_values) {
    mojom::MediaFeedItemPtr converted_item = mojom::MediaFeedItem::New();
    auto convert_property =
        base::BindRepeating(&ConvertProperty<mojom::MediaFeedItem>, item.get(),
                            converted_item.get());

    auto type = GetMediaItemType(item->type);
    if (!type.has_value())
      continue;
    converted_item->type = type.value();

    // Check that the id is present and unique. This does not get converted.
    if (item->id == "" || item_ids.find(item->id) != item_ids.end()) {
      continue;
    }
    item_ids.insert(item->id);

    auto* name = GetProperty(item.get(), schema_org::property::kName);
    if (name && IsNonEmptyString(*name)) {
      converted_item->name = base::ASCIIToUTF16(name->values->string_values[0]);
    } else {
      continue;
    }

    auto* date_published =
        GetProperty(item.get(), schema_org::property::kDatePublished);
    if (date_published && !date_published->values->date_time_values.empty()) {
      converted_item->date_published =
          date_published->values->date_time_values[0];
    } else {
      continue;
    }

    if (!convert_property.Run(schema_org::property::kIsFamilyFriendly, true,
                              base::BindOnce(&GetIsFamilyFriendly))) {
      continue;
    }

    auto* image = GetProperty(item.get(), schema_org::property::kImage);
    if (!image)
      continue;
    auto converted_images = GetMediaImage(*image);
    if (!converted_images.has_value())
      continue;
    converted_item->images = converted_images.value();

    bool has_embedded_action =
        item->type == schema_org::entity::kTVSeries &&
        GetProperty(item.get(), schema_org::property::kEpisode);
    if (!convert_property.Run(schema_org::property::kPotentialAction,
                              !has_embedded_action,
                              base::BindOnce(&GetActionAndStatus))) {
      continue;
    }

    if (!convert_property.Run(schema_org::property::kInteractionStatistic,
                              false,
                              base::BindOnce(&GetInteractionStatistics))) {
      continue;
    }

    if (!convert_property.Run(schema_org::property::kContentRating, false,
                              base::BindOnce(&GetContentRatings))) {
      continue;
    }

    auto* genre = GetProperty(item.get(), schema_org::property::kGenre);
    if (genre) {
      if (!IsNonEmptyString(*genre))
        continue;
      for (const auto& genre_value : genre->values->string_values) {
        converted_item->genre.push_back(genre_value);
        if (converted_item->genre.size() >= kMaxGenres)
          continue;
      }
    }

    if (!convert_property.Run(schema_org::property::kPublication, false,
                              base::BindOnce(&GetLiveDetails))) {
      continue;
    }

    if (!convert_property.Run(
            schema_org::property::kIdentifier, false,
            base::BindOnce(&GetIdentifiers<mojom::MediaFeedItem>))) {
      continue;
    }

    if (converted_item->type == mojom::MediaFeedItemType::kVideo) {
      if (!convert_property.Run(schema_org::property::kAuthor, true,
                                base::BindOnce(&GetMediaItemAuthor))) {
        continue;
      }
      if (!convert_property.Run(schema_org::property::kDuration,
                                !converted_item->live,
                                base::BindOnce(&GetDuration))) {
        continue;
      }
    }

    if (converted_item->type == mojom::MediaFeedItemType::kTVSeries) {
      auto* num_episodes =
          GetProperty(item.get(), schema_org::property::kNumberOfEpisodes);
      if (!num_episodes || !IsPositiveInteger(*num_episodes))
        continue;
      auto* num_seasons =
          GetProperty(item.get(), schema_org::property::kNumberOfSeasons);
      if (!num_seasons || !IsPositiveInteger(*num_seasons))
        continue;
      if (!convert_property.Run(schema_org::property::kEpisode, false,
                                base::BindOnce(&GetEpisode))) {
        continue;
      }
      if (!convert_property.Run(schema_org::property::kContainsSeason, false,
                                base::BindOnce(&GetSeason))) {
        continue;
      }
    }

    converted_feed_items->push_back(std::move(converted_item));
  }
}

// static
base::Optional<std::vector<mojom::MediaFeedItemPtr>> GetMediaFeeds(
    EntityPtr entity) {
  if (entity->type != "CompleteDataFeed")
    return base::nullopt;

  auto* provider = GetProperty(entity.get(), schema_org::property::kProvider);
  std::string display_name;
  std::vector<media_session::MediaImage> logos;
  if (!ValidateProvider(*provider, &display_name, &logos))
    return base::nullopt;

  std::vector<mojom::MediaFeedItemPtr> media_feed_items;
  auto data_feed_items = std::find_if(
      entity->properties.begin(), entity->properties.end(),
      [](const PropertyPtr& property) {
        return property->name == schema_org::property::kDataFeedElement;
      });
  if (data_feed_items != entity->properties.end() &&
      (*data_feed_items)->values) {
    GetDataFeedItems(*data_feed_items, &media_feed_items);
  }

  return media_feed_items;
}

}  // namespace media_feeds
