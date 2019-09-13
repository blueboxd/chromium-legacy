// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_advanced_caps.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend.h"

namespace printing {

namespace {

using AdvancedCapabilities = std::vector<AdvancedCapability>;

using AttributeHandler = base::RepeatingCallback<
    void(const CupsOptionProvider&, const char*, AdvancedCapabilities*)>;

void NoOpHandler(const CupsOptionProvider& printer,
                 const char* attribute_name,
                 AdvancedCapabilities*) {}

void ScalarHandler(const CupsOptionProvider& printer,
                   const char* attribute_name,
                   AdvancedCapabilities* capabilities) {
  capabilities->emplace_back();
  AdvancedCapability& capability = capabilities->back();
  capability.name = attribute_name;
}

void PopulateBooleanCapability(AdvancedCapability* capability,
                               bool default_value) {
  // TODO(crbug.com/964919) Support checkbox in UI instead of making this
  // two-value enum.
  capability->values.emplace_back();
  capability->values.back().name = kOptionFalse;
  capability->values.back().is_default = !default_value;
  capability->values.emplace_back();
  capability->values.back().name = kOptionTrue;
  capability->values.back().is_default = default_value;
}

void BooleanHandler(const CupsOptionProvider& printer,
                    const char* attribute_name,
                    AdvancedCapabilities* capabilities) {
  ipp_attribute_t* attr_default = printer.GetDefaultOptionValue(attribute_name);
  bool default_value = attr_default && ippGetBoolean(attr_default, 0);

  capabilities->emplace_back();
  AdvancedCapability& capability = capabilities->back();
  capability.name = attribute_name;
  PopulateBooleanCapability(&capability, default_value);
}

void KeywordHandler(const CupsOptionProvider& printer,
                    const char* attribute_name,
                    AdvancedCapabilities* capabilities) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attribute_name);
  if (!attr)
    return;

  ipp_attribute_t* attr_default = printer.GetDefaultOptionValue(attribute_name);
  std::string default_value;
  if (attr_default) {
    const char* value = ippGetString(attr_default, 0, nullptr);
    if (value)
      default_value = value;
  }

  capabilities->emplace_back();
  AdvancedCapability& capability = capabilities->back();
  capability.name = attribute_name;
  int num_values = ippGetCount(attr);
  for (int i = 0; i < num_values; i++) {
    const char* value = ippGetString(attr, i, nullptr);
    if (!value)
      continue;

    capability.values.emplace_back();
    capability.values.back().name = value;
    capability.values.back().is_default = default_value == value;
  }
}

void EnumHandler(const CupsOptionProvider& printer,
                 const char* attribute_name,
                 AdvancedCapabilities* capabilities) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attribute_name);
  if (!attr)
    return;

  ipp_attribute_t* attr_default = printer.GetDefaultOptionValue(attribute_name);
  int default_value = attr_default ? ippGetInteger(attr_default, 0) : 0;

  capabilities->emplace_back();
  AdvancedCapability& capability = capabilities->back();
  capability.name = attribute_name;
  int num_values = ippGetCount(attr);
  for (int i = 0; i < num_values; i++) {
    int value = ippGetInteger(attr, i);
    capability.values.emplace_back();
    capability.values.back().name = base::NumberToString(value);
    capability.values.back().is_default = default_value == value;
  }
}

void MultivalueEnumHandler(int none_value,
                           const CupsOptionProvider& printer,
                           const char* attribute_name,
                           AdvancedCapabilities* capabilities) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attribute_name);
  if (!attr)
    return;

  int num_values = ippGetCount(attr);
  for (int i = 0; i < num_values; i++) {
    int value = ippGetInteger(attr, i);
    // Check for 'none' value;
    if (value == none_value)
      continue;

    capabilities->emplace_back();
    AdvancedCapability& capability = capabilities->back();
    capability.name =
        std::string(attribute_name) + "/" + base::NumberToString(value);

    // TODO(crbug.com/964919) Get correct defaults.
    PopulateBooleanCapability(&capability, false);
  }
}

using HandlerMap = std::map<base::StringPiece, AttributeHandler>;

HandlerMap GenerateHandlers() {
  // TODO(crbug.com/964919) Generate from csv.
  HandlerMap result;
  result.emplace("confirmation-sheet-print",
                 base::BindRepeating(&BooleanHandler));
  result.emplace("copies", base::BindRepeating(&NoOpHandler));
  result.emplace("finishings", base::BindRepeating(&MultivalueEnumHandler, 3));
  result.emplace("ipp-attribute-fidelity", base::BindRepeating(BooleanHandler));
  // We don't have a way to release jobs yet.
  result.emplace("job-hold-until", base::BindRepeating(&NoOpHandler));
  result.emplace("job-name", base::BindRepeating(&ScalarHandler));
  result.emplace("job-password", base::BindRepeating(&NoOpHandler));
  result.emplace("job-password-encryption", base::BindRepeating(&NoOpHandler));
  // TODO(crbug.com/964919) Add validation for an int in 1..100 range.
  result.emplace("job-priority", base::BindRepeating(&ScalarHandler));
  // CUPS thinks "job-sheets" is multivalue. RFC 8011 says it isn't.
  result.emplace("job-sheets", base::BindRepeating(&KeywordHandler));
  result.emplace("media", base::BindRepeating(&NoOpHandler));
  result.emplace("media-col", base::BindRepeating(&NoOpHandler));
  result.emplace("multiple-document-handling",
                 base::BindRepeating(&KeywordHandler));
  result.emplace("number-up", base::BindRepeating(&NoOpHandler));
  result.emplace("orientation-requested", base::BindRepeating(&EnumHandler));
  result.emplace("output-bin", base::BindRepeating(&KeywordHandler));
  result.emplace("page-ranges", base::BindRepeating(&NoOpHandler));
  result.emplace("print-color-mode", base::BindRepeating(&NoOpHandler));
  result.emplace("print-quality", base::BindRepeating(&EnumHandler));
  result.emplace("printer-resolution", base::BindRepeating(&NoOpHandler));
  result.emplace("sheet-collate", base::BindRepeating(&NoOpHandler));
  result.emplace("sides", base::BindRepeating(&NoOpHandler));
  return result;
}

void AddAttributes(const CupsOptionProvider& printer,
                   const char* attribute_name,
                   AdvancedCapabilities* options) {
  static const base::NoDestructor<HandlerMap> handlers(GenerateHandlers());

  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attribute_name);
  if (!attr)
    return;

  int num_options = ippGetCount(attr);
  for (int i = 0; i < num_options; i++) {
    const char* option_name = ippGetString(attr, i, nullptr);
    auto it = handlers->find(option_name);
    if (it == handlers->end()) {
      LOG(WARNING) << "Unknown IPP option: " << option_name;
      continue;
    }
    it->second.Run(printer, option_name, options);
  }
}

}  // namespace

void ExtractAdvancedCapabilities(const CupsOptionProvider& printer,
                                 PrinterSemanticCapsAndDefaults* printer_info) {
  AdvancedCapabilities* options = &printer_info->advanced_capabilities;
  AddAttributes(printer, kIppJobAttributes, options);
  AddAttributes(printer, kIppDocumentAttributes, options);
}

}  // namespace printing
