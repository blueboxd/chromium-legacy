// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/test_cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/test_cups_wrapper.h"
#include "chrome/browser/chromeos/printing/test_printer_configurer.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/api/printing.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class PrintingEventObserver : public TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  PrintingEventObserver(TestEventRouter* event_router,
                        const std::string& event_name)
      : event_router_(event_router), event_name_(event_name) {
    event_router_->AddEventObserver(this);
  }

  ~PrintingEventObserver() override {
    event_router_->RemoveEventObserver(this);
  }

  // TestEventRouter::EventObserver:
  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const Event& event) override {
    if (event.event_name == event_name_) {
      extension_id_ = extension_id;
      event_args_ = event.event_args->Clone();
    }
  }

  const std::string& extension_id() const { return extension_id_; }

  const base::Value& event_args() const { return event_args_; }

 private:
  // Event router this class should observe.
  TestEventRouter* const event_router_;

  // The name of the observed event.
  const std::string event_name_;

  // The extension id passed for the last observed event.
  std::string extension_id_;

  // The arguments passed for the last observed event.
  base::Value event_args_;

  DISALLOW_COPY_AND_ASSIGN(PrintingEventObserver);
};

constexpr char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";
constexpr char kExtensionId2[] = "abcdefghijklmnopqrstuvwxyzaaaaaa";
constexpr char kPrinterId[] = "printer";
constexpr int kJobId = 10;

constexpr char kId1[] = "id1";
constexpr char kId2[] = "id2";
constexpr char kId3[] = "id3";
constexpr char kName[] = "name";
constexpr char kDescription[] = "description";
constexpr char kUri[] = "ipp://1.2.3.4/";

chromeos::Printer ConstructPrinter(const std::string& id,
                                   const std::string& name,
                                   const std::string& description,
                                   const std::string& uri,
                                   chromeos::Printer::Source source) {
  chromeos::Printer printer(id);
  printer.set_display_name(name);
  printer.set_description(description);
  printer.set_uri(uri);
  printer.set_source(source);
  return printer;
}

}  // namespace

class PrintingAPIHandlerUnittest : public testing::Test {
 public:
  PrintingAPIHandlerUnittest()
      : scoped_current_channel_(version_info::Channel::DEV) {}
  ~PrintingAPIHandlerUnittest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ =
        profile_manager_->CreateTestingProfile(chrome::kInitialProfile);

    const char kExtensionName[] = "Printing extension";
    const char kPermissionName[] = "printing";
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(kExtensionName)
            .SetID(kExtensionId)
            .AddPermission(kPermissionName)
            .Build();
    ExtensionRegistry::Get(testing_profile_)->AddEnabled(extension);

    print_job_manager_ =
        std::make_unique<chromeos::TestCupsPrintJobManager>(testing_profile_);
    printers_manager_ = std::make_unique<chromeos::TestCupsPrintersManager>();
    auto cups_wrapper = std::make_unique<chromeos::TestCupsWrapper>();
    cups_wrapper_ = cups_wrapper.get();
    test_backend_ = base::MakeRefCounted<printing::TestPrintBackend>();
    printing::PrintBackend::SetPrintBackendForTesting(test_backend_.get());
    event_router_ = CreateAndUseTestEventRouter(testing_profile_);

    printing_api_handler_ = PrintingAPIHandler::CreateForTesting(
        testing_profile_, event_router_,
        ExtensionRegistry::Get(testing_profile_), print_job_manager_.get(),
        printers_manager_.get(),
        std::make_unique<chromeos::TestPrinterConfigurer>(),
        std::move(cups_wrapper));
  }

  void TearDown() override {
    printing_api_handler_.reset();

    test_backend_.reset();
    printers_manager_.reset();
    print_job_manager_.reset();

    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  void OnPrinterInfoRetrieved(
      base::RepeatingClosure run_loop_closure,
      base::Optional<base::Value> capabilities,
      base::Optional<api::printing::PrinterStatus> printer_status,
      base::Optional<std::string> error) {
    if (capabilities)
      capabilities_ = capabilities.value().Clone();
    else
      capabilities_ = base::nullopt;
    printer_status_ = printer_status;
    error_ = error;
    run_loop_closure.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile* testing_profile_;
  scoped_refptr<printing::TestPrintBackend> test_backend_;
  TestEventRouter* event_router_ = nullptr;
  std::unique_ptr<chromeos::TestCupsPrintJobManager> print_job_manager_;
  std::unique_ptr<chromeos::TestCupsPrintersManager> printers_manager_;
  chromeos::TestCupsWrapper* cups_wrapper_;
  std::unique_ptr<PrintingAPIHandler> printing_api_handler_;
  base::Optional<base::Value> capabilities_;
  base::Optional<api::printing::PrinterStatus> printer_status_;
  base::Optional<std::string> error_;

 private:
  // TODO(crbug.com/992889): Remove this once the API is launched for stable.
  ScopedCurrentChannel scoped_current_channel_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(PrintingAPIHandlerUnittest);
};

// Test that |OnJobStatusChanged| is dispatched when the print job status is
// changed.
TEST_F(PrintingAPIHandlerUnittest, EventIsDispatched) {
  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);

  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::EXTENSION,
          kExtensionId, chromeos::printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());

  EXPECT_EQ(kExtensionId, event_observer.extension_id());
  const base::Value& event_args = event_observer.event_args();
  ASSERT_TRUE(event_args.is_list());
  ASSERT_EQ(2u, event_args.GetList().size());
  base::Value job_id = event_args.GetList()[0].Clone();
  ASSERT_TRUE(job_id.is_string());
  EXPECT_EQ(chromeos::CupsPrintJob::CreateUniqueId(kPrinterId, kJobId),
            job_id.GetString());
  base::Value job_status = event_args.GetList()[1].Clone();
  ASSERT_TRUE(job_status.is_string());
  EXPECT_EQ(api::printing::JOB_STATUS_PENDING,
            api::printing::ParseJobStatus(job_status.GetString()));
}

