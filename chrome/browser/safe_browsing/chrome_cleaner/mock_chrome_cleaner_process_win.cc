// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_process_win.h"

#include <stdlib.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

namespace {

using ::chrome_cleaner::mojom::ChromePromptPtr;
using ::chrome_cleaner::mojom::ChromePromptPtrInfo;
using CrashPoint = MockChromeCleanerProcess::CrashPoint;
using ExtensionCleaningFeatureStatus =
    MockChromeCleanerProcess::ExtensionCleaningFeatureStatus;
using ItemsReporting = MockChromeCleanerProcess::ItemsReporting;
using PromptAcceptance = ChromePromptActions::PromptAcceptance;
using UwsFoundStatus = MockChromeCleanerProcess::UwsFoundStatus;

constexpr char kCrashPointSwitch[] = "mock-crash-point";
constexpr char kUwsFoundSwitch[] = "mock-uws-found";
constexpr char kRebootRequiredSwitch[] = "mock-reboot-required";
constexpr char kRegistryKeysReportingSwitch[] = "registry-keys-reporting";
constexpr char kExtensionsReportingSwitch[] = "extensions-reporting";
constexpr char kExpectedUserResponseSwitch[] = "mock-expected-user-response";

scoped_refptr<extensions::Extension> CreateExtension(const base::string16& name,
                                                     const base::string16& id,
                                                     std::string* error) {
  base::DictionaryValue manifest;
  manifest.SetKey("name", base::Value(name));
  manifest.SetKey("version", base::Value("0"));
  manifest.SetKey("manifest_version", base::Value(2));

  return extensions::Extension::Create(base::FilePath(),
                                       extensions::Manifest::INTERNAL, manifest,
                                       0, base::UTF16ToUTF8(id), error);
}

void PromptUserCallback(
    std::unique_ptr<ChromePromptPtr> chrome_prompt_ptr,
    const MockChromeCleanerProcess::Options& options,
    base::OnceClosure quit_closure,
    PromptAcceptance* result_holder,
    chrome_cleaner::mojom::PromptAcceptance prompt_acceptance) {
  // Delete the ChromePromptPtr now that the PromptUser response callback has
  // been received through it.
  chrome_prompt_ptr.reset();

  *result_holder = static_cast<PromptAcceptance>(prompt_acceptance);

  if (options.crash_point() == CrashPoint::kAfterResponseReceived)
    ::exit(MockChromeCleanerProcess::kDeliberateCrashExitCode);

  std::move(quit_closure).Run();
}

void SendScanResults(const MockChromeCleanerProcess::Options& options,
                     ChromePromptPtrInfo prompt_ptr_info,
                     base::OnceClosure quit_closure,
                     PromptAcceptance* result_holder) {
  // This pointer will be deleted by PromptUserCallback.
  auto chrome_prompt_ptr = std::make_unique<ChromePromptPtr>();
  chrome_prompt_ptr->Bind(std::move(prompt_ptr_info));

  if (options.crash_point() == CrashPoint::kAfterRequestSent) {
    // This task is posted to the IPC thread so that it will happen after the
    // request is sent to the parent process and before the response gets
    // handled on the IPC thread.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce([]() {
          ::exit(MockChromeCleanerProcess::kDeliberateCrashExitCode);
        }));
  }

  (*chrome_prompt_ptr)
      ->PromptUser(
          options.files_to_delete(), options.registry_keys(),
          options.extension_ids(),
          base::BindOnce(&PromptUserCallback, std::move(chrome_prompt_ptr),
                         options, std::move(quit_closure),
                         base::Unretained(result_holder)));
}

}  // namespace

const base::char16 MockChromeCleanerProcess::kInstalledExtensionId1[] =
    L"aaaabbbbccccddddeeeeffffgggghhhh";
const base::char16 MockChromeCleanerProcess::kInstalledExtensionName1[] =
    L"Some Extension";
const base::char16 MockChromeCleanerProcess::kInstalledExtensionId2[] =
    L"ababababcdcdcdcdefefefefghghghgh";
const base::char16 MockChromeCleanerProcess::kInstalledExtensionName2[] =
    L"Another Extension";
const base::char16 MockChromeCleanerProcess::kUnknownExtensionId[] =
    L"unexpectedextensionidabcdefghijk";

