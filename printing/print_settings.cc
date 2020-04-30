// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/atomic_sequence_num.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

namespace printing {

namespace {

base::LazyInstance<std::string>::Leaky g_user_agent;

base::Optional<ColorModel> ColorModeToColorModel(int color_mode) {
  if (color_mode < UNKNOWN_COLOR_MODEL || color_mode > COLOR_MODEL_LAST)
    return base::nullopt;
  return static_cast<ColorModel>(color_mode);
}

}  // namespace

void SetAgent(const std::string& user_agent) {
  g_user_agent.Get() = user_agent;
}

const std::string& GetAgent() {
  return g_user_agent.Get();
}

#if defined(USE_CUPS)
void GetColorModelForMode(int color_mode,
                          std::string* color_setting_name,
                          std::string* color_value) {
#if defined(OS_MACOSX)
  constexpr char kCUPSColorMode[] = "ColorMode";
  constexpr char kCUPSColorModel[] = "ColorModel";
  constexpr char kCUPSPrintoutMode[] = "PrintoutMode";
  constexpr char kCUPSProcessColorModel[] = "ProcessColorModel";
  constexpr char kCUPSInk[] = "Ink";
  constexpr char kCUPSBrotherMonoColor[] = "BRMonoColor";
  constexpr char kCUPSBrotherPrintQuality[] = "BRPrintQuality";
  constexpr char kCUPSSharpARCMode[] = "ARCMode";
  constexpr char kCUPSXeroxXRXColor[] = "XRXColor";
#else
  constexpr char kCUPSColorMode[] = "cups-ColorMode";
  constexpr char kCUPSColorModel[] = "cups-ColorModel";
  constexpr char kCUPSPrintoutMode[] = "cups-PrintoutMode";
  constexpr char kCUPSProcessColorModel[] = "cups-ProcessColorModel";
  constexpr char kCUPSInk[] = "cups-Ink";
  constexpr char kCUPSBrotherMonoColor[] = "cups-BRMonoColor";
  constexpr char kCUPSBrotherPrintQuality[] = "cups-BRPrintQuality";
  constexpr char kCUPSSharpARCMode[] = "cups-ARCMode";
  constexpr char kCUPSXeroxXRXColor[] = "cups-XRXColor";
#endif  // defined(OS_MACOSX)

  *color_setting_name = kCUPSColorModel;

  base::Optional<ColorModel> color_model = ColorModeToColorModel(color_mode);
  if (!color_model.has_value()) {
    NOTREACHED();
    return;
  }

  switch (color_model.value()) {
    case UNKNOWN_COLOR_MODEL:
      *color_value = kGrayscale;
      break;
    case GRAY:
      *color_value = kGray;
      break;
    case COLOR:
      *color_value = kColor;
      break;
    case CMYK:
      *color_value = kCMYK;
      break;
    case CMY:
      *color_value = kCMY;
      break;
    case KCMY:
      *color_value = kKCMY;
      break;
    case CMY_K:
      *color_value = kCMY_K;
      break;
    case BLACK:
      *color_value = kBlack;
      break;
    case GRAYSCALE:
      *color_value = kGrayscale;
      break;
    case RGB:
      *color_value = kRGB;
      break;
    case RGB16:
      *color_value = kRGB16;
      break;
    case RGBA:
      *color_value = kRGBA;
      break;
    case COLORMODE_COLOR:
      *color_setting_name = kCUPSColorMode;
      *color_value = kColor;
      break;
    case COLORMODE_MONOCHROME:
      *color_setting_name = kCUPSColorMode;
      *color_value = kMonochrome;
      break;
    case HP_COLOR_COLOR:
      *color_setting_name = kColor;
      *color_value = kColor;
      break;
    case HP_COLOR_BLACK:
      *color_setting_name = kColor;
      *color_value = kBlack;
      break;
    case PRINTOUTMODE_NORMAL:
      *color_setting_name = kCUPSPrintoutMode;
      *color_value = kNormal;
      break;
    case PRINTOUTMODE_NORMAL_GRAY:
      *color_setting_name = kCUPSPrintoutMode;
      *color_value = kNormalGray;
      break;
    case PROCESSCOLORMODEL_CMYK:
      *color_setting_name = kCUPSProcessColorModel;
      *color_value = kCMYK;
      break;
    case PROCESSCOLORMODEL_GREYSCALE:
      *color_setting_name = kCUPSProcessColorModel;
      *color_value = kGreyscale;
      break;
    case PROCESSCOLORMODEL_RGB:
      *color_setting_name = kCUPSProcessColorModel;
      *color_value = kRGB;
      break;
    case BROTHER_CUPS_COLOR:
      *color_setting_name = kCUPSBrotherMonoColor;
      *color_value = kFullColor;
      break;
    case BROTHER_CUPS_MONO:
      *color_setting_name = kCUPSBrotherMonoColor;
      *color_value = kMono;
      break;
    case BROTHER_BRSCRIPT3_COLOR:
      *color_setting_name = kCUPSBrotherPrintQuality;
      *color_value = kColor;
      break;
    case BROTHER_BRSCRIPT3_BLACK:
      *color_setting_name = kCUPSBrotherPrintQuality;
      *color_value = kBlack;
      break;
    case EPSON_INK_COLOR:
      *color_setting_name = kCUPSInk;
      *color_value = kColor;
      break;
    case EPSON_INK_MONO:
      *color_setting_name = kCUPSInk;
      *color_value = kMono;
      break;
    case SHARP_ARCMODE_CMCOLOR:
      *color_setting_name = kCUPSSharpARCMode;
      *color_value = kSharpCMColor;
      break;
    case SHARP_ARCMODE_CMBW:
      *color_setting_name = kCUPSSharpARCMode;
      *color_value = kSharpCMBW;
      break;
    case XEROX_XRXCOLOR_AUTOMATIC:
      *color_setting_name = kCUPSXeroxXRXColor;
      *color_value = kXeroxAutomatic;
      break;
    case XEROX_XRXCOLOR_BW:
      *color_setting_name = kCUPSXeroxXRXColor;
      *color_value = kXeroxBW;
      break;
  }
  // The default case is excluded from the above switch statement to ensure that
  // all ColorModel values are determinantly handled.
}
#endif  // defined(USE_CUPS)

base::Optional<bool> IsColorModelSelected(int color_mode) {
  base::Optional<ColorModel> color_model = ColorModeToColorModel(color_mode);
  if (!color_model.has_value()) {
    NOTREACHED();
    return base::nullopt;
  }

  switch (color_model.value()) {
    case COLOR:
    case CMYK:
    case CMY:
    case KCMY:
    case CMY_K:
    case RGB:
    case RGB16:
    case RGBA:
    case COLORMODE_COLOR:
    case HP_COLOR_COLOR:
    case PRINTOUTMODE_NORMAL:
    case PROCESSCOLORMODEL_CMYK:
    case PROCESSCOLORMODEL_RGB:
    case BROTHER_CUPS_COLOR:
    case BROTHER_BRSCRIPT3_COLOR:
    case EPSON_INK_COLOR:
    case SHARP_ARCMODE_CMCOLOR:
    case XEROX_XRXCOLOR_AUTOMATIC:
      return true;
    case GRAY:
    case BLACK:
    case GRAYSCALE:
    case COLORMODE_MONOCHROME:
    case HP_COLOR_BLACK:
    case PRINTOUTMODE_NORMAL_GRAY:
    case PROCESSCOLORMODEL_GREYSCALE:
    case BROTHER_CUPS_MONO:
    case BROTHER_BRSCRIPT3_BLACK:
    case EPSON_INK_MONO:
    case SHARP_ARCMODE_CMBW:
    case XEROX_XRXCOLOR_BW:
      return false;
    case UNKNOWN_COLOR_MODEL:
      NOTREACHED();
      return base::nullopt;
  }
  // The default case is excluded from the above switch statement to ensure that
  // all ColorModel values are determinantly handled.
}

// Global SequenceNumber used for generating unique cookie values.
static base::AtomicSequenceNumber cookie_seq;

PrintSettings::PrintSettings() {
  Clear();
}

PrintSettings::~PrintSettings() = default;

void PrintSettings::Clear() {
  ranges_.clear();
  selection_only_ = false;
  margin_type_ = DEFAULT_MARGINS;
  title_.clear();
  url_.clear();
  display_header_footer_ = false;
  should_print_backgrounds_ = false;
  collate_ = false;
  color_ = UNKNOWN_COLOR_MODEL;
  copies_ = 0;
  duplex_mode_ = mojom::DuplexMode::kUnknownDuplexMode;
  device_name_.clear();
  requested_media_ = RequestedMedia();
  page_setup_device_units_.Clear();
  dpi_ = gfx::Size();
  scale_factor_ = 1.0f;
  rasterize_pdf_ = false;
  landscape_ = false;
  supports_alpha_blend_ = true;
#if defined(OS_WIN)
  print_text_with_gdi_ = false;
  printer_type_ = PrintSettings::PrinterType::TYPE_NONE;
#endif
  is_modifiable_ = true;
  pages_per_sheet_ = 1;
#if defined(OS_CHROMEOS)
  send_user_info_ = false;
  username_.clear();
  pin_value_.clear();
  advanced_settings_.clear();
#endif  // defined(OS_CHROMEOS)
}

void PrintSettings::SetPrinterPrintableArea(
    const gfx::Size& physical_size_device_units,
    const gfx::Rect& printable_area_device_units,
    bool landscape_needs_flip) {
  int units_per_inch = device_units_per_inch();
  int header_footer_text_height = 0;
  if (display_header_footer_) {
    // Hard-code text_height = 0.5cm = ~1/5 of inch.
    header_footer_text_height = ConvertUnit(kSettingHeaderFooterInterstice,
                                            kPointsPerInch, units_per_inch);
  }

  PageMargins margins;
  bool small_paper_size = false;
  switch (margin_type_) {
    case DEFAULT_MARGINS: {
      // Default margins 1.0cm = ~2/5 of an inch, unless a page dimension is
      // less than 2.54 cm = ~1 inch, in which case set the margins in that
      // dimension to 0.
      static constexpr double kCmInMicrons = 10000;
      int margin_printer_units =
          ConvertUnit(kCmInMicrons, kMicronsPerInch, units_per_inch);
      int min_size_printer_units = units_per_inch;
      margins.header = header_footer_text_height;
      margins.footer = header_footer_text_height;
      if (physical_size_device_units.height() > min_size_printer_units) {
        margins.top = margin_printer_units;
        margins.bottom = margin_printer_units;
      } else {
        margins.top = 0;
        margins.bottom = 0;
        small_paper_size = true;
      }
      if (physical_size_device_units.width() > min_size_printer_units) {
        margins.left = margin_printer_units;
        margins.right = margin_printer_units;
      } else {
        margins.left = 0;
        margins.right = 0;
        small_paper_size = true;
      }
      break;
    }
    case NO_MARGINS:
    case PRINTABLE_AREA_MARGINS: {
      margins.header = 0;
      margins.footer = 0;
      margins.top = 0;
      margins.bottom = 0;
      margins.left = 0;
      margins.right = 0;
      break;
    }
    case CUSTOM_MARGINS: {
      margins.header = 0;
      margins.footer = 0;
      margins.top = ConvertUnitDouble(requested_custom_margins_in_points_.top,
                                      kPointsPerInch, units_per_inch);
      margins.bottom =
          ConvertUnitDouble(requested_custom_margins_in_points_.bottom,
                            kPointsPerInch, units_per_inch);
      margins.left = ConvertUnitDouble(requested_custom_margins_in_points_.left,
                                       kPointsPerInch, units_per_inch);
      margins.right =
          ConvertUnitDouble(requested_custom_margins_in_points_.right,
                            kPointsPerInch, units_per_inch);
      break;
    }
    default: {
      NOTREACHED();
    }
  }

  if ((margin_type_ == DEFAULT_MARGINS ||
       margin_type_ == PRINTABLE_AREA_MARGINS) &&
      !small_paper_size) {
    page_setup_device_units_.SetRequestedMargins(margins);
  } else {
    page_setup_device_units_.ForceRequestedMargins(margins);
  }
  page_setup_device_units_.Init(physical_size_device_units,
                                printable_area_device_units,
                                header_footer_text_height);
  if (landscape_ && landscape_needs_flip)
    page_setup_device_units_.FlipOrientation();
}

void PrintSettings::SetCustomMargins(
    const PageMargins& requested_margins_in_points) {
  requested_custom_margins_in_points_ = requested_margins_in_points;
  margin_type_ = CUSTOM_MARGINS;
}

int PrintSettings::NewCookie() {
  // A cookie of 0 is used to mark a document as unassigned, count from 1.
  return cookie_seq.GetNext() + 1;
}

void PrintSettings::SetOrientation(bool landscape) {
  if (landscape_ != landscape) {
    landscape_ = landscape;
    page_setup_device_units_.FlipOrientation();
  }
}

}  // namespace printing