// Test that |OnJobStatusChanged| is not dispatched if the print job was created
// on Print Preview page.
TEST_F(PrintingAPIHandlerUnittest, PrintPreviewEventIsNotDispatched) {
  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);

  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::PRINT_PREVIEW,
          /*source_id=*/"", chromeos::printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());

  // Check that the print job created on Print Preview doesn't show up.
  EXPECT_EQ("", event_observer.extension_id());
  EXPECT_TRUE(event_observer.event_args().is_none());
}

// Test that calling GetPrinters() returns no printers before any are added to
// the profile.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_NoPrinters) {
  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();
  EXPECT_TRUE(printers.empty());
}

// Test that calling GetPrinters() returns the mock printer.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_OnePrinter) {
  chromeos::Printer printer = ConstructPrinter(kId1, kName, kDescription, kUri,
                                               chromeos::Printer::SRC_POLICY);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);

  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();

  ASSERT_EQ(1u, printers.size());
  const api::printing::Printer& idl_printer = printers[0];

  EXPECT_EQ(kId1, idl_printer.id);
  EXPECT_EQ(kName, idl_printer.name);
  EXPECT_EQ(kDescription, idl_printer.description);
  EXPECT_EQ(kUri, idl_printer.uri);
  EXPECT_EQ(api::printing::PRINTER_SOURCE_POLICY, idl_printer.source);
  EXPECT_FALSE(idl_printer.is_default);
  EXPECT_EQ(nullptr, idl_printer.recently_used_rank);
}

// Test that calling GetPrinters() returns printers of all classes.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_ThreePrinters) {
  chromeos::Printer printer1 = chromeos::Printer(kId1);
  chromeos::Printer printer2 = chromeos::Printer(kId2);
  chromeos::Printer printer3 = chromeos::Printer(kId3);
  printers_manager_->AddPrinter(printer1, chromeos::PrinterClass::kEnterprise);
  printers_manager_->AddPrinter(printer2, chromeos::PrinterClass::kSaved);
  printers_manager_->AddPrinter(printer3, chromeos::PrinterClass::kAutomatic);

  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();

  ASSERT_EQ(3u, printers.size());
  std::vector<std::string> printer_ids;
  for (const auto& printer : printers)
    printer_ids.push_back(printer.id);
  EXPECT_THAT(printer_ids, testing::UnorderedElementsAre(kId1, kId2, kId3));
}

// Test that calling GetPrinters() returns printers with correct |is_default|
// flag.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_IsDefault) {
  testing_profile_->GetPrefs()->SetString(
      prefs::kPrintPreviewDefaultDestinationSelectionRules,
      R"({"kind": "local", "idPattern": "id.*"})");
  chromeos::Printer printer = ConstructPrinter(kId1, kName, kDescription, kUri,
                                               chromeos::Printer::SRC_POLICY);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);

  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();

  ASSERT_EQ(1u, printers.size());
  api::printing::Printer idl_printer = std::move(printers[0]);

  EXPECT_EQ(kId1, idl_printer.id);
  EXPECT_TRUE(idl_printer.is_default);
}

// Test that calling GetPrinters() returns printers with correct
// |recently_used_rank| flag.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_RecentlyUsedRank) {
  printing::PrintPreviewStickySettings* sticky_settings =
      printing::PrintPreviewStickySettings::GetInstance();
  sticky_settings->StoreAppState(R"({
    "version": 2,
    "recentDestinations": [
      {
        "id": "id3"
      },
      {
        "id": "id1"
      }
    ]
  })");
  sticky_settings->SaveInPrefs(testing_profile_->GetPrefs());

  chromeos::Printer printer = ConstructPrinter(kId1, kName, kDescription, kUri,
                                               chromeos::Printer::SRC_POLICY);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);

  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();

  ASSERT_EQ(1u, printers.size());
  api::printing::Printer idl_printer = std::move(printers[0]);

  EXPECT_EQ(kId1, idl_printer.id);
  ASSERT_TRUE(idl_printer.recently_used_rank);
  // The "id1" printer is listed as second printer in the recently used printers
  // list, so we expect 1 as its rank.
  EXPECT_EQ(1, *idl_printer.recently_used_rank);
}