// static
void MockChromeCleanerProcess::AddMockExtensionsToProfile(Profile* profile) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  scoped_refptr<extensions::Extension> extension;
  std::string error;

  extension =
      CreateExtension(MockChromeCleanerProcess::kInstalledExtensionName1,
                      MockChromeCleanerProcess::kInstalledExtensionId1, &error);
  if (extension && error.empty()) {
    extension_registry->AddEnabled(extension);
  } else {
    LOG(ERROR) << "Error creating mock extension: " << error;
  }

  extension =
      CreateExtension(MockChromeCleanerProcess::kInstalledExtensionName2,
                      MockChromeCleanerProcess::kInstalledExtensionId2, &error);
  if (extension && error.empty()) {
    extension_registry->AddEnabled(extension);
  } else {
    LOG(ERROR) << "Error creating mock extension: " << error;
  }
}

// static
bool MockChromeCleanerProcess::Options::FromCommandLine(
    const base::CommandLine& command_line,
    Options* options) {
  int registry_keys_reporting_int = -1;
  if (!base::StringToInt(
          command_line.GetSwitchValueASCII(kRegistryKeysReportingSwitch),
          &registry_keys_reporting_int) ||
      registry_keys_reporting_int < 0 ||
      registry_keys_reporting_int >=
          static_cast<int>(ItemsReporting::kNumItemsReporting)) {
    return false;
  }

  int extensions_reporting_int = -1;
  if (!base::StringToInt(
          command_line.GetSwitchValueASCII(kExtensionsReportingSwitch),
          &extensions_reporting_int) ||
      extensions_reporting_int < 0 ||
      extensions_reporting_int >=
          static_cast<int>(ItemsReporting::kNumItemsReporting)) {
    return false;
  }

  options->SetReportedResults(
      command_line.HasSwitch(kUwsFoundSwitch),
      static_cast<ItemsReporting>(registry_keys_reporting_int),
      static_cast<ItemsReporting>(extensions_reporting_int));
  options->set_reboot_required(command_line.HasSwitch(kRebootRequiredSwitch));

  if (command_line.HasSwitch(kCrashPointSwitch)) {
    int crash_point_int = 0;
    if (base::StringToInt(command_line.GetSwitchValueASCII(kCrashPointSwitch),
                          &crash_point_int) &&
        crash_point_int >= 0 &&
        crash_point_int < static_cast<int>(CrashPoint::kNumCrashPoints)) {
      options->set_crash_point(static_cast<CrashPoint>(crash_point_int));
    } else {
      return false;
    }
  }

  if (command_line.HasSwitch(kExpectedUserResponseSwitch)) {
    static const std::vector<PromptAcceptance> kValidPromptAcceptanceList{
        PromptAcceptance::UNSPECIFIED,
        PromptAcceptance::ACCEPTED_WITH_LOGS,
        PromptAcceptance::ACCEPTED_WITHOUT_LOGS,
        PromptAcceptance::DENIED,
    };

    int expected_response_int = 0;
    if (!base::StringToInt(
            command_line.GetSwitchValueASCII(kExpectedUserResponseSwitch),
            &expected_response_int)) {
      return false;
    }

    const PromptAcceptance expected_response =
        static_cast<PromptAcceptance>(expected_response_int);
    if (!base::Contains(kValidPromptAcceptanceList, expected_response)) {
      return false;
    }

    options->set_expected_user_response(expected_response);
  }

  return true;
}

MockChromeCleanerProcess::Options::Options() = default;

MockChromeCleanerProcess::Options::Options(const Options& other)
    : files_to_delete_(other.files_to_delete_),
      registry_keys_(other.registry_keys_),
      extension_ids_(other.extension_ids_),
      reboot_required_(other.reboot_required_),
      crash_point_(other.crash_point_),
      registry_keys_reporting_(other.registry_keys_reporting_),
      extensions_reporting_(other.extensions_reporting_),
      expected_user_response_(other.expected_user_response_) {}

MockChromeCleanerProcess::Options& MockChromeCleanerProcess::Options::operator=(
    const Options& other) {
  files_to_delete_ = other.files_to_delete_;
  registry_keys_ = other.registry_keys_;
  extension_ids_ = other.extension_ids_;
  reboot_required_ = other.reboot_required_;
  crash_point_ = other.crash_point_;
  registry_keys_reporting_ = other.registry_keys_reporting_;
  extensions_reporting_ = other.extensions_reporting_;
  expected_user_response_ = other.expected_user_response_;
  return *this;
}

