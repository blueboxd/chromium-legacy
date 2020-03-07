// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_helper.h"

#include <cups/ppd.h>
#include <stddef.h>

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/printing_utils.h"
#include "printing/units.h"
#include "url/gurl.h"

using base::EqualsCaseInsensitiveASCII;

namespace printing {

// This section contains helper code for PPD parsing for semantic capabilities.
namespace {

// Function availability can be tested by checking whether its address is not
// nullptr. Weak symbols remove the need for platform specific build flags and
// allow for appropriate CUPS usage on platforms with non-uniform version
// support, namely Linux.
#define WEAK_CUPS_FN(x) extern "C" __attribute__((weak)) decltype(x) x

WEAK_CUPS_FN(httpConnect2);

// Timeout for establishing a CUPS connection.  It is expected that cupsd is
// able to start and respond on all systems within this duration.
constexpr base::TimeDelta kCupsTimeout = base::TimeDelta::FromSeconds(5);

// CUPS default max copies value (taken from default cupsMaxCopies parsing in
// cups/ppd-cache.c).
constexpr int32_t kDefaultMaxCopies = 9999;

constexpr char kColorDevice[] = "ColorDevice";
constexpr char kColorModel[] = "ColorModel";
constexpr char kColorMode[] = "ColorMode";
constexpr char kInk[] = "Ink";
constexpr char kProcessColorModel[] = "ProcessColorModel";
constexpr char kPrintoutMode[] = "PrintoutMode";
constexpr char kDraftGray[] = "Draft.Gray";
constexpr char kHighGray[] = "High.Gray";

constexpr char kDuplex[] = "Duplex";
constexpr char kDuplexNone[] = "None";
constexpr char kDuplexNoTumble[] = "DuplexNoTumble";
constexpr char kDuplexTumble[] = "DuplexTumble";
constexpr char kPageSize[] = "PageSize";

// Brother printer specific options.
constexpr char kBrotherDuplex[] = "BRDuplex";
constexpr char kBrotherMonoColor[] = "BRMonoColor";
constexpr char kBrotherPrintQuality[] = "BRPrintQuality";

// HP printer specific options.
constexpr char kHpColorMode[] = "HPColorMode";
constexpr char kHpColorPrint[] = "ColorPrint";
constexpr char kHpGrayscalePrint[] = "GrayscalePrint";

// Samsung printer specific options.
constexpr char kSamsungColorTrue[] = "True";
constexpr char kSamsungColorFalse[] = "False";

// Sharp printer specific options.
constexpr char kSharpARCMode[] = "ARCMode";
constexpr char kSharpCMColor[] = "CMColor";
constexpr char kSharpCMBW[] = "CMBW";

// Xerox printer specific options.
constexpr char kXeroxXRXColor[] = "XRXColor";
constexpr char kXeroxAutomatic[] = "Automatic";
constexpr char kXeroxBW[] = "BW";

void ParseLpOptions(const base::FilePath& filepath,
                    base::StringPiece printer_name,
                    int* num_options,
                    cups_option_t** options) {
  std::string content;
  if (!base::ReadFileToString(filepath, &content))
    return;

  const char kDest[] = "dest";
  const char kDefault[] = "default";
  const size_t kDestLen = sizeof(kDest) - 1;
  const size_t kDefaultLen = sizeof(kDefault) - 1;

  for (base::StringPiece line : base::SplitStringPiece(
           content, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::StartsWith(line, base::StringPiece(kDefault, kDefaultLen),
                         base::CompareCase::INSENSITIVE_ASCII) &&
        isspace(line[kDefaultLen])) {
      line = line.substr(kDefaultLen);
    } else if (base::StartsWith(line, base::StringPiece(kDest, kDestLen),
                                base::CompareCase::INSENSITIVE_ASCII) &&
               isspace(line[kDestLen])) {
      line = line.substr(kDestLen);
    } else {
      continue;
    }

    line = base::TrimWhitespaceASCII(line, base::TRIM_ALL);
    if (line.empty())
      continue;

    size_t space_found = line.find(' ');
    if (space_found == base::StringPiece::npos)
      continue;

    base::StringPiece name = line.substr(0, space_found);
    if (name.empty())
      continue;

    if (!EqualsCaseInsensitiveASCII(printer_name, name))
      continue;  // This is not the required printer.

    line = line.substr(space_found + 1);
    // Remove extra spaces.
    line = base::TrimWhitespaceASCII(line, base::TRIM_ALL);
    if (line.empty())
      continue;

    // Parse the selected printer custom options. Need to pass a
    // null-terminated string.
    *num_options = cupsParseOptions(line.as_string().c_str(), 0, options);
  }
}

void MarkLpOptions(base::StringPiece printer_name, ppd_file_t* ppd) {
  static constexpr char kSystemLpOptionPath[] = "/etc/cups/lpoptions";
  static constexpr char kUserLpOptionPath[] = ".cups/lpoptions";

  std::vector<base::FilePath> file_locations;
  file_locations.push_back(base::FilePath(kSystemLpOptionPath));
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  file_locations.push_back(base::FilePath(homedir.Append(kUserLpOptionPath)));

  for (const base::FilePath& location : file_locations) {
    int num_options = 0;
    cups_option_t* options = nullptr;
    ParseLpOptions(location, printer_name, &num_options, &options);
    if (num_options > 0 && options) {
      cupsMarkOptions(ppd, num_options, options);
      cupsFreeOptions(num_options, options);
    }
  }
}

void GetDuplexSettings(ppd_file_t* ppd,
                       std::vector<DuplexMode>* duplex_modes,
                       DuplexMode* duplex_default) {
  ppd_choice_t* duplex_choice = ppdFindMarkedChoice(ppd, kDuplex);
  ppd_option_t* option = ppdFindOption(ppd, kDuplex);
  if (!option)
    option = ppdFindOption(ppd, kBrotherDuplex);

  if (!option)
    return;

  if (!duplex_choice)
    duplex_choice = ppdFindChoice(option, option->defchoice);

  if (ppdFindChoice(option, kDuplexNone))
    duplex_modes->push_back(SIMPLEX);

  if (ppdFindChoice(option, kDuplexNoTumble))
    duplex_modes->push_back(LONG_EDGE);

  if (ppdFindChoice(option, kDuplexTumble))
    duplex_modes->push_back(SHORT_EDGE);

  if (!duplex_choice)
    return;

  const char* choice = duplex_choice->choice;
  if (EqualsCaseInsensitiveASCII(choice, kDuplexNone)) {
    *duplex_default = SIMPLEX;
  } else if (EqualsCaseInsensitiveASCII(choice, kDuplexTumble)) {
    *duplex_default = SHORT_EDGE;
  } else {
    *duplex_default = LONG_EDGE;
  }
}

bool GetBasicColorModelSettings(ppd_file_t* ppd,
                                ColorModel* color_model_for_black,
                                ColorModel* color_model_for_color,
                                bool* color_is_default) {
  ppd_option_t* color_model = ppdFindOption(ppd, kColorModel);
  if (!color_model)
    return false;

  if (ppdFindChoice(color_model, kBlack))
    *color_model_for_black = BLACK;
  else if (ppdFindChoice(color_model, kGray))
    *color_model_for_black = GRAY;
  else if (ppdFindChoice(color_model, kGrayscale))
    *color_model_for_black = GRAYSCALE;

  if (ppdFindChoice(color_model, kColor))
    *color_model_for_color = COLOR;
  else if (ppdFindChoice(color_model, kCMYK))
    *color_model_for_color = CMYK;
  else if (ppdFindChoice(color_model, kRGB))
    *color_model_for_color = RGB;
  else if (ppdFindChoice(color_model, kRGBA))
    *color_model_for_color = RGBA;
  else if (ppdFindChoice(color_model, kRGB16))
    *color_model_for_color = RGB16;
  else if (ppdFindChoice(color_model, kCMY))
    *color_model_for_color = CMY;
  else if (ppdFindChoice(color_model, kKCMY))
    *color_model_for_color = KCMY;
  else if (ppdFindChoice(color_model, kCMY_K))
    *color_model_for_color = CMY_K;

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kColorModel);
  if (!marked_choice)
    marked_choice = ppdFindChoice(color_model, color_model->defchoice);

