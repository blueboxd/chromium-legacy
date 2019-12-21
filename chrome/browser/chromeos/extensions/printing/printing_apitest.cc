// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/test_printer_configurer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kId[] = "id";

std::unique_ptr<KeyedService> BuildTestCupsPrintersManager(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::TestCupsPrintersManager>();
}

}  // namespace

class PrintingApiTest : public ExtensionApiTest {
 public:
  PrintingApiTest() = default;
  ~PrintingApiTest() override = default;

  PrintingApiTest(const PrintingApiTest&) = delete;
  PrintingApiTest& operator=(const PrintingApiTest&) = delete;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::BindRepeating(
                    &PrintingApiTest::OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
    chromeos::PrinterConfigurer::SetPrinterConfigurerForTesting(
        std::make_unique<chromeos::TestPrinterConfigurer>());
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    chromeos::CupsPrintersManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestCupsPrintersManager));
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(PrintingApiTest, GetPrinters) {
  constexpr char kName[] = "name";

  chromeos::Printer printer = chromeos::Printer(kId);
  printer.set_display_name(kName);
  chromeos::TestCupsPrintersManager* printers_manager =
      static_cast<chromeos::TestCupsPrintersManager*>(
          chromeos::CupsPrintersManagerFactory::GetForBrowserContext(
              browser()->profile()));
  printers_manager->AddPrinter(printer, chromeos::PrinterClass::kSaved);

  SetCustomArg(kName);
  ASSERT_TRUE(RunExtensionSubtest("printing", "get_printers.html"));
}

IN_PROC_BROWSER_TEST_F(PrintingApiTest, GetPrinterInfo) {
  chromeos::Printer printer = chromeos::Printer(kId);
  chromeos::TestCupsPrintersManager* printers_manager =
      static_cast<chromeos::TestCupsPrintersManager*>(
          chromeos::CupsPrintersManagerFactory::GetForBrowserContext(
              browser()->profile()));
  printers_manager->AddPrinter(printer, chromeos::PrinterClass::kSaved);

  auto test_backend = base::MakeRefCounted<printing::TestPrintBackend>();
  printing::PrintBackend::SetPrintBackendForTesting(test_backend.get());
  test_backend->AddValidPrinter(
      kId, std::make_unique<printing::PrinterSemanticCapsAndDefaults>());

  ASSERT_TRUE(RunExtensionSubtest("printing", "get_printer_info.html"));
}

}  // namespace extensions
