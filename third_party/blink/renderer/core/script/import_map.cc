// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/import_map.h"

#include <memory>
#include <utility>
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
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

static const char kStdScheme[] = "std";

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

// <specdef
// href="https://wicg.github.io/import-maps/#normalize-a-specifier-key">
String NormalizeSpecifierKey(const String& key_string,
                             const KURL& base_url,
                             ConsoleLogger& logger) {
  // <spec step="1">If specifierKey is the empty string, then:</spec>
  if (key_string.IsEmpty()) {
    // <spec step="1.1">Report a warning to the console that specifier keys
    // cannot be the empty string.</spec>
    AddIgnoredKeyMessage(logger, key_string,
                         "specifier keys cannot be the empty string.");

    // <spec step="1.2">Return null.</spec>
    return String();
  }

  // <spec step="2">Let url be the result of parsing a URL-like import
  // specifier, given specifierKey and baseURL.</spec>
  ParsedSpecifier key = ParsedSpecifier::Create(key_string, base_url);

  switch (key.GetType()) {
    case ParsedSpecifier::Type::kInvalid:
    case ParsedSpecifier::Type::kBare:
      // <spec step="4">Return specifierKey.</spec>
      return key_string;

    case ParsedSpecifier::Type::kURL:
      // <spec
      // href="https://wicg.github.io/import-maps/#parse-a-url-like-import-specifier"
      // step="4">If url’s scheme is either a fetch scheme or "std", then return
      // url.</spec>
      //
      // TODO(hiroshige): Perhaps we should move this into ParsedSpecifier.
      if (!SchemeRegistry::IsFetchScheme(key.GetUrl().Protocol()) &&
          key.GetUrl().Protocol() != kStdScheme) {
        // <spec step="4">Return specifierKey.</spec>
        return key_string;
      }
      // <spec step="3">If url is not null, then return the serialization of
      // url.</spec>
      return key.GetImportMapKeyString();
  }
}

// Step 3.3 of
// <specdef
// href="https://wicg.github.io/import-maps/#sort-and-normalize-a-specifier-map">
KURL NormalizeValue(const String& key,
                    const String& value_string,
                    const KURL& base_url,
                    ConsoleLogger& logger) {
  // <spec step="3.3.2">Let addressURL be the result of parsing a URL-like
  // import specifier given potentialAddress and baseURL.</spec>
  ParsedSpecifier value = ParsedSpecifier::Create(value_string, base_url);

  switch (value.GetType()) {
    case ParsedSpecifier::Type::kInvalid:
      // <spec step="3.3.3">If addressURL is null, then:</spec>
      //
      // <spec step="3.3.3.1">Report a warning to the console that the address
      // was invalid.</spec>
      AddIgnoredValueMessage(logger, key, "Invalid URL: " + value_string);
      // <spec step="3.3.1.2">Continue.</spec>
      return NullURL();

    case ParsedSpecifier::Type::kBare:
      // TODO(hiroshige): Switch this into aliasing with std: URLs.
      if (value.GetImportMapKeyString().StartsWith("@std/"))
        return KURL("import:" + value.GetImportMapKeyString());

      // Do not allow bare specifiers except for @std/.
      AddIgnoredValueMessage(logger, key, "Bare specifier: " + value_string);
      return NullURL();

    case ParsedSpecifier::Type::kURL:
      // <spec step="3.3.4">If specifierKey ends with U+002F (/), and the
      // serialization of addressURL does not end with U+002F (/), then:</spec>
      if (key.EndsWith("/") && !value.GetUrl().GetString().EndsWith("/")) {
        // <spec step="3.3.4.1">Report a warning to the console that an invalid
        // address was given for the specifier key specifierKey; since
        // specifierKey ended in a slash, so must the address.</spec>
        AddIgnoredValueMessage(
            logger, key,
            "Since specifierKey ended in a slash, so must the address: " +
                value_string);

        // <spec step="3.3.4.2">Continue.</spec>
        return NullURL();
      }

      // <spec step="3.3.5">Append addressURL to
      // validNormalizedAddresses.</spec>
      return value.GetUrl();
  }
}

}  // namespace