  if (marked_choice) {
    *color_is_default =
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kBlack) &&
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kGray) &&
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kGrayscale);
  }
  return true;
}

bool GetPrintOutModeColorSettings(ppd_file_t* ppd,
                                  ColorModel* color_model_for_black,
                                  ColorModel* color_model_for_color,
                                  bool* color_is_default) {
  ppd_option_t* printout_mode = ppdFindOption(ppd, kPrintoutMode);
  if (!printout_mode)
    return false;

  *color_model_for_color = PRINTOUTMODE_NORMAL;
  *color_model_for_black = PRINTOUTMODE_NORMAL;

  // Check to see if NORMAL_GRAY value is supported by PrintoutMode.
  // If NORMAL_GRAY is not supported, NORMAL value is used to
  // represent grayscale. If NORMAL_GRAY is supported, NORMAL is used to
  // represent color.
  if (ppdFindChoice(printout_mode, kNormalGray))
    *color_model_for_black = PRINTOUTMODE_NORMAL_GRAY;

  // Get the default marked choice to identify the default color setting
  // value.
  ppd_choice_t* printout_mode_choice = ppdFindMarkedChoice(ppd, kPrintoutMode);
  if (!printout_mode_choice) {
    printout_mode_choice =
        ppdFindChoice(printout_mode, printout_mode->defchoice);
  }
  if (printout_mode_choice) {
    if (EqualsCaseInsensitiveASCII(printout_mode_choice->choice, kNormalGray) ||
        EqualsCaseInsensitiveASCII(printout_mode_choice->choice, kHighGray) ||
        EqualsCaseInsensitiveASCII(printout_mode_choice->choice, kDraftGray)) {
      *color_model_for_black = PRINTOUTMODE_NORMAL_GRAY;
      *color_is_default = false;
    }
  }
  return true;
}