MockChromeCleanerProcess::Options::~Options() {}

void MockChromeCleanerProcess::Options::AddSwitchesToCommandLine(
    base::CommandLine* command_line) const {
  if (!files_to_delete_.empty())
    command_line->AppendSwitch(kUwsFoundSwitch);

  if (reboot_required())
    command_line->AppendSwitch(kRebootRequiredSwitch);

  if (crash_point() != CrashPoint::kNone) {
    command_line->AppendSwitchASCII(
        kCrashPointSwitch,
        base::NumberToString(static_cast<int>(crash_point())));
  }

  command_line->AppendSwitchASCII(
      kRegistryKeysReportingSwitch,
      base::NumberToString(static_cast<int>(registry_keys_reporting())));
  command_line->AppendSwitchASCII(
      kExtensionsReportingSwitch,
      base::NumberToString(static_cast<int>(extensions_reporting())));

  if (expected_user_response() != PromptAcceptance::UNSPECIFIED) {
    command_line->AppendSwitchASCII(
        kExpectedUserResponseSwitch,
        base::NumberToString(static_cast<int>(expected_user_response())));
  }
}

void MockChromeCleanerProcess::Options::SetReportedResults(
    bool has_files_to_remove,
    ItemsReporting registry_keys_reporting,
    ItemsReporting extensions_reporting) {
  if (!has_files_to_remove)
    files_to_delete_.clear();
  if (has_files_to_remove) {
    files_to_delete_.push_back(
        base::FilePath(FILE_PATH_LITERAL("/path/to/file1.exe")));
    files_to_delete_.push_back(
        base::FilePath(FILE_PATH_LITERAL("/path/to/other/file2.exe")));
    files_to_delete_.push_back(
        base::FilePath(FILE_PATH_LITERAL("/path/to/some file.dll")));
  }

  registry_keys_reporting_ = registry_keys_reporting;
  switch (registry_keys_reporting) {
    case ItemsReporting::kUnsupported:
      // Defined as an optional object in which a registry keys vector is not
      // present.
      registry_keys_ = base::Optional<std::vector<base::string16>>();
      break;

    case ItemsReporting::kNotReported:
      // Defined as an optional object in which an empty registry keys vector is
      // present.
      registry_keys_ =
          base::Optional<std::vector<base::string16>>(base::in_place);
      break;

    case ItemsReporting::kReported:
      // Defined as an optional object in which a non-empty registry keys vector
      // is present.
      registry_keys_ = base::Optional<std::vector<base::string16>>({
          L"HKCU:32\\Software\\Some\\Unwanted Software",
          L"HKCU:32\\Software\\Another\\Unwanted Software",
      });
      break;

    default:
      NOTREACHED();
  }

  extensions_reporting_ = extensions_reporting;
  switch (extensions_reporting) {
    case ItemsReporting::kUnsupported:
      // Defined as an optional object in which an extensions vector is not
      // present.
      extension_ids_ = base::Optional<std::vector<base::string16>>();
      expected_extension_names_ = base::Optional<std::vector<base::string16>>();
      break;

    case ItemsReporting::kNotReported:
      // Defined as an optional object in which an empty extensions vector is
      // present.
      extension_ids_ =
          base::Optional<std::vector<base::string16>>(base::in_place);
      expected_extension_names_ =
          base::Optional<std::vector<base::string16>>(base::in_place);
      break;

    case ItemsReporting::kReported:
      // Defined as an optional object in which a non-empty extensions vector is
      // present.
      extension_ids_ = base::Optional<std::vector<base::string16>>({
          kInstalledExtensionId1, kInstalledExtensionId2, kUnknownExtensionId,
      });
// Scanner results only fetches extension names on Windows Chrome build.
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
      expected_extension_names_ = base::Optional<std::vector<base::string16>>({
          kInstalledExtensionName1, kInstalledExtensionName2,
          l10n_util::GetStringFUTF16(
              IDS_SETTINGS_RESET_CLEANUP_DETAILS_EXTENSION_UNKNOWN,
              kUnknownExtensionId),
      });
#else
      expected_extension_names_ =
          base::Optional<std::vector<base::string16>>(base::in_place);
#endif
      break;

    default:
      NOTREACHED();
  }
}