// <specdef
// href="https://wicg.github.io/import-maps/#parse-an-import-map-string">
//
// Parse |text| as an import map. Errors (e.g. json parsing error, invalid
// keys/values, etc.) are basically ignored, except that they are reported to
// the console |logger|.
ImportMap* ImportMap::Parse(const Modulator& modulator,
                            const String& input,
                            const KURL& base_url,
                            ConsoleLogger& logger,
                            ScriptValue* error_to_rethrow) {
  DCHECK(error_to_rethrow);

  // <spec step="1">Let parsed be the result of parsing JSON into Infra values
  // given input.</spec>
  std::unique_ptr<JSONValue> parsed = ParseJSON(input);

  if (!parsed) {
    *error_to_rethrow =
        modulator.CreateSyntaxError("Failed to parse import map: invalid JSON");
    return MakeGarbageCollected<ImportMap>(modulator, SpecifierMap());
  }

  // <spec step="2">If parsed is not a map, then throw a TypeError indicating
  // that the top-level value must be a JSON object.</spec>
  std::unique_ptr<JSONObject> parsed_map = JSONObject::From(std::move(parsed));
  if (!parsed_map) {
    *error_to_rethrow =
        modulator.CreateTypeError("Failed to parse import map: not an object");
    return MakeGarbageCollected<ImportMap>(modulator, SpecifierMap());
  }

  // <spec step="3">Let sortedAndNormalizedImports be an empty map.</spec>
  SpecifierMap sorted_and_normalized_imports;

  // <spec step="4">If parsed["imports"] exists, then:</spec>
  if (parsed_map->Get("imports")) {
    // <spec step="4.1">If parsed["imports"] is not a map, then throw a
    // TypeError indicating that the "imports" top-level key must be a JSON
    // object.</spec>
    JSONObject* imports = parsed_map->GetJSONObject("imports");
    if (!imports) {
      *error_to_rethrow = modulator.CreateTypeError(
          "Failed to parse import map: \"imports\" "
          "top-level key must be a JSON object.");
      return MakeGarbageCollected<ImportMap>(modulator, SpecifierMap());
    }

    // <spec step="4.2">Set sortedAndNormalizedImports to the result of sorting
    // and normalizing a specifier map given parsed["imports"] and
    // baseURL.</spec>
    sorted_and_normalized_imports =
        SortAndNormalizeSpecifierMap(imports, base_url, logger);
  }

  // TODO(crbug.com/927181): Process "scopes" entry (Steps 5 and 6).

  // TODO(hiroshige): Implement Step 7.

  // <spec step="8">Return the import map whose imports are
  // sortedAndNormalizedImports and whose scopes scopes are
  // sortedAndNormalizedScopes.</spec>
  return MakeGarbageCollected<ImportMap>(modulator,
                                         sorted_and_normalized_imports);
}

