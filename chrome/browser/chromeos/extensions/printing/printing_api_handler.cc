// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api_utils.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_wrapper.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "printing/backend/cups_jobs.h"
#include "printing/backend/print_backend.h"

namespace extensions {

namespace {

constexpr char kInvalidPrinterIdError[] = "Invalid printer ID";

}  // namespace

// static
std::unique_ptr<PrintingAPIHandler> PrintingAPIHandler::CreateForTesting(
    content::BrowserContext* browser_context,
    EventRouter* event_router,
    ExtensionRegistry* extension_registry,
    chromeos::CupsPrintJobManager* print_job_manager,
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
    std::unique_ptr<chromeos::CupsWrapper> cups_wrapper) {
  return base::WrapUnique(new PrintingAPIHandler(
      browser_context, event_router, extension_registry, print_job_manager,
      printers_manager, std::move(printer_configurer),
      std::move(cups_wrapper)));
}

PrintingAPIHandler::PrintingAPIHandler(content::BrowserContext* browser_context)
    : PrintingAPIHandler(
          browser_context,
          EventRouter::Get(browser_context),
          ExtensionRegistry::Get(browser_context),
          chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(
              browser_context),
          chromeos::CupsPrintersManagerFactory::GetForBrowserContext(
              browser_context),
          chromeos::PrinterConfigurer::Create(
              Profile::FromBrowserContext(browser_context)),
          chromeos::CupsWrapper::Create()) {}

PrintingAPIHandler::PrintingAPIHandler(
    content::BrowserContext* browser_context,
    EventRouter* event_router,
    ExtensionRegistry* extension_registry,
    chromeos::CupsPrintJobManager* print_job_manager,
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
    std::unique_ptr<chromeos::CupsWrapper> cups_wrapper)
    : browser_context_(browser_context),
      event_router_(event_router),
      extension_registry_(extension_registry),
      print_job_manager_(print_job_manager),
      printers_manager_(printers_manager),
      printer_capabilities_provider_(printers_manager,
                                     std::move(printer_configurer)),
      cups_wrapper_(std::move(cups_wrapper)),
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

void PrintingAPIHandler::GetPrinterInfo(const std::string& printer_id,
                                        GetPrinterInfoCallback callback) {
  if (!printers_manager_->GetPrinter(printer_id)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*capabilities=*/base::nullopt,
                       /*status=*/base::nullopt, kInvalidPrinterIdError));
    return;
  }
  printer_capabilities_provider_.GetPrinterCapabilities(
      printer_id, base::BindOnce(&PrintingAPIHandler::GetPrinterStatus,
                                 weak_ptr_factory_.GetWeakPtr(), printer_id,
                                 std::move(callback)));
}

void PrintingAPIHandler::GetPrinterStatus(
    const std::string& printer_id,
    GetPrinterInfoCallback callback,
    base::Optional<printing::PrinterSemanticCapsAndDefaults> capabilities) {
  if (!capabilities.has_value()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*capabilities=*/base::nullopt,
                       api::printing::PRINTER_STATUS_UNREACHABLE,
                       /*error=*/base::nullopt));
    return;
  }
  base::Value capabilities_value =
      cloud_print::PrinterSemanticCapsAndDefaultsToCdd(capabilities.value());
  cups_wrapper_->QueryCupsPrinterStatus(
      printer_id,
      base::BindOnce(&PrintingAPIHandler::OnPrinterStatusRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(capabilities_value)));
}

void PrintingAPIHandler::OnPrinterStatusRetrieved(
    GetPrinterInfoCallback callback,
    base::Value capabilities,
    std::unique_ptr<::printing::PrinterStatus> printer_status) {
  if (!printer_status) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(capabilities),
                                  api::printing::PRINTER_STATUS_UNREACHABLE,
                                  /*error=*/base::nullopt));
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), std::move(capabilities),
          PrinterStatusToIdl(chromeos::PrinterErrorCodeFromPrinterStatusReasons(
              *printer_status)),
          /*error=*/base::nullopt));
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