int MockChromeCleanerProcess::Options::ExpectedExitCode(
    PromptAcceptance received_prompt_acceptance) const {
  if (crash_point() != CrashPoint::kNone)
    return kDeliberateCrashExitCode;

  if (files_to_delete_.empty())
    return kNothingFoundExitCode;

  if (received_prompt_acceptance == PromptAcceptance::ACCEPTED_WITH_LOGS ||
      received_prompt_acceptance == PromptAcceptance::ACCEPTED_WITHOUT_LOGS) {
    return reboot_required() ? kRebootRequiredExitCode
                             : kRebootNotRequiredExitCode;
  }

  return kDeclinedExitCode;
}

bool MockChromeCleanerProcess::InitWithCommandLine(
    const base::CommandLine& command_line) {
  if (!Options::FromCommandLine(command_line, &options_))
    return false;
  chrome_mojo_pipe_token_ = command_line.GetSwitchValueASCII(
      chrome_cleaner::kChromeMojoPipeTokenSwitch);
  if (chrome_mojo_pipe_token_.empty())
    return false;
  return true;
}

int MockChromeCleanerProcess::Run() {
  // We use EXPECT_*() macros to get good log lines, but since this code is run
  // in a separate process, failing a check in an EXPECT_*() macro will not fail
  // the test. Therefore, we use ::testing::Test::HasFailure() to detect
  // EXPECT_*() failures and return an error code that indicates that the test
  // should fail.
  EXPECT_FALSE(chrome_mojo_pipe_token_.empty());
  if (::testing::Test::HasFailure())
    return kInternalTestFailureExitCode;

  if (options_.crash_point() == CrashPoint::kOnStartup)
    exit(kDeliberateCrashExitCode);

  mojo::core::Init();
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  base::Thread io_thread("IPCThread");
  EXPECT_TRUE(io_thread.StartWithOptions(thread_options));
  if (::testing::Test::HasFailure())
    return kInternalTestFailureExitCode;

  mojo::core::ScopedIPCSupport ipc_support(
      io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  auto channel_endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess());
  auto invitation =
      mojo::IncomingInvitation::Accept(std::move(channel_endpoint));
  ChromePromptPtrInfo prompt_ptr_info(
      invitation.ExtractMessagePipe(chrome_mojo_pipe_token_), 0);

  if (options_.crash_point() == CrashPoint::kAfterConnection)
    exit(kDeliberateCrashExitCode);

  base::test::ScopedTaskEnvironment scoped_task_environment;
  base::RunLoop run_loop;
  // After the response from the parent process is received, this will post a
  // task to unblock the child process's main thread.
  auto quit_closure = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> main_runner,
         base::Closure quit_closure) {
        main_runner->PostTask(FROM_HERE, std::move(quit_closure));
      },
      base::SequencedTaskRunnerHandle::Get(),
      base::Passed(run_loop.QuitClosure()));

  io_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SendScanResults, options_, std::move(prompt_ptr_info),
                     base::Passed(&quit_closure),
                     base::Unretained(&received_prompt_acceptance_)));

  run_loop.Run();

  EXPECT_NE(received_prompt_acceptance_, PromptAcceptance::UNSPECIFIED);
  EXPECT_EQ(received_prompt_acceptance_, options_.expected_user_response());
  if (::testing::Test::HasFailure())
    return kInternalTestFailureExitCode;
  return options_.ExpectedExitCode(received_prompt_acceptance_);
}

// Keep the printable names of these enums short since they're used in tests
// with very long parameter lists.

std::ostream& operator<<(std::ostream& out, CrashPoint crash_point) {
  return out << "CrPt" << static_cast<int>(crash_point);
}

std::ostream& operator<<(std::ostream& out, UwsFoundStatus status) {
  return out << "UwS" << static_cast<int>(status);
}

std::ostream& operator<<(std::ostream& out,
                         ExtensionCleaningFeatureStatus status) {
  return out << "Ext" << static_cast<int>(status);
}

std::ostream& operator<<(std::ostream& out, ItemsReporting items_reporting) {
  return out << "Items" << static_cast<int>(items_reporting);
}

}  // namespace safe_browsing