// <specdef
// href="https://wicg.github.io/import-maps/#sort-and-normalize-a-specifier-map">
ImportMap::SpecifierMap ImportMap::SortAndNormalizeSpecifierMap(
    const JSONObject* imports,
    const KURL& base_url,
    ConsoleLogger& logger) {
  // <spec step="1">Let normalized be an empty map.</spec>
  SpecifierMap normalized;

  // <spec step="2">First, normalize all entries so that their values are lists.
  // For each specifierKey → value of originalMap,</spec>
  for (wtf_size_t i = 0; i < imports->size(); ++i) {
    const JSONObject::Entry& entry = imports->at(i);

    // <spec step="2.1">Let normalizedSpecifierKey be the result of normalizing
    // a specifier key given specifierKey and baseURL.</spec>
    const String normalized_specifier_key =
        NormalizeSpecifierKey(entry.first, base_url, logger);

    // <spec step="2.2">If normalizedSpecifierKey is null, then continue.</spec>
    if (normalized_specifier_key.IsEmpty())
      continue;

    Vector<KURL> values;
    switch (entry.second->GetType()) {
      case JSONValue::ValueType::kTypeNull:
        // <spec step="2.4">Otherwise, if value is null, then set
        // normalized[normalizedSpecifierKey] to a new empty list.</spec>
        break;

      case JSONValue::ValueType::kTypeBoolean:
      case JSONValue::ValueType::kTypeInteger:
      case JSONValue::ValueType::kTypeDouble:
      case JSONValue::ValueType::kTypeObject:
        // <spec step="2.6">Otherwise, report a warning to the console that
        // addresses must be strings, arrays, or null.</spec>
        AddIgnoredValueMessage(logger, entry.first, "Invalid value type.");

        // By continuing here, we leave |normalized[normalized_specifier_key]|
        // unset, and continue processing.
        continue;

      case JSONValue::ValueType::kTypeString: {
        // <spec step="2.3">If value is a string, then set
        // normalized[normalizedSpecifierKey] to «value».</spec>
        String value_string;
        if (!imports->GetString(entry.first, &value_string)) {
          AddIgnoredValueMessage(logger, entry.first,
                                 "Internal error in GetString().");
          break;
        }
        KURL value =
            NormalizeValue(entry.first, value_string, base_url, logger);

        // <spec step="3.3.5">Append addressURL to
        // validNormalizedAddresses.</spec>
        if (value.IsValid())
          values.push_back(value);
        break;
      }

      case JSONValue::ValueType::kTypeArray: {
        // <spec step="2.5">Otherwise, if value is a list, then set
        // normalized[normalizedSpecifierKey] to value.</spec>
        JSONArray* array = imports->GetArray(entry.first);
        if (!array) {
          AddIgnoredValueMessage(logger, entry.first,
                                 "Internal error in GetArray().");
          break;
        }

        // <spec step="3.3">For each potentialAddress of
        // potentialAddresses,</spec>
        for (wtf_size_t j = 0; j < array->size(); ++j) {
          // <spec step="3.3.1">If potentialAddress is not a string,
          // then:</spec>
          String value_string;
          if (!array->at(j)->AsString(&value_string)) {
            // <spec step="3.3.1.1">Report a warning to the console that the
            // contents of address arrays must be strings.</spec>
            AddIgnoredValueMessage(logger, entry.first,
                                   "Non-string in the value.");
            // <spec step="3.3.1.2">Continue.</spec>
            continue;
          }
          KURL value =
              NormalizeValue(entry.first, value_string, base_url, logger);

          // <spec step="3.3.5">Append addressURL to
          // validNormalizedAddresses.</spec>
          if (value.IsValid())
            values.push_back(value);
        }
        break;
      }
    }

    // TODO(hiroshige): Move these checks to resolution time.
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
      } else if (normalized_specifier_key != values[1]) {
        AddIgnoredValueMessage(logger, entry.first,
                               "Fallback URL should match the original URL.");
        values.clear();
      }
    }

    // <spec step="3.4">Set normalized[specifierKey] to
    // validNormalizedAddresses.</spec>
    normalized.Set(normalized_specifier_key, values);
  }

  return normalized;
}

// <specdef href="https://wicg.github.io/import-maps/#resolve-an-imports-match">
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

  // <spec step="1">For each specifierKey → addresses of specifierMap,</spec>
  for (auto it = imports_.begin(); it != imports_.end(); ++it) {
    // <spec step="1.2">If specifierKey ends with U+002F (/) and
    // normalizedSpecifier starts with specifierKey, then:</spec>
    if (!it->key.EndsWith('/'))
      continue;

    if (!key.StartsWith(it->key))
      continue;

    // https://wicg.github.io/import-maps/#longer-or-code-unit-less-than
    // We omit code unit comparison, because there can be at most one
    // prefix-matching entry with the same length.
    if (best_match && it->key.length() < (*best_match)->key.length())
      continue;

    best_match = it;
  }
  return best_match;
}

ImportMap::ImportMap(const Modulator& modulator_for_built_in_modules,
                     const HashMap<String, Vector<KURL>>& imports)
    : imports_(imports),
      modulator_for_built_in_modules_(&modulator_for_built_in_modules) {}

