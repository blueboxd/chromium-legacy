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
#include "components/schema_org/common/metadata.mojom.h"
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

const std::unordered_set<std::string> kSupportedTypes{
    entity::kVideoObject, entity::kMovie, entity::kTVEpisode, entity::kTVSeason,
    entity::kTVSeries};
bool IsSupportedType(const std::string& type) {
  return kSupportedTypes.find(type) != kSupportedTypes.end();
}

void ExtractEntity(base::DictionaryValue*, mojom::Entity&, int recursionLevel);

bool ParseRepeatedValue(base::Value::ListView& arr,
                        mojom::Values& values,
                        int recursionLevel) {
  if (arr.empty()) {
    return false;
  }

  bool is_first_item = true;
  base::Value::Type type = base::Value::Type::NONE;

  for (size_t j = 0; j < std::min(arr.size(), kMaxRepeatedSize); ++j) {
    auto& listItem = arr[j];
    if (is_first_item) {
      is_first_item = false;
      type = listItem.type();
      switch (type) {
        case base::Value::Type::BOOLEAN:
          values.set_bool_values(std::vector<bool>());
          break;
        case base::Value::Type::INTEGER:
          values.set_long_values(std::vector<int64_t>());
          break;
        case base::Value::Type::DOUBLE:
          // App Indexing doesn't support double type, so just encode its
          // decimal value as a string instead.
          values.set_string_values(std::vector<std::string>());
          break;
        case base::Value::Type::STRING:
          values.set_string_values(std::vector<std::string>());
          break;
        case base::Value::Type::DICTIONARY:
          if (recursionLevel + 1 >= kMaxDepth) {
            return false;
          }
          values.set_entity_values(std::vector<mojom::EntityPtr>());
          break;
        case base::Value::Type::LIST:
          // App Indexing doesn't support nested arrays.
          return false;
        default:
          // Unknown value type.
          return false;
      }
    }

    if (listItem.type() != type) {
      // App Indexing doesn't support mixed types. If there are mixed
      // types in the parsed object, we will drop the property.
      return false;
    }
    switch (listItem.type()) {
      case base::Value::Type::BOOLEAN: {
        bool v;
        listItem.GetAsBoolean(&v);
        values.get_bool_values().push_back(v);
      } break;
      case base::Value::Type::INTEGER: {
        int v = listItem.GetInt();
        values.get_long_values().push_back(v);
      } break;
      case base::Value::Type::DOUBLE: {
        // App Indexing doesn't support double type, so just encode its decimal
        // value as a string instead.
        double v = listItem.GetDouble();
        std::string s = base::NumberToString(v);
        s = s.substr(0, kMaxStringLength);
        values.get_string_values().push_back(s);
      } break;
      case base::Value::Type::STRING: {
        std::string v = listItem.GetString();
        v = v.substr(0, kMaxStringLength);
        values.get_string_values().push_back(v);
      } break;
      case base::Value::Type::DICTIONARY: {
        values.get_entity_values().push_back(mojom::Entity::New());

        base::DictionaryValue* dict_value = nullptr;
        if (listItem.GetAsDictionary(&dict_value)) {
          ExtractEntity(dict_value, *(values.get_entity_values().at(j)),
                        recursionLevel + 1);
        }
      } break;
      default:
        break;
    }
  }
  return true;
}

void ExtractEntity(base::DictionaryValue* val,
                   mojom::Entity& entity,
                   int recursionLevel) {
  if (recursionLevel >= kMaxDepth) {
    return;
  }

  std::string type = "";
  val->GetString(kJSONLDKeyType, &type);
  if (type == "") {
    type = "Thing";
  }
  entity.type = type;
  for (const auto& entry : val->DictItems()) {
    if (entity.properties.size() >= kMaxNumFields) {
      break;
    }
    mojom::PropertyPtr property = mojom::Property::New();
    property->name = entry.first;
    if (property->name == kJSONLDKeyType) {
      continue;
    }
    property->values = mojom::Values::New();

    if (entry.second.is_bool()) {
      bool v;
      val->GetBoolean(entry.first, &v);
      property->values->set_bool_values({v});
    } else if (entry.second.is_int()) {
      int v;
      val->GetInteger(entry.first, &v);
      property->values->set_long_values({v});
    } else if (entry.second.is_double()) {
      double v;
      val->GetDouble(entry.first, &v);
      std::string s = base::NumberToString(v);
      s = s.substr(0, kMaxStringLength);
      property->values->set_string_values({s});
    } else if (entry.second.is_string()) {
      std::string v;
      val->GetString(entry.first, &v);
      v = v.substr(0, kMaxStringLength);
      property->values->set_string_values({v});
    } else if (entry.second.is_dict()) {
      if (recursionLevel + 1 >= kMaxDepth) {
        continue;
      }
      property->values->set_entity_values(std::vector<mojom::EntityPtr>());
      property->values->get_entity_values().push_back(mojom::Entity::New());

      base::DictionaryValue* dict_value = nullptr;
      if (!entry.second.GetAsDictionary(&dict_value)) {
        continue;
      }
      ExtractEntity(dict_value, *(property->values->get_entity_values().at(0)),
                    recursionLevel + 1);
    } else if (entry.second.is_list()) {
      base::Value::ListView list_view = entry.second.GetList();
      if (!ParseRepeatedValue(list_view, *(property->values), recursionLevel)) {
        continue;
      }
    }

    entity.properties.push_back(std::move(property));
  }
}

// Extract a JSONObject which corresponds to a single (possibly nested) entity.
mojom::EntityPtr ExtractTopLevelEntity(base::DictionaryValue* val) {
  mojom::EntityPtr entity = mojom::Entity::New();
  std::string type;
  val->GetString(kJSONLDKeyType, &type);
  if (!IsSupportedType(type)) {
    return nullptr;
  }
  ExtractEntity(val, *entity, 0);
  return entity;
}

}  // namespace

mojom::EntityPtr Extractor::Extract(const std::string& content) {
  base::Optional<base::Value> value(base::JSONReader::Read(content));
  base::DictionaryValue* dict_value = nullptr;

  if (!value || !value.value().GetAsDictionary(&dict_value)) {
    return nullptr;
  }

  return ExtractTopLevelEntity(dict_value);
}

}  // namespace schema_org