TEST_F(PrintingAPIHandlerUnittest, GetPrinterInfo_InvalidId) {
  base::RunLoop run_loop;
  printing_api_handler_->GetPrinterInfo(
      kPrinterId,
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrinterInfoRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // Printer is not added to CupsPrintersManager, so we expect "Invalid printer
  // id" error.
  EXPECT_FALSE(capabilities_.has_value());
  EXPECT_FALSE(printer_status_.has_value());
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Invalid printer ID", error_.value());
}

TEST_F(PrintingAPIHandlerUnittest, GetPrinterInfo_NoCapabilities) {
  chromeos::Printer printer = chromeos::Printer(kPrinterId);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);
  printers_manager_->InstallPrinter(kPrinterId);

  base::RunLoop run_loop;
  printing_api_handler_->GetPrinterInfo(
      kPrinterId,
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrinterInfoRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(capabilities_.has_value());
  ASSERT_TRUE(printer_status_.has_value());
  EXPECT_EQ(api::printing::PRINTER_STATUS_UNREACHABLE, printer_status_.value());
  EXPECT_FALSE(error_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, GetPrinterInfo) {
  chromeos::Printer printer = chromeos::Printer(kPrinterId);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);

  // Add printer capabilities to |test_backend_|.
  test_backend_->AddValidPrinter(
      kPrinterId, std::make_unique<printing::PrinterSemanticCapsAndDefaults>());

  // Mock CUPS wrapper to return predefined status for given printer.
  printing::PrinterStatus::PrinterReason reason;
  reason.reason = printing::PrinterStatus::PrinterReason::MEDIA_EMPTY;
  cups_wrapper_->SetPrinterStatus(kPrinterId, reason);

  base::RunLoop run_loop;
  printing_api_handler_->GetPrinterInfo(
      kPrinterId,
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrinterInfoRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(capabilities_.has_value());
  const base::Value* capabilities_value = capabilities_->FindDictKey("printer");
  ASSERT_TRUE(capabilities_value);

  const base::Value* color = capabilities_value->FindDictKey("color");
  ASSERT_TRUE(color);
  const base::Value* color_options = color->FindListKey("option");
  ASSERT_TRUE(color_options);
  ASSERT_EQ(1u, color_options->GetList().size());
  const std::string* color_type =
      color_options->GetList()[0].FindStringKey("type");
  ASSERT_TRUE(color_type);
  EXPECT_EQ("STANDARD_MONOCHROME", *color_type);

  const base::Value* page_orientation =
      capabilities_value->FindDictKey("page_orientation");
  ASSERT_TRUE(page_orientation);
  const base::Value* page_orientation_options =
      page_orientation->FindListKey("option");
  ASSERT_TRUE(page_orientation_options);
  ASSERT_EQ(3u, page_orientation_options->GetList().size());
  std::vector<std::string> page_orientation_types;
  for (const base::Value& page_orientation_option :
       page_orientation_options->GetList()) {
    const std::string* page_orientation_type =
        page_orientation_option.FindStringKey("type");
    ASSERT_TRUE(page_orientation_type);
    page_orientation_types.push_back(*page_orientation_type);
  }
  EXPECT_THAT(page_orientation_types,
              testing::UnorderedElementsAre("PORTRAIT", "LANDSCAPE", "AUTO"));

  ASSERT_TRUE(printer_status_.has_value());
  EXPECT_EQ(api::printing::PRINTER_STATUS_OUT_OF_PAPER,
            printer_status_.value());
  EXPECT_FALSE(error_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidId) {
  base::Optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, "job_id");

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("No active print job with given ID", error.value());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidId_OtherExtension) {
  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::EXTENSION,
          kExtensionId, chromeos::printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());

  // Try to cancel print job from other extension.
  base::Optional<std::string> error = printing_api_handler_->CancelJob(
      kExtensionId2,
      chromeos::CupsPrintJob::CreateUniqueId(kPrinterId, kJobId));

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("No active print job with given ID", error.value());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidState) {
  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::EXTENSION,
          kExtensionId, chromeos::printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());
  print_job_manager_->CompletePrintJob(print_job.get());

  // Try to cancel already completed print job.
  base::Optional<std::string> error = printing_api_handler_->CancelJob(
      kExtensionId, chromeos::CupsPrintJob::CreateUniqueId(kPrinterId, kJobId));

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("No active print job with given ID", error.value());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob) {
  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::EXTENSION,
          kExtensionId, chromeos::printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());

  base::Optional<std::string> error = printing_api_handler_->CancelJob(
      kExtensionId, chromeos::CupsPrintJob::CreateUniqueId(kPrinterId, kJobId));

  EXPECT_FALSE(error.has_value());
}

}  // namespace extensions
