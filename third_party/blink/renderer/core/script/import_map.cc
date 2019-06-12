// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/import_map.h"

#include <memory>
#include <utility>
#include "third_party/blink/renderer/core/script/layered_api.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/parsed_specifier.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// TODO(https://crbug.com/928549): Audit and improve error messages throughout
// this file.

void AddIgnoredKeyMessage(ConsoleLogger& logger,
                          const String& key,
                          const String& reason) {
  logger.AddConsoleMessage(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kWarning,
      "Ignored an import map key \"" + key + "\": " + reason);
}

void AddIgnoredValueMessage(ConsoleLogger& logger,
                            const String& key,
                            const String& reason) {
  logger.AddConsoleMessage(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kWarning,
      "Ignored an import map value of \"" + key + "\": " + reason);
}

// GetKey() and GetValue() returns null String/KURL if the given ParsedSpecifier
// is not valid as keys/values of import maps.
String GetKey(const String& key_string,
              const KURL& base_url,
              ConsoleLogger& logger) {
  ParsedSpecifier key = ParsedSpecifier::Create(key_string, base_url);
  switch (key.GetType()) {
    case ParsedSpecifier::Type::kInvalid:
      AddIgnoredKeyMessage(logger, key_string, "Invalid key (invalid URL)");
      return String();

    case ParsedSpecifier::Type::kBare:
      return key.GetImportMapKeyString();

    case ParsedSpecifier::Type::kURL:
      if (!SchemeRegistry::IsFetchScheme(key.GetUrl().Protocol())) {
        AddIgnoredKeyMessage(logger, key_string,
                             "Invalid key (non-fetch scheme)");
        return String();
      }
      return key.GetImportMapKeyString();
  }
}

KURL GetValue(const String& key,
              const String& value_string,
              const KURL& base_url,
              ConsoleLogger& logger) {
  ParsedSpecifier value = ParsedSpecifier::Create(value_string, base_url);
  switch (value.GetType()) {
    case ParsedSpecifier::Type::kInvalid:
      AddIgnoredValueMessage(logger, key, "Invalid URL: " + value_string);
      return NullURL();

    case ParsedSpecifier::Type::kBare:
      if (value.GetImportMapKeyString().StartsWith("@std/"))
        return KURL("import:" + value.GetImportMapKeyString());

      // Do not allow bare specifiers except for @std/.
      AddIgnoredValueMessage(logger, key, "Bare specifier: " + value_string);
      return NullURL();

    case ParsedSpecifier::Type::kURL:
      return value.GetUrl();
  }
}

}  // namespace

// Parse |text| as an import map.
// Errors (e.g. json parsing error, invalid keys/values, etc.) are basically
// ignored, except that they are reported to the console |logger|.
// TODO(hiroshige): Handle errors in a spec-conformant way once specified.
// https://github.com/WICG/import-maps/issues/100
ImportMap* ImportMap::Create(const Modulator& modulator_for_built_in_modules,
                             const String& text,
                             const KURL& base_url,
                             ConsoleLogger& logger) {
  HashMap<String, Vector<KURL>> modules_map;

  std::unique_ptr<JSONValue> root = ParseJSON(text);
  if (!root) {
    logger.AddConsoleMessage(mojom::ConsoleMessageSource::kOther,
                             mojom::ConsoleMessageLevel::kError,
                             "Failed to parse import map: invalid JSON");
    return MakeGarbageCollected<ImportMap>(modulator_for_built_in_modules,
                                           modules_map);
  }

  std::unique_ptr<JSONObject> root_object = JSONObject::From(std::move(root));
  if (!root_object) {
    logger.AddConsoleMessage(mojom::ConsoleMessageSource::kOther,
                             mojom::ConsoleMessageLevel::kError,
                             "Failed to parse import map: not an object");
    return MakeGarbageCollected<ImportMap>(modulator_for_built_in_modules,
                                           modules_map);
  }

  JSONObject* modules = root_object->GetJSONObject("imports");
  if (!modules) {
    logger.AddConsoleMessage(
        mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kError,
        "Failed to parse import map: no \"imports\" entry.");
    return MakeGarbageCollected<ImportMap>(modulator_for_built_in_modules,
                                           modules_map);
  }

  for (wtf_size_t i = 0; i < modules->size(); ++i) {
    const JSONObject::Entry& entry = modules->at(i);

    const String key = GetKey(entry.first, base_url, logger);
    if (key.IsEmpty())
      continue;

    Vector<KURL> values;
    switch (entry.second->GetType()) {
      case JSONValue::ValueType::kTypeNull:
        break;

      case JSONValue::ValueType::kTypeBoolean:
      case JSONValue::ValueType::kTypeInteger:
      case JSONValue::ValueType::kTypeDouble:
      case JSONValue::ValueType::kTypeObject:
        AddIgnoredValueMessage(logger, entry.first, "Invalid value type.");
        break;

      case JSONValue::ValueType::kTypeString: {
        String value_string;
        if (!modules->GetString(entry.first, &value_string)) {
          AddIgnoredValueMessage(logger, entry.first,
                                 "Internal error in GetString().");
          break;
        }
        KURL value = GetValue(entry.first, value_string, base_url, logger);
        if (value.IsValid())
          values.push_back(value);
        break;
      }

      case JSONValue::ValueType::kTypeArray: {
        JSONArray* array = modules->GetArray(entry.first);
        if (!array) {
          AddIgnoredValueMessage(logger, entry.first,
                                 "Internal error in GetArray().");
          break;
        }

        for (wtf_size_t j = 0; j < array->size(); ++j) {
          String value_string;
          if (!array->at(j)->AsString(&value_string)) {
            AddIgnoredValueMessage(logger, entry.first,
                                   "Non-string in the value.");
            continue;
          }
          KURL value = GetValue(entry.first, value_string, base_url, logger);
          if (value.IsValid())
            values.push_back(value);
        }
        break;
      }
    }

    if (values.size() > 2) {
      AddIgnoredValueMessage(logger, entry.first,
                             "An array of length > 2 is not yet supported.");
      values.clear();
    }
    if (values.size() == 2) {
      if (layered_api::GetBuiltinPath(values[0]).IsNull()) {
        AddIgnoredValueMessage(
            logger, entry.first,
            "Fallback from a non-builtin URL is not yet supported.");
        values.clear();
      } else if (key != values[1]) {
        AddIgnoredValueMessage(logger, entry.first,
                               "Fallback URL should match the original URL.");
        values.clear();
      }
    }

    modules_map.Set(key, values);
  }

  // TODO(crbug.com/927181): Process "scopes" entry.

  return MakeGarbageCollected<ImportMap>(modulator_for_built_in_modules,
                                         modules_map);
}

