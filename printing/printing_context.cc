// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include <utility>

#include "base/logging.h"
#include "printing/page_setup.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_conversion.h"
#include "printing/units.h"

namespace printing {

namespace {
const float kCloudPrintMarginInch = 0.25;
}

PrintingContext::PrintingContext(Delegate* delegate)
    : delegate_(delegate), in_print_job_(false), abort_printing_(false) {
  DCHECK(delegate_);
}

PrintingContext::~PrintingContext() = default;

void PrintingContext::set_margin_type(MarginType type) {
  DCHECK(type != CUSTOM_MARGINS);
  settings_->set_margin_type(type);
}

void PrintingContext::set_is_modifiable(bool is_modifiable) {
  settings_->set_is_modifiable(is_modifiable);
#if defined(OS_WIN)
  settings_->set_print_text_with_gdi(is_modifiable);
#endif
}

const PrintSettings& PrintingContext::settings() const {
  DCHECK(!in_print_job_);
  return *settings_;
}

void PrintingContext::ResetSettingsImpl(bool create_empty) {
  ReleaseContext();

  settings_ = create_empty ? std::make_unique<PrintSettings>() : nullptr;

  in_print_job_ = false;
  abort_printing_ = false;
}

void PrintingContext::ResetSettings() {
  ResetSettingsImpl(true);
}

void PrintingContext::DeleteSettings() {
  ResetSettingsImpl(false);
}

std::unique_ptr<PrintSettings> PrintingContext::ExtractSettings() {
  return std::move(settings_);
}

PrintingContext::Result PrintingContext::OnError() {
  Result result = abort_printing_ ? CANCEL : FAILED;
  DeleteSettings();
  return result;
}

PrintingContext::Result PrintingContext::UsePdfSettings() {
  base::Value pdf_settings(base::Value::Type::DICTIONARY);
  pdf_settings.SetKey(kSettingHeaderFooterEnabled, base::Value(false));
  pdf_settings.SetKey(kSettingShouldPrintBackgrounds, base::Value(false));
  pdf_settings.SetKey(kSettingShouldPrintSelectionOnly, base::Value(false));
  pdf_settings.SetKey(kSettingMarginsType, base::Value(printing::NO_MARGINS));
  pdf_settings.SetKey(kSettingCollate, base::Value(true));
  pdf_settings.SetKey(kSettingCopies, base::Value(1));
  pdf_settings.SetKey(kSettingColor, base::Value(printing::COLOR));
  pdf_settings.SetKey(kSettingDpiHorizontal, base::Value(kPointsPerInch));
  pdf_settings.SetKey(kSettingDpiVertical, base::Value(kPointsPerInch));
  pdf_settings.SetKey(kSettingDuplexMode, base::Value(printing::SIMPLEX));
  pdf_settings.SetKey(kSettingLandscape, base::Value(false));
  pdf_settings.SetKey(kSettingDeviceName, base::Value(""));
  pdf_settings.SetKey(kSettingPrintToPDF, base::Value(true));
  pdf_settings.SetKey(kSettingCloudPrintDialog, base::Value(false));
  pdf_settings.SetKey(kSettingPrintWithPrivet, base::Value(false));
  pdf_settings.SetKey(kSettingPrintWithExtension, base::Value(false));
  pdf_settings.SetKey(kSettingScaleFactor, base::Value(100));
  pdf_settings.SetKey(kSettingRasterizePdf, base::Value(false));
  pdf_settings.SetKey(kSettingPagesPerSheet, base::Value(1));
  return UpdatePrintSettings(std::move(pdf_settings));
}

PrintingContext::Result PrintingContext::UpdatePrintSettings(
    base::Value job_settings) {
  ResetSettings();

  if (!PrintSettingsFromJobSettings(job_settings, settings_.get())) {
    NOTREACHED();
    return OnError();
  }

  base::Optional<bool> print_to_pdf_opt =
      job_settings.FindBoolKey(kSettingPrintToPDF);
  base::Optional<bool> is_cloud_dialog_opt =
      job_settings.FindBoolKey(kSettingCloudPrintDialog);
  base::Optional<bool> print_with_privet_opt =
      job_settings.FindBoolKey(kSettingPrintWithPrivet);
  base::Optional<bool> print_with_extension_opt =
      job_settings.FindBoolKey(kSettingPrintWithExtension);

  if (!print_to_pdf_opt || !is_cloud_dialog_opt || !print_with_privet_opt ||
      !print_with_extension_opt) {
    NOTREACHED();
    return OnError();
  }

  bool print_to_pdf = print_to_pdf_opt.value();
  bool is_cloud_dialog = is_cloud_dialog_opt.value();
  bool print_with_privet = print_with_privet_opt.value();
  bool print_with_extension = print_with_extension_opt.value();

  bool print_to_cloud = job_settings.FindKey(kSettingCloudPrintId) != nullptr;
  bool open_in_external_preview =
      job_settings.FindKey(kSettingOpenPDFInPreview) != nullptr;

  if (!open_in_external_preview &&
      (print_to_pdf || print_to_cloud || is_cloud_dialog || print_with_privet ||
       print_with_extension)) {
    settings_->set_dpi(kDefaultPdfDpi);
    gfx::Size paper_size(GetPdfPaperSizeDeviceUnits());
    if (!settings_->requested_media().size_microns.IsEmpty()) {
      float device_microns_per_device_unit =
          static_cast<float>(kMicronsPerInch) /
          settings_->device_units_per_inch();
      paper_size =
          gfx::Size(settings_->requested_media().size_microns.width() /
                        device_microns_per_device_unit,
                    settings_->requested_media().size_microns.height() /
                        device_microns_per_device_unit);
    }
    gfx::Rect paper_rect(0, 0, paper_size.width(), paper_size.height());
    if (print_to_cloud || print_with_privet) {
      paper_rect.Inset(
          kCloudPrintMarginInch * settings_->device_units_per_inch(),
          kCloudPrintMarginInch * settings_->device_units_per_inch());
    }
    settings_->SetPrinterPrintableArea(paper_size, paper_rect, true);
    return OK;
  }

  return UpdatePrinterSettings(
      open_in_external_preview,
      job_settings.FindBoolKey(kSettingShowSystemDialog).value_or(false),
      job_settings.FindIntKey(kSettingPreviewPageCount).value_or(0));
}

#if defined(OS_CHROMEOS)
PrintingContext::Result PrintingContext::UpdatePrintSettingsFromPOD(
    std::unique_ptr<PrintSettings> job_settings) {
  DeleteSettings();
  settings_ = std::move(job_settings);

  return UpdatePrinterSettings(false /* external_preview */,
                               false /* show_system_dialog */,
                               0 /* page_count is only used on Android */);
}
#endif

}  // namespace printing
