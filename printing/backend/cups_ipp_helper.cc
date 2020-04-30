// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_helper.h"

#include <cups/cups.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_utils.h"
#include "printing/units.h"

#if defined(OS_CHROMEOS)
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "printing/backend/ipp_handler_map.h"
#include "printing/printing_features.h"
#endif  // defined(OS_CHROMEOS)

namespace printing {

#if defined(OS_CHROMEOS)
constexpr int kPinMinimumLength = 4;
#endif  // defined(OS_CHROMEOS)

namespace {

constexpr double kMMPerInch = 25.4;
constexpr double kCmPerInch = kMMPerInch * 0.1;

struct ColorMap {
  const char* color;
  ColorModel model;
};

struct DuplexMap {
  const char* name;
  mojom::DuplexMode mode;
};

const ColorMap kColorList[]{
    {CUPS_PRINT_COLOR_MODE_COLOR, COLORMODE_COLOR},
    {CUPS_PRINT_COLOR_MODE_MONOCHROME, COLORMODE_MONOCHROME},
};

const DuplexMap kDuplexList[]{
    {CUPS_SIDES_ONE_SIDED, mojom::DuplexMode::kSimplex},
    {CUPS_SIDES_TWO_SIDED_PORTRAIT, mojom::DuplexMode::kLongEdge},
    {CUPS_SIDES_TWO_SIDED_LANDSCAPE, mojom::DuplexMode::kShortEdge},
};

ColorModel ColorModelFromIppColor(base::StringPiece ippColor) {
  for (const ColorMap& color : kColorList) {
    if (ippColor.compare(color.color) == 0) {
      return color.model;
    }
  }

  return UNKNOWN_COLOR_MODEL;
}

mojom::DuplexMode DuplexModeFromIpp(base::StringPiece ipp_duplex) {
  for (const DuplexMap& entry : kDuplexList) {
    if (base::EqualsCaseInsensitiveASCII(ipp_duplex, entry.name))
      return entry.mode;
  }
  return mojom::DuplexMode::kUnknownDuplexMode;
}

ColorModel DefaultColorModel(const CupsOptionProvider& printer) {
  // default color
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppColor);
  if (!attr)
    return UNKNOWN_COLOR_MODEL;

  return ColorModelFromIppColor(ippGetString(attr, 0, nullptr));
}

std::vector<ColorModel> SupportedColorModels(
    const CupsOptionProvider& printer) {
  std::vector<ColorModel> colors;

  std::vector<base::StringPiece> color_modes =
      printer.GetSupportedOptionValueStrings(kIppColor);
  for (base::StringPiece color : color_modes) {
    ColorModel color_model = ColorModelFromIppColor(color);
    if (color_model != UNKNOWN_COLOR_MODEL) {
      colors.push_back(color_model);
    }
  }

  return colors;
}

void ExtractColor(const CupsOptionProvider& printer,
                  PrinterSemanticCapsAndDefaults* printer_info) {
  printer_info->bw_model = UNKNOWN_COLOR_MODEL;
  printer_info->color_model = UNKNOWN_COLOR_MODEL;

  // color and b&w
  std::vector<ColorModel> color_models = SupportedColorModels(printer);
  for (ColorModel color : color_models) {
    switch (color) {
      case COLORMODE_COLOR:
        printer_info->color_model = COLORMODE_COLOR;
        break;
      case COLORMODE_MONOCHROME:
        printer_info->bw_model = COLORMODE_MONOCHROME;
        break;
      default:
        // value not needed
        break;
    }
  }

  // changeable
  printer_info->color_changeable =
      (printer_info->color_model != UNKNOWN_COLOR_MODEL &&
       printer_info->bw_model != UNKNOWN_COLOR_MODEL);

  // default color
  printer_info->color_default = DefaultColorModel(printer) == COLORMODE_COLOR;
}

void ExtractDuplexModes(const CupsOptionProvider& printer,
                        PrinterSemanticCapsAndDefaults* printer_info) {
  std::vector<base::StringPiece> duplex_modes =
      printer.GetSupportedOptionValueStrings(kIppDuplex);
  for (base::StringPiece duplex : duplex_modes) {
    mojom::DuplexMode duplex_mode = DuplexModeFromIpp(duplex);
    if (duplex_mode != mojom::DuplexMode::kUnknownDuplexMode)
      printer_info->duplex_modes.push_back(duplex_mode);
  }
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppDuplex);
  printer_info->duplex_default =
      attr ? DuplexModeFromIpp(ippGetString(attr, 0, nullptr))
           : mojom::DuplexMode::kUnknownDuplexMode;
}

void CopiesRange(const CupsOptionProvider& printer,
                 int* lower_bound,
                 int* upper_bound) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(kIppCopies);
  if (!attr) {
    *lower_bound = -1;
    *upper_bound = -1;
  }

  *lower_bound = ippGetRange(attr, 0, upper_bound);
}

void ExtractCopies(const CupsOptionProvider& printer,
                   PrinterSemanticCapsAndDefaults* printer_info) {
  // copies
  int lower_bound;
  int upper_bound;
  CopiesRange(printer, &lower_bound, &upper_bound);
  printer_info->copies_max =
      (lower_bound != -1 && upper_bound >= 2) ? upper_bound : 1;
}

// Reads resolution from |attr| and puts into |size| in dots per inch.
base::Optional<gfx::Size> GetResolution(ipp_attribute_t* attr, int i) {
  ipp_res_t units;
  int yres;
  int xres = ippGetResolution(attr, i, &yres, &units);
  if (!xres)
    return {};

  switch (units) {
    case IPP_RES_PER_INCH:
      return gfx::Size(xres, yres);
    case IPP_RES_PER_CM:
      return gfx::Size(xres * kCmPerInch, yres * kCmPerInch);
  }
  return {};
}

