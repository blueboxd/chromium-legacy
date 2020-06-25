// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_metadata_parser.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/values.h"

namespace chromeos {

namespace {

// Attempts to
// 1. parse |input| as a Value having Type::DICTIONARY and
// 2. return Value of |key| having a given |target_type| from the same.
//
// Additionally,
// *  this function never returns empty Value objects and
// *  |target_type| must appear in the switch statement below.
base::Optional<base::Value> ParseJsonAndUnnestKey(
    base::StringPiece input,
    base::StringPiece key,
    base::Value::Type target_type) {
  base::Optional<base::Value> parsed = base::JSONReader::Read(input);
  if (!parsed || !parsed->is_dict()) {
    return base::nullopt;
  }

  base::Optional<base::Value> unnested = parsed->ExtractKey(key);
  if (!unnested || unnested->type() != target_type) {
    return base::nullopt;
  }

  bool unnested_is_empty = true;
  switch (target_type) {
    case base::Value::Type::LIST:
      unnested_is_empty = unnested->GetList().empty();
      break;
    case base::Value::Type::DICTIONARY:
      unnested_is_empty = unnested->DictEmpty();
      break;
    default:
      NOTREACHED();
      break;
  }

  if (unnested_is_empty) {
    return base::nullopt;
  }
  return unnested;
}

}  // namespace

base::Optional<std::vector<std::string>> ParseLocales(
    base::StringPiece locales_json) {
  const auto as_value =
      ParseJsonAndUnnestKey(locales_json, "locales", base::Value::Type::LIST);
  if (!as_value.has_value()) {
    return base::nullopt;
  }

  std::vector<std::string> locales;
  for (const auto& iter : as_value.value().GetList()) {
    std::string locale;
    if (!iter.GetAsString(&locale)) {
      continue;
    }
    locales.push_back(locale);
  }

  if (locales.empty()) {
    return base::nullopt;
  }
  return locales;
}

base::Optional<ParsedManufacturers> ParseManufacturers(
    base::StringPiece manufacturers_json) {
  const auto as_value = ParseJsonAndUnnestKey(manufacturers_json, "filesMap",
                                              base::Value::Type::DICTIONARY);
  if (!as_value.has_value()) {
    return base::nullopt;
  }
  ParsedManufacturers manufacturers;
  for (const auto& iter : as_value.value().DictItems()) {
    std::string printers_metadata_basename;
    if (!iter.second.GetAsString(&printers_metadata_basename)) {
      continue;
    }
    manufacturers[iter.first] = printers_metadata_basename;
  }
  if (manufacturers.empty()) {
    return base::nullopt;
  }
  return manufacturers;
}

base::Optional<ParsedPrinters> ParsePrinters(base::StringPiece printers_json) {
  const auto as_value = ParseJsonAndUnnestKey(printers_json, "modelToEmm",
                                              base::Value::Type::DICTIONARY);

  if (!as_value.has_value()) {
    return base::nullopt;
  }
  ParsedPrinters printers;
  for (const auto& iter : as_value.value().DictItems()) {
    std::string printer_effective_make_and_model;
    if (!iter.second.GetAsString(&printer_effective_make_and_model)) {
      continue;
    }
    printers.push_back(
        ParsedPrinter{iter.first, printer_effective_make_and_model});
  }
  if (printers.empty()) {
    return base::nullopt;
  }
  return printers;
}

base::Optional<ParsedReverseIndex> ParseReverseIndex(
    base::StringPiece reverse_index_json) {
  const base::Optional<base::Value> makes_and_models = ParseJsonAndUnnestKey(
      reverse_index_json, "reverseIndex", base::Value::Type::DICTIONARY);
  if (!makes_and_models.has_value() || makes_and_models->DictSize() == 0) {
    return base::nullopt;
  }

  ParsedReverseIndex parsed;
  for (const auto& kv : makes_and_models->DictItems()) {
    if (!kv.second.is_dict()) {
      continue;
    }

    const std::string* manufacturer = kv.second.FindStringKey("manufacturer");
    const std::string* model = kv.second.FindStringKey("model");
    if (manufacturer && model && !manufacturer->empty() && !model->empty()) {
      parsed.insert_or_assign(kv.first,
                              ReverseIndexLeaf{*manufacturer, *model});
    }
  }

  if (parsed.empty()) {
    return base::nullopt;
  }
  return parsed;
}

}  // namespace chromeos
