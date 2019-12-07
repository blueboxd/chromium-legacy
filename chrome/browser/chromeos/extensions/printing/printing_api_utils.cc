// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api_utils.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chromeos/printing/printer_configuration.h"
#include "third_party/re2/src/re2/re2.h"

namespace extensions {

namespace idl = api::printing;

namespace {

constexpr char kLocal[] = "local";
constexpr char kKind[] = "kind";
constexpr char kIdPattern[] = "idPattern";
constexpr char kNamePattern[] = "namePattern";

idl::PrinterSource PrinterSourceToIdl(chromeos::Printer::Source source) {
  switch (source) {
    case chromeos::Printer::Source::SRC_USER_PREFS:
      return idl::PRINTER_SOURCE_USER;
    case chromeos::Printer::Source::SRC_POLICY:
      return idl::PRINTER_SOURCE_POLICY;
  }
  NOTREACHED();
  return idl::PRINTER_SOURCE_USER;
}

bool DoesPrinterMatchDefaultPrinterRules(
    const chromeos::Printer& printer,
    const base::Optional<DefaultPrinterRules>& rules) {
  if (!rules.has_value())
    return false;
  return (rules->kind.empty() || rules->kind == kLocal) &&
         (rules->id_pattern.empty() ||
          RE2::FullMatch(printer.id(), rules->id_pattern)) &&
         (rules->name_pattern.empty() ||
          RE2::FullMatch(printer.display_name(), rules->name_pattern));
}

}  // namespace

base::Optional<DefaultPrinterRules> GetDefaultPrinterRules(
    const std::string& default_destination_selection_rules) {
  if (default_destination_selection_rules.empty())
    return base::nullopt;

  base::Optional<base::Value> default_destination_selection_rules_value =
      base::JSONReader::Read(default_destination_selection_rules);
  if (!default_destination_selection_rules_value)
    return base::nullopt;

  DefaultPrinterRules default_printer_rules;
  const std::string* kind =
      default_destination_selection_rules_value->FindStringKey(kKind);
  if (kind)
    default_printer_rules.kind = *kind;
  const std::string* id_pattern =
      default_destination_selection_rules_value->FindStringKey(kIdPattern);
  if (id_pattern)
    default_printer_rules.id_pattern = *id_pattern;
  const std::string* name_pattern =
      default_destination_selection_rules_value->FindStringKey(kNamePattern);
  if (name_pattern)
    default_printer_rules.name_pattern = *name_pattern;
  return default_printer_rules;
}

idl::Printer PrinterToIdl(
    const chromeos::Printer& printer,
    const base::Optional<DefaultPrinterRules>& default_printer_rules,
    const base::flat_map<std::string, int>& recently_used_ranks) {
  idl::Printer idl_printer;
  idl_printer.id = printer.id();
  idl_printer.name = printer.display_name();
  idl_printer.description = printer.description();
  idl_printer.uri = printer.uri();
  idl_printer.source = PrinterSourceToIdl(printer.source());
  idl_printer.is_default =
      DoesPrinterMatchDefaultPrinterRules(printer, default_printer_rules);
  auto it = recently_used_ranks.find(printer.id());
  if (it != recently_used_ranks.end())
    idl_printer.recently_used_rank = std::make_unique<int>(it->second);
  else
    idl_printer.recently_used_rank = nullptr;
  return idl_printer;
}

}  // namespace extensions
