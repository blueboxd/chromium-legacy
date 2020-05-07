// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/schema_org/extractor.h"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/json/json_parser.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/common/time.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "components/schema_org/schema_org_property_configurations.h"

namespace schema_org {

namespace {

// App Indexing enforces a max nesting depth of 5. Our top level message
// corresponds to the WebPage, so this only leaves 4 more levels. We will parse
// entities up to this depth, and ignore any further nesting. If an object at
// the max nesting depth has a property corresponding to an entity, that
// property will be dropped. Note that we will still parse json-ld blocks deeper
// than this, but it won't be passed to App Indexing.
constexpr int kMaxDictionaryDepth = 5;
// Maximum amount of nesting of arrays to support, where 0 is a completely flat
// array.
constexpr int kMaxNestedArrayDepth = 1;
// Some strings are very long, and we don't currently use those, so limit string
// length to something reasonable to avoid undue pressure on Icing. Note that
// App Indexing supports strings up to length 20k.
constexpr size_t kMaxStringLength = 200;
// Enforced by App Indexing, so stop processing early if possible.
constexpr size_t kMaxNumFields = 25;
// Enforced by App Indexing, so stop processing early if possible.
constexpr size_t kMaxRepeatedSize = 100;

constexpr char kJSONLDKeyType[] = "@type";
constexpr char kJSONLDKeyId[] = "@id";

using improved::mojom::Entity;
using improved::mojom::EntityPtr;
using improved::mojom::Property;
using improved::mojom::PropertyPtr;
using improved::mojom::Values;
using improved::mojom::ValuesPtr;

void ExtractEntity(const base::DictionaryValue&, Entity*, int recursion_level);

// Returns true if a property can be of the Duration class type.
bool HasDuration(const property::PropertyConfiguration& config) {
  for (const auto& thing : config.thing_types) {
    GURL thing_url = GURL(thing);
    DCHECK(thing_url.is_valid() && !thing_url.path().empty());
    std::string thing_name = thing_url.path().substr(1);
    if (thing_name == schema_org::entity::kDuration)
      return true;
  }
  return false;
}

// Parses a string into a property value. The string may be parsed as a
// double, date, or time, depending on the types that the property supports.
// If the property supports text, uses the string itself.
bool ParseStringValue(const std::string& property_type,
                      base::StringPiece value,
                      Values* values) {
  value = value.substr(0, kMaxStringLength);

  schema_org::property::PropertyConfiguration prop_config =
      schema_org::property::GetPropertyConfiguration(property_type);
  if (prop_config.number) {
    int64_t l;
    bool parsed_long = base::StringToInt64(value, &l);
    if (parsed_long) {
      values->long_values.push_back(l);
      return true;
    }
    double d;
    bool parsed_double = base::StringToDouble(value, &d);
    if (parsed_double) {
      values->double_values.push_back(d);
      return true;
    }
  }
  if (prop_config.date_time || prop_config.date) {
    base::Time time;
    bool parsed_time = base::Time::FromString(value.data(), &time);
    if (parsed_time) {
      values->date_time_values.push_back(time);
      return true;
    }
  }
  if (prop_config.time) {
    base::Time time_of_day;
    base::Time start_of_day;
    bool parsed_time = base::Time::FromString(
        ("1970-01-01T" + value.as_string()).c_str(), &time_of_day);
    bool parsed_day_start =
        base::Time::FromString("1970-01-01T00:00:00", &start_of_day);
    base::TimeDelta time = time_of_day - start_of_day;
    // The string failed to parse as a DateTime, but did parse as a Time. Use
    // this value instead.
    if (parsed_time && parsed_day_start) {
      values->time_values.push_back(time);
      return true;
    }
  }
  if (prop_config.boolean) {
    if (value == "https://schema.org/True" ||
        value == "http://schema.org/True" || value == "true") {
      values->bool_values.push_back(true);
      return true;
    }
    if (value == "https://schema.org/False" ||
        value == "http://schema.org/False" || value == "false") {
      values->bool_values.push_back(false);
      return true;
    }
  }
  if (HasDuration(prop_config)) {
    auto time = ParseISO8601Duration(value.as_string());
    if (time.has_value()) {
      values->time_values.push_back(time.value());
      return true;
    }
  }
  if (!prop_config.thing_types.empty() || !prop_config.enum_types.empty() ||
      prop_config.url) {
    auto url = GURL(value);
    if (url.is_valid()) {
      values->url_values.push_back(url);
      return true;
    }
  }
  if (!prop_config.thing_types.empty() || !prop_config.enum_types.empty() ||
      prop_config.text) {
    values->string_values.push_back(value.as_string());
    return true;
  }
  return false;
}

bool ParseRepeatedValue(const base::Value::ConstListView& arr,
                        const std::string& property_type,
                        Values* values,
                        int recursion_level,
                        int nested_array_level) {
  DCHECK(values);
  if (arr.empty()) {
    return false;
  }

  if (nested_array_level > kMaxNestedArrayDepth) {
    return false;
  }

  for (size_t j = 0; j < std::min(arr.size(), kMaxRepeatedSize); ++j) {
    auto& list_item = arr[j];
    switch (list_item.type()) {
      case base::Value::Type::BOOLEAN: {
        values->bool_values.push_back(list_item.GetBool());
      } break;
      case base::Value::Type::INTEGER: {
        values->long_values.push_back(list_item.GetInt());
      } break;
      case base::Value::Type::DOUBLE: {
        values->double_values.push_back(list_item.GetDouble());
      } break;
      case base::Value::Type::STRING: {
        base::StringPiece v = list_item.GetString();
        if (!ParseStringValue(property_type, v, values)) {
          return false;
        }
      } break;
      case base::Value::Type::DICTIONARY: {
        const base::DictionaryValue* dict_value = nullptr;
        if (list_item.GetAsDictionary(&dict_value)) {
          auto entity = Entity::New();
          ExtractEntity(*dict_value, entity.get(), recursion_level + 1);
          values->entity_values.push_back(std::move(entity));
        }
      } break;
      case base::Value::Type::LIST: {
        const base::Value::ConstListView list_view = list_item.GetList();
        if (!ParseRepeatedValue(list_view, property_type, values,
                                recursion_level, nested_array_level + 1)) {
          continue;
        }
      } break;
      default:
        break;
    }
  }
  return true;
}

void ExtractEntity(const base::DictionaryValue& val,
                   Entity* entity,
                   int recursion_level) {
  if (recursion_level >= kMaxDictionaryDepth) {
    return;
  }

  std::string type = "";
  val.GetString(kJSONLDKeyType, &type);
  if (type == "") {
    type = "Thing";
  }
  entity->type = type;

  std::string id;
  if (val.GetString(kJSONLDKeyId, &id)) {
    entity->id = id;
  }

  for (const auto& entry : val.DictItems()) {
    if (entity->properties.size() >= kMaxNumFields) {
      break;
    }
    PropertyPtr property = Property::New();
    property->name = entry.first;
    if (property->name == kJSONLDKeyType) {
      continue;
    }
    property->values = Values::New();

    switch (entry.second.type()) {
      case base::Value::Type::BOOLEAN:
        property->values->bool_values.push_back(entry.second.GetBool());
        break;
      case base::Value::Type::INTEGER:
        property->values->long_values.push_back(entry.second.GetInt());
        break;
      case base::Value::Type::DOUBLE:
        property->values->double_values.push_back(entry.second.GetDouble());
        break;
      case base::Value::Type::STRING: {
        base::StringPiece v = entry.second.GetString();
        if (!(ParseStringValue(property->name, v, property->values.get()))) {
          continue;
        }
        break;
      }
      case base::Value::Type::DICTIONARY: {
        if (recursion_level + 1 >= kMaxDictionaryDepth) {
          continue;
        }

        const base::DictionaryValue* dict_value = nullptr;
        if (!entry.second.GetAsDictionary(&dict_value)) {
          continue;
        }

        auto nested_entity = Entity::New();
        ExtractEntity(*dict_value, nested_entity.get(), recursion_level + 1);
        property->values->entity_values.push_back(std::move(nested_entity));
        break;
      }
      case base::Value::Type::LIST: {
        const base::Value::ConstListView list_view = entry.second.GetList();
        if (!ParseRepeatedValue(list_view, property->name,
                                property->values.get(), recursion_level, 0)) {
          continue;
        }
        break;
      }
      default: {
        // Unsupported value type. Skip this property.
        continue;
      }
    }

    entity->properties.push_back(std::move(property));
  }
}

}  // namespace

Extractor::Extractor(base::flat_set<base::StringPiece> supported_entity_types)
    : supported_types_(supported_entity_types) {}

Extractor::~Extractor() = default;

bool Extractor::IsSupportedType(const std::string& type) {
  return supported_types_.find(type) != supported_types_.end();
}

// Extract a JSONObject which corresponds to a single (possibly nested)
// entity.
EntityPtr Extractor::ExtractTopLevelEntity(const base::DictionaryValue& val) {
  EntityPtr entity = Entity::New();
  std::string type;
  val.GetString(kJSONLDKeyType, &type);
  if (!IsSupportedType(type)) {
    return nullptr;
  }
  ExtractEntity(val, entity.get(), 0);
  return entity;
}

void Extractor::Extract(const std::string& content,
                        ExtractorCallback callback) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      content, base::BindOnce(&Extractor::OnJsonParsed,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void Extractor::OnJsonParsed(ExtractorCallback callback,
                             data_decoder::DataDecoder::ValueOrError result) {
  const base::DictionaryValue* dict_value = nullptr;

  if (!result.value || !result.value.value().GetAsDictionary(&dict_value)) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(ExtractTopLevelEntity(*dict_value));
}

}  // namespace schema_org