bool GetColorModeSettings(ppd_file_t* ppd,
                          ColorModel* color_model_for_black,
                          ColorModel* color_model_for_color,
                          bool* color_is_default) {
  // Samsung printers use "ColorMode" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kColorMode);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kColor) ||
      ppdFindChoice(color_mode_option, kSamsungColorTrue)) {
    *color_model_for_color = COLORMODE_COLOR;
  }

  if (ppdFindChoice(color_mode_option, kMonochrome) ||
      ppdFindChoice(color_mode_option, kSamsungColorFalse)) {
    *color_model_for_black = COLORMODE_MONOCHROME;
  }

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kColorMode);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default =
        EqualsCaseInsensitiveASCII(mode_choice->choice, kColor) ||
        EqualsCaseInsensitiveASCII(mode_choice->choice, kSamsungColorTrue);
  }
  return true;
}

bool GetBrotherColorSettings(ppd_file_t* ppd,
                             ColorModel* color_model_for_black,
                             ColorModel* color_model_for_color,
                             bool* color_is_default) {
  // Some Brother printers use "BRMonoColor" attribute in their PPDs.
  // Some Brother printers use "BRPrintQuality" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kBrotherMonoColor);
  if (!color_mode_option)
    color_mode_option = ppdFindOption(ppd, kBrotherPrintQuality);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kFullColor))
    *color_model_for_color = BROTHER_CUPS_COLOR;
  else if (ppdFindChoice(color_mode_option, kColor))
    *color_model_for_color = BROTHER_BRSCRIPT3_COLOR;

  if (ppdFindChoice(color_mode_option, kMono))
    *color_model_for_black = BROTHER_CUPS_MONO;
  else if (ppdFindChoice(color_mode_option, kBlack))
    *color_model_for_black = BROTHER_BRSCRIPT3_BLACK;

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kColorMode);
  if (!marked_choice) {
    marked_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }
  if (marked_choice) {
    *color_is_default =
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kBlack) &&
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kMono);
  }
  return true;
}

bool GetHPColorSettings(ppd_file_t* ppd,
                        ColorModel* color_model_for_black,
                        ColorModel* color_model_for_color,
                        bool* color_is_default) {
  // Some HP printers use "Color/Color Model" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kColor);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kColor))
    *color_model_for_color = HP_COLOR_COLOR;
  if (ppdFindChoice(color_mode_option, kBlack))
    *color_model_for_black = HP_COLOR_BLACK;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kColorMode);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }
  if (mode_choice) {
    *color_is_default = EqualsCaseInsensitiveASCII(mode_choice->choice, kColor);
  }
  return true;
}

bool GetHPColorModeSettings(ppd_file_t* ppd,
                            ColorModel* color_model_for_black,
                            ColorModel* color_model_for_color,
                            bool* color_is_default) {
  // Some HP printers use "HPColorMode/Mode" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kHpColorMode);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kHpColorPrint))
    *color_model_for_color = HP_COLOR_COLOR;
  if (ppdFindChoice(color_mode_option, kHpGrayscalePrint))
    *color_model_for_black = HP_COLOR_BLACK;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kHpColorMode);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }
  if (mode_choice) {
    *color_is_default =
        EqualsCaseInsensitiveASCII(mode_choice->choice, kHpColorPrint);
  }
  return true;
}

bool GetEpsonInkSettings(ppd_file_t* ppd,
                         ColorModel* color_model_for_black,
                         ColorModel* color_model_for_color,
                         bool* color_is_default) {
  // Epson printers use "Ink" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kInk);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kColor))
    *color_model_for_color = EPSON_INK_COLOR;
  if (ppdFindChoice(color_mode_option, kMono))
    *color_model_for_black = EPSON_INK_MONO;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kInk);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default = EqualsCaseInsensitiveASCII(mode_choice->choice, kColor);
  }
  return true;
}

