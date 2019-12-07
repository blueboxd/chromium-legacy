// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_UTILS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "chrome/common/extensions/api/printing.h"

namespace chromeos {
class Printer;
}  // namespace chromeos

namespace extensions {

struct DefaultPrinterRules {
  std::string kind;
  std::string id_pattern;
  std::string name_pattern;
};

// Parses the string containing
// |prefs::kPrintPreviewDefaultDestinationSelectionRules| value and returns
// default printer selection rules in the form declared above.
base::Optional<DefaultPrinterRules> GetDefaultPrinterRules(
    const std::string& default_destination_selection_rules);

api::printing::Printer PrinterToIdl(
    const chromeos::Printer& printer,
    const base::Optional<DefaultPrinterRules>& default_printer_rules,
    const base::flat_map<std::string, int>& recently_used_ranks);

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_UTILS_H_
