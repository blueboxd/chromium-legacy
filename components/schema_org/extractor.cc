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
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/schema_org_entity_names.h"

namespace schema_org {

namespace {

// App Indexing enforces a max nesting depth of 5. Our top level message
// corresponds to the WebPage, so this only leaves 4 more levels. We will parse
// entities up to this depth, and ignore any further nesting. If an object at
// the max nesting depth has a property corresponding to an entity, that
// property will be dropped. Note that we will still parse json-ld blocks deeper
// than this, but it won't be passed to App Indexing.
constexpr int kMaxDepth = 5;
// Some strings are very long, and we don't currently use those, so limit string
// length to something reasonable to avoid undue pressure on Icing. Note that
// App Indexing supports strings up to length 20k.
constexpr size_t kMaxStringLength = 200;
// Enforced by App Indexing, so stop processing early if possible.
constexpr size_t kMaxNumFields = 25;
// Enforced by App Indexing, so stop processing early if possible.
constexpr size_t kMaxRepeatedSize = 100;

constexpr char kJSONLDKeyType[] = "@type";

using improved::mojom::Entity;
using improved::mojom::EntityPtr;
using improved::mojom::Property;
using improved::mojom::PropertyPtr;
using improved::mojom::Values;
using improved::mojom::ValuesPtr;

const std::unordered_set<std::string> kSupportedTypes{
    entity::kVideoObject, entity::kMovie, entity::kTVEpisode, entity::kTVSeason,
    entity::kTVSeries};
bool IsSupportedType(const std::string& type) {
  return kSupportedTypes.find(type) != kSupportedTypes.end();
}

void ExtractEntity(const base::DictionaryValue&, Entity*, int recursion_level);

bool ParseRepeatedValue(const base::Value::ConstListView& arr,
                        Values* values,
                        int recursion_level) {
  DCHECK(values);
  if (arr.empty()) {
    return false;
  }

  for (size_t j = 0; j < std::min(arr.size(), kMaxRepeatedSize); ++j) {
    auto& listItem = arr[j];

    switch (listItem.type()) {
      case base::Value::Type::BOOLEAN: {
        bool v;
        listItem.GetAsBoolean(&v);
        values->bool_values.push_back(v);
      } break;
      case base::Value::Type::INTEGER: {
        int v = listItem.GetInt();
        values->long_values.push_back(v);
      } break;
      case base::Value::Type::DOUBLE: {
        // App Indexing doesn't support double type, so just encode its decimal
        // value as a string instead.
        double v = listItem.GetDouble();
        std::string s = base::NumberToString(v);
        s = s.substr(0, kMaxStringLength);
        values->string_values.push_back(s);
      } break;
      case base::Value::Type::STRING: {
        std::string v = listItem.GetString();
        v = v.substr(0, kMaxStringLength);
        values->string_values.push_back(v);
      } break;
      case base::Value::Type::DICTIONARY: {
        const base::DictionaryValue* dict_value = nullptr;
        if (listItem.GetAsDictionary(&dict_value)) {
          auto entity = Entity::New();
          ExtractEntity(*dict_value, entity.get(), recursion_level + 1);
          values->entity_values.push_back(std::move(entity));
        }
      } break;
      case base::Value::Type::LIST:
        // App Indexing doesn't support nested arrays.
        return false;
      default:
        break;
    }
  }
  return true;
}

void ExtractEntity(const base::DictionaryValue& val,
                   Entity* entity,
                   int recursion_level) {
  if (recursion_level >= kMaxDepth) {
    return;
  }

  std::string type = "";
  val.GetString(kJSONLDKeyType, &type);
  if (type == "") {
    type = "Thing";
  }
  entity->type = type;
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

    if (entry.second.is_bool()) {
      bool v;
      val.GetBoolean(entry.first, &v);
      property->values->bool_values.push_back(v);
    } else if (entry.second.is_int()) {
      int v;
      val.GetInteger(entry.first, &v);
      property->values->long_values.push_back(v);
    } else if (entry.second.is_double()) {
      double v;
      val.GetDouble(entry.first, &v);
      std::string s = base::NumberToString(v);
      s = s.substr(0, kMaxStringLength);
      property->values->string_values.push_back(s);
    } else if (entry.second.is_string()) {
      std::string v;
      val.GetString(entry.first, &v);
      v = v.substr(0, kMaxStringLength);
      property->values->string_values.push_back(v);
    } else if (entry.second.is_dict()) {
      if (recursion_level + 1 >= kMaxDepth) {
        continue;
      }
      const base::DictionaryValue* dict_value = nullptr;
      if (!entry.second.GetAsDictionary(&dict_value)) {
        continue;
      }
      auto nested_entity = Entity::New();
      ExtractEntity(*dict_value, nested_entity.get(), recursion_level + 1);
      property->values->entity_values.push_back(std::move(nested_entity));
    } else if (entry.second.is_list()) {
      const auto& list_view = entry.second.GetList();
      if (!ParseRepeatedValue(list_view, property->values.get(),
                              recursion_level)) {
        continue;
      }
    }

    entity->properties.push_back(std::move(property));
  }
}

// Extract a JSONObject which corresponds to a single (possibly nested) entity.
EntityPtr ExtractTopLevelEntity(const base::DictionaryValue& val) {
  EntityPtr entity = Entity::New();
  std::string type;
  val.GetString(kJSONLDKeyType, &type);
  if (!IsSupportedType(type)) {
    return nullptr;
  }
  ExtractEntity(val, entity.get(), 0);
  return entity;
}

}  // namespace

EntityPtr Extractor::Extract(const std::string& content) {
  base::Optional<base::Value> value(base::JSONReader::Read(content));
  const base::DictionaryValue* dict_value = nullptr;

  if (!value || !value.value().GetAsDictionary(&dict_value)) {
    return nullptr;
  }

  return ExtractTopLevelEntity(*dict_value);
}

}  // namespace schema_org