bool GetSharpARCModeSettings(ppd_file_t* ppd,
                             ColorModel* color_model_for_black,
                             ColorModel* color_model_for_color,
                             bool* color_is_default) {
  // Sharp printers use "ARCMode" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kSharpARCMode);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kSharpCMColor))
    *color_model_for_color = SHARP_ARCMODE_CMCOLOR;
  if (ppdFindChoice(color_mode_option, kSharpCMBW))
    *color_model_for_black = SHARP_ARCMODE_CMBW;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kSharpARCMode);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    // Many Sharp printers use "CMAuto" as the default color mode.
    *color_is_default =
        !EqualsCaseInsensitiveASCII(mode_choice->choice, kSharpCMBW);
  }
  return true;
}

bool GetXeroxColorSettings(ppd_file_t* ppd,
                           ColorModel* color_model_for_black,
                           ColorModel* color_model_for_color,
                           bool* color_is_default) {
  // Some Xerox printers use "XRXColor" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kXeroxXRXColor);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kXeroxAutomatic))
    *color_model_for_color = XEROX_XRXCOLOR_AUTOMATIC;
  if (ppdFindChoice(color_mode_option, kXeroxBW))
    *color_model_for_black = XEROX_XRXCOLOR_BW;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kXeroxXRXColor);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    // Many Xerox printers use "Automatic" as the default color mode.
    *color_is_default =
        !EqualsCaseInsensitiveASCII(mode_choice->choice, kXeroxBW);
  }
  return true;
}

bool GetProcessColorModelSettings(ppd_file_t* ppd,
                                  ColorModel* color_model_for_black,
                                  ColorModel* color_model_for_color,
                                  bool* color_is_default) {
  // Canon printers use "ProcessColorModel" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kProcessColorModel);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kRGB))
    *color_model_for_color = PROCESSCOLORMODEL_RGB;
  else if (ppdFindChoice(color_mode_option, kCMYK))
    *color_model_for_color = PROCESSCOLORMODEL_CMYK;

  if (ppdFindChoice(color_mode_option, kGreyscale))
    *color_model_for_black = PROCESSCOLORMODEL_GREYSCALE;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kProcessColorModel);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default =
        !EqualsCaseInsensitiveASCII(mode_choice->choice, kGreyscale);
  }
  return true;
}

bool GetColorModelSettings(ppd_file_t* ppd,
                           ColorModel* cm_black,
                           ColorModel* cm_color,
                           bool* is_color) {
  bool is_color_device = false;
  ppd_attr_t* attr = ppdFindAttr(ppd, kColorDevice, nullptr);
  if (attr && attr->value)
    is_color_device = ppd->color_device;

  *is_color = is_color_device;
  return (is_color_device &&
          GetBasicColorModelSettings(ppd, cm_black, cm_color, is_color)) ||
         GetPrintOutModeColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetColorModeSettings(ppd, cm_black, cm_color, is_color) ||
         GetHPColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetHPColorModeSettings(ppd, cm_black, cm_color, is_color) ||
         GetBrotherColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetEpsonInkSettings(ppd, cm_black, cm_color, is_color) ||
         GetSharpARCModeSettings(ppd, cm_black, cm_color, is_color) ||
         GetXeroxColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetProcessColorModelSettings(ppd, cm_black, cm_color, is_color);
}

// Default port for IPP print servers.
const int kDefaultIPPServerPort = 631;

}  // namespace

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
HttpConnectionCUPS::HttpConnectionCUPS(const GURL& print_server_url,
                                       http_encryption_t encryption,
                                       bool blocking)
    : http_(nullptr) {
  // If we have an empty url, use default print server.
  if (print_server_url.is_empty())
    return;

  int port = print_server_url.IntPort();
  if (port == url::PORT_UNSPECIFIED)
    port = kDefaultIPPServerPort;

  if (httpConnect2) {
    http_ = httpConnect2(print_server_url.host().c_str(), port,
                         /*addrlist=*/nullptr, AF_UNSPEC, encryption,
                         blocking ? 1 : 0, kCupsTimeout.InMilliseconds(),
                         /*cancel=*/nullptr);
  } else {
    // Continue to use deprecated CUPS calls because because older Linux
    // distribution such as RHEL/CentOS 7 are shipped with CUPS 1.6.
    http_ =
        httpConnectEncrypt(print_server_url.host().c_str(), port, encryption);
  }

  if (!http_) {
    LOG(ERROR) << "CP_CUPS: Failed connecting to print server: "
               << print_server_url;
    return;
  }

  if (!httpConnect2)
    httpBlocking(http_, blocking ? 1 : 0);
}