base::Optional<ImportMap::MatchResult> ImportMap::MatchExact(
    const ParsedSpecifier& parsed_specifier) const {
  const String key = parsed_specifier.GetImportMapKeyString();
  MatchResult exact = imports_.find(key);
  if (exact != imports_.end())
    return exact;
  return base::nullopt;
}

base::Optional<ImportMap::MatchResult> ImportMap::MatchPrefix(
    const ParsedSpecifier& parsed_specifier) const {
  // Do not perform prefix match for non-bare specifiers.
  if (parsed_specifier.GetType() != ParsedSpecifier::Type::kBare)
    return base::nullopt;

  const String key = parsed_specifier.GetImportMapKeyString();

  // Prefix match, i.e. "Packages" via trailing slashes.
  // https://github.com/WICG/import-maps#packages-via-trailing-slashes
  //
  // TODO(hiroshige): optimize this if necessary. See
  // https://github.com/WICG/import-maps/issues/73#issuecomment-439327758
  // for some candidate implementations.

  // "most-specific wins", i.e. when there are multiple matching keys,
  // choose the longest.
  // https://github.com/WICG/import-maps/issues/102
  base::Optional<MatchResult> best_match;

  for (auto it = imports_.begin(); it != imports_.end(); ++it) {
    if (!it->key.EndsWith('/'))
      continue;

    if (!key.StartsWith(it->key))
      continue;

    if (best_match && it->key.length() < (*best_match)->key.length())
      continue;

    best_match = it;
  }
  return best_match;
}

base::Optional<ImportMap::MatchResult> ImportMap::Match(
    const ParsedSpecifier& parsed_specifier) const {
  if (auto exact = MatchExact(parsed_specifier))
    return exact;
  return MatchPrefix(parsed_specifier);
}

ImportMap::ImportMap(const Modulator& modulator_for_built_in_modules,
                     const HashMap<String, Vector<KURL>>& imports)
    : imports_(imports),
      modulator_for_built_in_modules_(&modulator_for_built_in_modules) {}

base::Optional<KURL> ImportMap::Resolve(const ParsedSpecifier& parsed_specifier,
                                        String* debug_message) const {
  DCHECK(debug_message);
  const String key = parsed_specifier.GetImportMapKeyString();

  base::Optional<MatchResult> maybe_matched = Match(parsed_specifier);

  if (!maybe_matched) {
    *debug_message = "Import Map: \"" + key +
                     "\" matches with no entries and thus is not mapped.";
    return base::nullopt;
  }

  MatchResult& matched = *maybe_matched;
  const String postfix = key.Substring(matched->key.length());

  for (const KURL& value : matched->value) {
    const KURL complete_url = postfix.IsEmpty() ? value : KURL(value, postfix);
    if (layered_api::ResolveFetchingURL(*modulator_for_built_in_modules_,
                                        complete_url)
            .IsValid()) {
      *debug_message = "Import Map: \"" + key + "\" matches with \"" +
                       matched->key + "\" and is mapped to " +
                       complete_url.ElidedString();
      return complete_url;
    }
  }

  *debug_message = "Import Map: \"" + key + "\" matches with \"" +
                   matched->key + "\" but fails to be mapped (no viable URLs)";
  return NullURL();
}

String ImportMap::ToString() const {
  StringBuilder builder;
  builder.Append("{\n");
  for (const auto& it : imports_) {
    builder.Append("  \"");
    builder.Append(it.key);
    builder.Append("\": [\n");
    for (const auto& v : it.value) {
      builder.Append("    \"");
      builder.Append(v.GetString());
      builder.Append("\",\n");
    }
    builder.Append("  ]\n");
  }
  builder.Append("}\n");
  return builder.ToString();
}

void ImportMap::Trace(Visitor* visitor) {
  visitor->Trace(modulator_for_built_in_modules_);
}

}  // namespace blink
