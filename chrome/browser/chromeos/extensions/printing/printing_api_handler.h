// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/extensions/printing/print_job_controller.h"
#include "chrome/browser/chromeos/extensions/printing/print_job_submitter.h"
#include "chrome/browser/chromeos/extensions/printing/printer_capabilities_provider.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/common/extensions/api/printing.h"
#include "chrome/services/printing/public/mojom/pdf_flattener.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router_factory.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
class CupsWrapper;
class Printer;
class PrinterConfigurer;
}  // namespace chromeos

namespace content {
class BrowserContext;
}  // namespace content

namespace printing {
struct PrinterSemanticCapsAndDefaults;
struct PrinterStatus;
}  // namespace printing

namespace extensions {

class PrintJobSubmitter;
class ExtensionRegistry;

// Handles chrome.printing API functions calls, observes CupsPrintJobManager and
// generates OnJobStatusChanged() events of chrome.printing API.
class PrintingAPIHandler : public BrowserContextKeyedAPI,
                           public chromeos::CupsPrintJobManager::Observer {
 public:
  using SubmitJobCallback = base::OnceCallback<void(
      base::Optional<api::printing::SubmitJobStatus> status,
      std::unique_ptr<std::string> job_id,
      base::Optional<std::string> error)>;
  using GetPrinterInfoCallback = base::OnceCallback<void(
      base::Optional<base::Value> capabilities,
      base::Optional<api::printing::PrinterStatus> status,
      base::Optional<std::string> error)>;

  static std::unique_ptr<PrintingAPIHandler> CreateForTesting(
      content::BrowserContext* browser_context,
      EventRouter* event_router,
      ExtensionRegistry* extension_registry,
      chromeos::CupsPrintJobManager* print_job_manager,
      chromeos::CupsPrintersManager* printers_manager,
      std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
      std::unique_ptr<chromeos::CupsWrapper> cups_wrapper);

  explicit PrintingAPIHandler(content::BrowserContext* browser_context);
  ~PrintingAPIHandler() override;

  // BrowserContextKeyedAPI:
  static BrowserContextKeyedAPIFactory<PrintingAPIHandler>*
  GetFactoryInstance();

  // Returns the current instance for |browser_context|.
  static PrintingAPIHandler* Get(content::BrowserContext* browser_context);

  void SubmitJob(const std::string& extension_id,
                 std::unique_ptr<api::printing::SubmitJob::Params> params,
                 PrintJobSubmitter::SubmitJobCallback callback);

  std::vector<api::printing::Printer> GetPrinters();

  void GetPrinterInfo(const std::string& printer_id,
                      GetPrinterInfoCallback callback);

 private:
  // Needed for BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<PrintingAPIHandler>;

  PrintingAPIHandler(
      content::BrowserContext* browser_context,
      EventRouter* event_router,
      ExtensionRegistry* extension_registry,
      chromeos::CupsPrintJobManager* print_job_manager,
      chromeos::CupsPrintersManager* printers_manager,
      std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
      std::unique_ptr<chromeos::CupsWrapper> cups_wrapper);

  // This is needed to save ownership of |print_job_submitter| object which
  // could be destructed because of asynchronous work otherwise.
  void OnPrintJobSubmitted(
      std::unique_ptr<PrintJobSubmitter> print_job_submitter,
      PrintJobSubmitter::SubmitJobCallback callback,
      base::Optional<api::printing::SubmitJobStatus> status,
      std::unique_ptr<std::string> job_id,
      base::Optional<std::string> error);

  void GetPrinterStatus(
      const std::string& printer_id,
      GetPrinterInfoCallback callback,
      base::Optional<printing::PrinterSemanticCapsAndDefaults> capabilities);

  void OnPrinterStatusRetrieved(
      GetPrinterInfoCallback callback,
      base::Value capabilities,
      std::unique_ptr<::printing::PrinterStatus> printer_status);

  // CupsPrintJobManager::Observer:
  void OnPrintJobCreated(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobStarted(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobDone(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobError(base::WeakPtr<chromeos::CupsPrintJob> job) override;
  void OnPrintJobCancelled(base::WeakPtr<chromeos::CupsPrintJob> job) override;

  void DispatchJobStatusChangedEvent(api::printing::JobStatus job_status,
                                     base::WeakPtr<chromeos::CupsPrintJob> job);

  void FinishJob(base::WeakPtr<chromeos::CupsPrintJob> job);

  // BrowserContextKeyedAPI:
  static const bool kServiceIsNULLWhileTesting = true;
  static const char* service_name() { return "PrintingAPIHandler"; }

  content::BrowserContext* const browser_context_;
  EventRouter* const event_router_;
  ExtensionRegistry* const extension_registry_;

  chromeos::CupsPrintJobManager* print_job_manager_;
  chromeos::CupsPrintersManager* const printers_manager_;
  PrintJobController print_job_controller_;
  PrinterCapabilitiesProvider printer_capabilities_provider_;
  std::unique_ptr<chromeos::CupsWrapper> cups_wrapper_;

  // Remote interface used to flatten a PDF.
  mojo::Remote<printing::mojom::PdfFlattener> pdf_flattener_;

  ScopedObserver<chromeos::CupsPrintJobManager,
                 chromeos::CupsPrintJobManager::Observer>
      print_job_manager_observer_;

  base::WeakPtrFactory<PrintingAPIHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintingAPIHandler);
};

template <>
struct BrowserContextFactoryDependencies<PrintingAPIHandler> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<PrintingAPIHandler>* factory) {
    factory->DependsOn(EventRouterFactory::GetInstance());
    factory->DependsOn(chromeos::CupsPrintJobManagerFactory::GetInstance());
    factory->DependsOn(chromeos::CupsPrintersManagerFactory::GetInstance());
  }
};

template <>
KeyedService*
BrowserContextKeyedAPIFactory<PrintingAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINTING_API_HANDLER_H_