HttpConnectionCUPS::~HttpConnectionCUPS() {
  if (http_)
    httpClose(http_);
}

http_t* HttpConnectionCUPS::http() {
  return http_;
}

bool ParsePpdCapabilities(base::StringPiece printer_name,
                          base::StringPiece locale,
                          base::StringPiece printer_capabilities,
                          PrinterSemanticCapsAndDefaults* printer_info) {
  base::FilePath ppd_file_path;
  if (!base::CreateTemporaryFile(&ppd_file_path))
    return false;

  int data_size = printer_capabilities.length();
  if (data_size !=
      base::WriteFile(ppd_file_path, printer_capabilities.data(), data_size)) {
    base::DeleteFile(ppd_file_path, false);
    return false;
  }

  ppd_file_t* ppd = ppdOpenFile(ppd_file_path.value().c_str());
  if (!ppd) {
    int line = 0;
    ppd_status_t ppd_status = ppdLastError(&line);
    LOG(ERROR) << "Failed to open PDD file: error " << ppd_status << " at line "
               << line << ", " << ppdErrorString(ppd_status);
    return false;
  }
  ppdMarkDefaults(ppd);
  MarkLpOptions(printer_name, ppd);

  PrinterSemanticCapsAndDefaults caps;
  caps.collate_capable = true;
  caps.collate_default = true;
  // TODO(crbug.com/1027830): Parse PPD for cupsMaxCopies value.
  caps.copies_max = kDefaultMaxCopies;

  GetDuplexSettings(ppd, &caps.duplex_modes, &caps.duplex_default);

  ColorModel cm_black = UNKNOWN_COLOR_MODEL;
  ColorModel cm_color = UNKNOWN_COLOR_MODEL;
  bool is_color = false;
  if (!GetColorModelSettings(ppd, &cm_black, &cm_color, &is_color)) {
    VLOG(1) << "Unknown printer color model";
  }

  caps.color_changeable =
      ((cm_color != UNKNOWN_COLOR_MODEL) && (cm_black != UNKNOWN_COLOR_MODEL) &&
       (cm_color != cm_black));
  caps.color_default = is_color;
  caps.color_model = cm_color;
  caps.bw_model = cm_black;

  if (ppd->num_sizes > 0 && ppd->sizes) {
    VLOG(1) << "Paper list size - " << ppd->num_sizes;
    ppd_option_t* paper_option = ppdFindOption(ppd, kPageSize);
    bool is_default_found = false;
    for (int i = 0; i < ppd->num_sizes; ++i) {
      gfx::Size paper_size_microns(
          ConvertUnit(ppd->sizes[i].width, kPointsPerInch, kMicronsPerInch),
          ConvertUnit(ppd->sizes[i].length, kPointsPerInch, kMicronsPerInch));
      if (!paper_size_microns.IsEmpty()) {
        PrinterSemanticCapsAndDefaults::Paper paper;
        paper.size_um = paper_size_microns;
        paper.vendor_id = ppd->sizes[i].name;
        if (paper_option) {
          ppd_choice_t* paper_choice =
              ppdFindChoice(paper_option, ppd->sizes[i].name);
          // Human readable paper name should be UTF-8 encoded, but some PPDs
          // do not follow this standard.
          if (paper_choice && base::IsStringUTF8(paper_choice->text)) {
            paper.display_name = paper_choice->text;
          }
        }
        caps.papers.push_back(paper);
        if (ppd->sizes[i].marked) {
          caps.default_paper = paper;
          is_default_found = true;
        }
      }
    }
    if (!is_default_found) {
      gfx::Size locale_paper_microns =
          GetDefaultPaperSizeFromLocaleMicrons(locale);
      for (const PrinterSemanticCapsAndDefaults::Paper& paper : caps.papers) {
        // Set epsilon to 500 microns to allow tolerance of rounded paper sizes.
        // While the above utility function returns paper sizes in microns, they
        // are still rounded to the nearest millimeter (1000 microns).
        constexpr int kSizeEpsilon = 500;
        if (SizesEqualWithinEpsilon(paper.size_um, locale_paper_microns,
                                    kSizeEpsilon)) {
          caps.default_paper = paper;
          is_default_found = true;
          break;
        }
      }

      // If no default was set in the PPD or if the locale default is not within
      // the printer's capabilities, select the first on the list.
      if (!is_default_found)
        caps.default_paper = caps.papers[0];
    }
  }

  ppdClose(ppd);
  base::DeleteFile(ppd_file_path, false);

  *printer_info = caps;
  return true;
}

}  // namespace printing
