// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"

#include "base/stl_util.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api_utils.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

PrintingAPIHandler::PrintingAPIHandler(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      event_router_(EventRouter::Get(browser_context)),
      extension_registry_(ExtensionRegistry::Get(browser_context)),
      print_job_manager_(
          chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(
              browser_context)),
      printers_manager_(
          chromeos::CupsPrintersManagerFactory::GetForBrowserContext(
              browser_context)),
      print_job_manager_observer_(this) {
  print_job_manager_observer_.Add(print_job_manager_);
}

PrintingAPIHandler::~PrintingAPIHandler() = default;

// static
BrowserContextKeyedAPIFactory<PrintingAPIHandler>*
PrintingAPIHandler::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<PrintingAPIHandler>>
      instance;
  return instance.get();
}

// static
PrintingAPIHandler* PrintingAPIHandler::Get(
    content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<PrintingAPIHandler>::Get(
      browser_context);
}

std::vector<api::printing::Printer> PrintingAPIHandler::GetPrinters() {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();

  base::Optional<DefaultPrinterRules> default_printer_rules =
      GetDefaultPrinterRules(prefs->GetString(
          prefs::kPrintPreviewDefaultDestinationSelectionRules));

  auto* sticky_settings = printing::PrintPreviewStickySettings::GetInstance();
  sticky_settings->RestoreFromPrefs(prefs);
  base::flat_map<std::string, int> recently_used_ranks =
      sticky_settings->GetPrinterRecentlyUsedRanks();

  static constexpr chromeos::PrinterClass kPrinterClasses[] = {
      chromeos::PrinterClass::kEnterprise,
      chromeos::PrinterClass::kSaved,
      chromeos::PrinterClass::kAutomatic,
  };
  std::vector<api::printing::Printer> all_printers;
  for (auto printer_class : kPrinterClasses) {
    const std::vector<chromeos::Printer>& printers =
        printers_manager_->GetPrinters(printer_class);
    for (const auto& printer : printers) {
      all_printers.emplace_back(
          PrinterToIdl(printer, default_printer_rules, recently_used_ranks));
    }
  }
  return all_printers;
}

void PrintingAPIHandler::OnPrintJobCreated(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_PENDING, job);
}

void PrintingAPIHandler::OnPrintJobStarted(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_IN_PROGRESS, job);
}

void PrintingAPIHandler::OnPrintJobDone(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_PRINTED, job);
}

void PrintingAPIHandler::OnPrintJobError(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_FAILED, job);
}

void PrintingAPIHandler::OnPrintJobCancelled(
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  DispatchJobStatusChangedEvent(api::printing::JOB_STATUS_CANCELED, job);
}

void PrintingAPIHandler::DispatchJobStatusChangedEvent(
    api::printing::JobStatus job_status,
    base::WeakPtr<chromeos::CupsPrintJob> job) {
  if (!job || job->source() != printing::PrintJob::Source::EXTENSION)
    return;

  auto event =
      std::make_unique<Event>(events::PRINTING_ON_JOB_STATUS_CHANGED,
                              api::printing::OnJobStatusChanged::kEventName,
                              api::printing::OnJobStatusChanged::Create(
                                  job->GetUniqueId(), job_status));

  if (extension_registry_->enabled_extensions().Contains(job->source_id()))
    event_router_->DispatchEventToExtension(job->source_id(), std::move(event));
}

template <>
KeyedService*
BrowserContextKeyedAPIFactory<PrintingAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // We do not want an instance of PrintingAPIHandler on the lock screen.
  // This will lead to multiple printing notifications.
  if (chromeos::ProfileHelper::IsLockScreenAppProfile(profile) ||
      chromeos::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }
  return new PrintingAPIHandler(context);
}

}  // namespace extensions