// <specdef href="https://wicg.github.io/import-maps/#resolve-an-imports-match">
base::Optional<KURL> ImportMap::ResolveImportsMatch(
    const ParsedSpecifier& parsed_specifier,
    String* debug_message) const {
  DCHECK(debug_message);
  const String key = parsed_specifier.GetImportMapKeyString();

  // <spec step="1.1">If specifierKey is normalizedSpecifier, then:</spec>
  MatchResult exact = imports_.find(key);
  if (exact != imports_.end()) {
    return ResolveImportsMatchInternal(key, exact, debug_message);
  }

  // Step 1.2.
  if (auto prefix_match = MatchPrefix(parsed_specifier)) {
    return ResolveImportsMatchInternal(key, *prefix_match, debug_message);
  }

  // <spec step="2">Return null.</spec>
  *debug_message = "Import Map: \"" + key +
                   "\" matches with no entries and thus is not mapped.";
  return base::nullopt;
}

// <specdef href="https://wicg.github.io/import-maps/#resolve-an-imports-match">
base::Optional<KURL> ImportMap::ResolveImportsMatchInternal(
    const String& key,
    const MatchResult& matched,
    String* debug_message) const {
  // <spec step="1.2.2.1">Let afterPrefix be the portion of normalizedSpecifier
  // after the initial specifierKey prefix.</spec>
  //
  // <spec step="1.2.3.1">Let afterPrefix be the portion of normalizedSpecifier
  // after the initial specifierKey prefix.</spec>
  const String after_prefix = key.Substring(matched->key.length());

  // <spec step="1.1.3">If addresses’s size is 2, and addresses[0]'s scheme is
  // "std", and addresses[1]'s scheme is not "std", then:</spec>
  //
  // <spec step="1.2.3">If addresses’s size is 2, and addresses[0]'s scheme is
  // "std", and addresses[1]'s scheme is not "std", then:</spec>
  //
  // TODO(hiroshige): Add these checks here. Currently they are checked at
  // parsing.

  for (const KURL& value : matched->value) {
    // <spec step="1.2.2.3">Let url be the result of parsing the concatenation
    // of the serialization of addresses[0] with afterPrefix.</spec>
    //
    // <spec step="1.2.3.3">Let url0 be the result of parsing the concatenation
    // of the serialization of addresses[0] with afterPrefix; similarly, let
    // url1 be the result of parsing the concatenation of the serialization of
    // addresses[1] with afterPrefix.</spec>
    //
    // TODO(hiroshige): Update this according to the spec.
    const KURL url = after_prefix.IsEmpty() ? value : KURL(value, after_prefix);

    // <spec step="1.2.3.5">Return url0, if moduleMap[url0] exists; otherwise,
    // return url1.</spec>
    //
    // Note: Here we filter out non-existing built-in modules in all cases.
    if (layered_api::ResolveFetchingURL(*modulator_for_built_in_modules_, url)
            .IsValid()) {
      *debug_message = "Import Map: \"" + key + "\" matches with \"" +
                       matched->key + "\" and is mapped to " +
                       url.ElidedString();
      return url;
    }
  }

  // <spec step="1.1.1">If addresses’s size is 0, then throw a TypeError
  // indicating that normalizedSpecifier was mapped to no addresses.</spec>
  *debug_message = "Import Map: \"" + key + "\" matches with \"" +
                   matched->key + "\" but fails to be mapped (no viable URLs)";
  return NullURL();
}

String ImportMap::ToString() const {
  StringBuilder builder;
  builder.Append("{");
  bool is_first_key = true;
  for (const auto& it : imports_) {
    if (!is_first_key)
      builder.Append(",");
    is_first_key = false;
    builder.Append("\n  \"");
    builder.Append(it.key);
    builder.Append("\": [");
    bool is_first_value = true;
    for (const auto& v : it.value) {
      if (!is_first_value)
        builder.Append(",");
      is_first_value = false;
      builder.Append("\n    \"");
      builder.Append(v.GetString());
      builder.Append("\"");
    }
    builder.Append("\n  ]");
  }
  builder.Append("\n}\n");
  return builder.ToString();
}

void ImportMap::Trace(Visitor* visitor) {
  visitor->Trace(modulator_for_built_in_modules_);
}

}  // namespace blink