// Initializes |printer_info.dpis| with available resolutions and
// |printer_info.default_dpi| with default resolution provided by |printer|.
void ExtractResolutions(const CupsOptionProvider& printer,
                        PrinterSemanticCapsAndDefaults* printer_info) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(kIppResolution);
  if (!attr)
    return;

  int num_options = ippGetCount(attr);
  for (int i = 0; i < num_options; i++) {
    base::Optional<gfx::Size> size = GetResolution(attr, i);
    if (size)
      printer_info->dpis.push_back(size.value());
  }
  ipp_attribute_t* def_attr = printer.GetDefaultOptionValue(kIppResolution);
  base::Optional<gfx::Size> size = GetResolution(def_attr, 0);
  if (size)
    printer_info->default_dpi = size.value();
}

PrinterSemanticCapsAndDefaults::Papers SupportedPapers(
    const CupsOptionProvider& printer) {
  std::vector<base::StringPiece> papers =
      printer.GetSupportedOptionValueStrings(kIppMedia);
  PrinterSemanticCapsAndDefaults::Papers parsed_papers;
  parsed_papers.reserve(papers.size());
  for (base::StringPiece paper : papers) {
    PrinterSemanticCapsAndDefaults::Paper parsed = ParsePaper(paper);
    // If a paper fails to parse reasonably, we should avoid propagating
    // it - e.g. CUPS is known to give out empty vendor IDs at times:
    // https://crbug.com/920295#c23
    if (!parsed.display_name.empty()) {
      parsed_papers.push_back(parsed);
    }
  }

  return parsed_papers;
}

bool CollateCapable(const CupsOptionProvider& printer) {
  std::vector<base::StringPiece> values =
      printer.GetSupportedOptionValueStrings(kIppCollate);
  return base::Contains(values, kCollated) &&
         base::Contains(values, kUncollated);
}

bool CollateDefault(const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppCollate);
  if (!attr)
    return false;

  base::StringPiece name = ippGetString(attr, 0, nullptr);
  return name.compare(kCollated) == 0;
}

#if defined(OS_CHROMEOS)
bool PinSupported(const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(kIppPin);
  if (!attr)
    return false;
  int password_maximum_length_supported = ippGetInteger(attr, 0);
  if (password_maximum_length_supported < kPinMinimumLength)
    return false;

  std::vector<base::StringPiece> values =
      printer.GetSupportedOptionValueStrings(kIppPinEncryption);
  return base::Contains(values, kPinEncryptionNone);
}

// Returns the number of IPP attributes added to |caps| (not necessarily in
// 1-to-1 correspondence).
size_t AddAttributes(const CupsOptionProvider& printer,
                     const char* attr_group_name,
                     AdvancedCapabilities* caps) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attr_group_name);
  if (!attr)
    return 0;

  int num_options = ippGetCount(attr);
  static const base::NoDestructor<HandlerMap> handlers(GenerateHandlers());
  std::vector<std::string> unknown_options;
  size_t attr_count = 0;
  for (int i = 0; i < num_options; i++) {
    const char* option_name = ippGetString(attr, i, nullptr);
    auto it = handlers->find(option_name);
    if (it == handlers->end()) {
      unknown_options.emplace_back(option_name);
      continue;
    }

    size_t previous_size = caps->size();
    // Run the handler that adds items to |caps| based on option type.
    it->second.Run(printer, option_name, caps);
    if (caps->size() > previous_size)
      attr_count++;
  }
  if (!unknown_options.empty()) {
    LOG(WARNING) << "Unknown IPP options: "
                 << base::JoinString(unknown_options, ", ");
  }
  return attr_count;
}

void ExtractAdvancedCapabilities(const CupsOptionProvider& printer,
                                 PrinterSemanticCapsAndDefaults* printer_info) {
  AdvancedCapabilities* options = &printer_info->advanced_capabilities;
  size_t attr_count = AddAttributes(printer, kIppJobAttributes, options);
  attr_count += AddAttributes(printer, kIppDocumentAttributes, options);
  base::UmaHistogramCounts1000("Printing.CUPS.IppAttributesCount", attr_count);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

PrinterSemanticCapsAndDefaults::Paper DefaultPaper(
    const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppMedia);
  if (!attr)
    return PrinterSemanticCapsAndDefaults::Paper();

  return ParsePaper(ippGetString(attr, 0, nullptr));
}

void CapsAndDefaultsFromPrinter(const CupsOptionProvider& printer,
                                PrinterSemanticCapsAndDefaults* printer_info) {
  // collate
  printer_info->collate_default = CollateDefault(printer);
  printer_info->collate_capable = CollateCapable(printer);

  // paper
  printer_info->default_paper = DefaultPaper(printer);
  printer_info->papers = SupportedPapers(printer);

#if defined(OS_CHROMEOS)
  printer_info->pin_supported = PinSupported(printer);
  if (base::FeatureList::IsEnabled(printing::features::kAdvancedPpdAttributes))
    ExtractAdvancedCapabilities(printer, printer_info);
#endif  // defined(OS_CHROMEOS)

  ExtractCopies(printer, printer_info);
  ExtractColor(printer, printer_info);
  ExtractDuplexModes(printer, printer_info);
  ExtractResolutions(printer, printer_info);
}

ScopedIppPtr WrapIpp(ipp_t* ipp) {
  return ScopedIppPtr(ipp, &ippDelete);
}

}  //  namespace printing
