// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/captured_sites_test_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sync_message.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {
// The maximum amount of time to wait for Chrome to finish autofilling a form.
const base::TimeDelta kAutofillActionWaitForVisualUpdateTimeout =
    base::TimeDelta::FromSeconds(3);

// The number of tries the TestRecipeReplayer should perform when executing an
// Chrome Autofill action.
// Chrome Autofill can be flaky on some real-world pages. The Captured Site
// Automation Framework will retry an autofill action a couple times before
// concluding that Chrome Autofill does not work.
const int kAutofillActionNumRetries = 5;

// The public key hash for the certificate Web Page Replay (WPR) uses to serve
// HTTPS content.
// The Captured Sites Test Framework relies on WPR to serve captured site
// traffic. If a machine does not have the WPR certificate installed, Chrome
// will detect a server certificate validation failure when WPR serves Chrome
// HTTPS content. In response Chrome will block the WPR HTTPS content.
// The test framework avoids this problem by launching Chrome with the
// ignore-certificate-errors-spki-list flag set to the WPR certificate's
// public key hash. Doing so tells Chrome to ignore server certificate
// validation errors from WPR.
const char kWebPageReplayCertSPKI[] =
    "PoNnQAwghMiLUPg1YNFtvTfGreNT8r9oeLEyzgNCJWc=";
}  // namespace

namespace captured_sites_test_utils {

constexpr base::TimeDelta PageActivityObserver::kPaintEventCheckInterval;

std::string FilePathToUTF8(const base::FilePath::StringType& str) {
#if defined(OS_WIN)
  return base::WideToUTF8(str);
#else
  return str;
#endif
}

// PageActivityObserver -------------------------------------------------------
PageActivityObserver::PageActivityObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PageActivityObserver::PageActivityObserver(content::RenderFrameHost* frame)
    : content::WebContentsObserver(
          content::WebContents::FromRenderFrameHost(frame)) {}

void PageActivityObserver::WaitTillPageIsIdle(
    base::TimeDelta continuous_paint_timeout) {
  base::TimeTicks finished_load_time = base::TimeTicks::Now();
  bool page_is_loading = false;
  do {
    paint_occurred_during_last_loop_ = false;
    base::RunLoop heart_beat;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, heart_beat.QuitClosure(), kPaintEventCheckInterval);
    heart_beat.Run();
    page_is_loading =
        web_contents()->IsWaitingForResponse() || web_contents()->IsLoading();
    if (page_is_loading) {
      finished_load_time = base::TimeTicks::Now();
    } else if ((base::TimeTicks::Now() - finished_load_time) >
               continuous_paint_timeout) {
      // |continuous_paint_timeout| has expired since Chrome loaded the page.
      // During this period of time, Chrome has been continuously painting
      // the page. In this case, the page is probably idle, but a bug, a
      // blinking caret or a persistent animation is making Chrome paint at
      // regular intervals. Exit.
      break;
    }
  } while (page_is_loading || paint_occurred_during_last_loop_);
}

bool PageActivityObserver::WaitForVisualUpdate(base::TimeDelta timeout) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (!paint_occurred_during_last_loop_) {
    base::RunLoop heart_beat;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, heart_beat.QuitClosure(), kPaintEventCheckInterval);
    heart_beat.Run();
    if ((base::TimeTicks::Now() - start_time) > timeout) {
      return false;
    }
  }
  return true;
}

void PageActivityObserver::DidCommitAndDrawCompositorFrame() {
  paint_occurred_during_last_loop_ = true;
}

// FrameObserver --------------------------------------------------------------
IFrameWaiter::IFrameWaiter(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      query_type_(URL),
      target_frame_(nullptr) {}

IFrameWaiter::~IFrameWaiter() {}

content::RenderFrameHost* IFrameWaiter::WaitForFrameMatchingName(
    const std::string& name,
    const base::TimeDelta timeout) {
  content::RenderFrameHost* frame = FrameMatchingPredicate(
      web_contents(), base::BindRepeating(&content::FrameMatchesName, name));
  if (frame) {
    return frame;
  } else {
    query_type_ = NAME;
    frame_name_ = name;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
    run_loop_.Run();
    return target_frame_;
  }
}

content::RenderFrameHost* IFrameWaiter::WaitForFrameMatchingOrigin(
    const GURL origin,
    const base::TimeDelta timeout) {
  content::RenderFrameHost* frame = FrameMatchingPredicate(
      web_contents(), base::BindRepeating(&FrameHasOrigin, origin));
  if (frame) {
    return frame;
  } else {
    query_type_ = ORIGIN;
    origin_ = origin;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
    run_loop_.Run();
    return target_frame_;
  }
}

content::RenderFrameHost* IFrameWaiter::WaitForFrameMatchingUrl(
    const GURL url,
    const base::TimeDelta timeout) {
  content::RenderFrameHost* frame = FrameMatchingPredicate(
      web_contents(), base::BindRepeating(&content::FrameHasSourceUrl, url));
  if (frame) {
    return frame;
  } else {
    query_type_ = URL;
    url_ = url;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_.QuitClosure(), timeout);
    run_loop_.Run();
    return target_frame_;
  }
}

void IFrameWaiter::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!run_loop_.running())
    return;
  switch (query_type_) {
    case NAME:
      if (FrameMatchesName(frame_name_, render_frame_host))
        run_loop_.Quit();
      break;
    case ORIGIN:
      if (render_frame_host->GetLastCommittedURL().GetOrigin() == origin_)
        run_loop_.Quit();
      break;
    case URL:
      if (FrameHasSourceUrl(url_, render_frame_host))
        run_loop_.Quit();
      break;
    default:
      break;
  }
}

void IFrameWaiter::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                 const GURL& validated_url) {
  if (!run_loop_.running())
    return;
  switch (query_type_) {
    case ORIGIN:
      if (validated_url.GetOrigin() == origin_)
        run_loop_.Quit();
      break;
    case URL:
      if (FrameHasSourceUrl(validated_url, render_frame_host))
        run_loop_.Quit();
      break;
    default:
      break;
  }
}

void IFrameWaiter::FrameNameChanged(content::RenderFrameHost* render_frame_host,
                                    const std::string& name) {
  if (!run_loop_.running())
    return;
  switch (query_type_) {
    case NAME:
      if (FrameMatchesName(name, render_frame_host))
        run_loop_.Quit();
      break;
    default:
      break;
  }
}

bool IFrameWaiter::FrameHasOrigin(const GURL& origin,
                                  content::RenderFrameHost* frame) {
  GURL url = frame->GetLastCommittedURL();
  return (url.GetOrigin() == origin.GetOrigin());
}

// TestRecipeReplayer ---------------------------------------------------------
TestRecipeReplayer::TestRecipeReplayer(
    Browser* browser,
    TestRecipeReplayChromeFeatureActionExecutor* feature_action_executor)
    : browser_(browser), feature_action_executor_(feature_action_executor) {}

TestRecipeReplayer::~TestRecipeReplayer() {}

bool TestRecipeReplayer::ReplayTest(const base::FilePath capture_file_path,
                                    const base::FilePath recipe_file_path) {
  if (!StartWebPageReplayServer(capture_file_path))
    return false;

  return ReplayRecordedActions(recipe_file_path);
}

// static
void TestRecipeReplayer::SetUpCommandLine(base::CommandLine* command_line) {
  // Direct traffic to the Web Page Replay server.
  command_line->AppendSwitchASCII(
      network::switches::kHostResolverRules,
      base::StringPrintf(
          "MAP *:80 127.0.0.1:%d,"
          "MAP *:443 127.0.0.1:%d,"
          // Uncomment to use the live autofill prediction server.
          // "EXCLUDE clients1.google.com,"
          "EXCLUDE localhost",
          kHostHttpPort, kHostHttpsPort));
  command_line->AppendSwitchASCII(
      network::switches::kIgnoreCertificateErrorsSPKIList,
      kWebPageReplayCertSPKI);
}

void TestRecipeReplayer::Setup() {
  CleanupSiteData();

  // Bypass permission dialogs.
  PermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(PermissionRequestManager::ACCEPT_ALL);
}

void TestRecipeReplayer::Cleanup() {
  // If there are still cookies at the time the browser test shuts down,
  // Chrome's SQL lite persistent cookie store will crash.
  CleanupSiteData();
  EXPECT_TRUE(StopWebPageReplayServer())
      << "Cannot stop the local Web Page Replay server.";
}

TestRecipeReplayChromeFeatureActionExecutor*
TestRecipeReplayer::feature_action_executor() {
  return feature_action_executor_;
}

Browser* TestRecipeReplayer::browser() {
  return browser_;
}

content::WebContents* TestRecipeReplayer::GetWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void TestRecipeReplayer::CleanupSiteData() {
  // Navigate to about:blank, then clear the browser cache.
  // Navigating to about:blank before clearing the cache ensures that
  // the cleanup is thorough and nothing is held.
  ui_test_utils::NavigateToURL(browser_, GURL(url::kAboutBlankURL));
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser_->profile());
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      &completion_observer);
  completion_observer.BlockUntilCompletion();
}

bool TestRecipeReplayer::StartWebPageReplayServer(
    const base::FilePath& capture_file_path) {
  std::vector<std::string> args;
  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    ADD_FAILURE() << "Failed to extract the Chromium source directory!";
    return false;
  }

  args.push_back(base::StringPrintf("--http_port=%d", kHostHttpPort));
  args.push_back(base::StringPrintf("--https_port=%d", kHostHttpsPort));
  args.push_back("--serve_response_in_chronological_sequence");
  args.push_back(base::StringPrintf(
      "--inject_scripts=%s,%s",
      FilePathToUTF8(src_dir.AppendASCII("third_party")
                         .AppendASCII("catapult")
                         .AppendASCII("web_page_replay_go")
                         .AppendASCII("deterministic.js")
                         .value())
          .c_str(),
      FilePathToUTF8(src_dir.AppendASCII("chrome")
                         .AppendASCII("test")
                         .AppendASCII("data")
                         .AppendASCII("web_page_replay_go_helper_scripts")
                         .AppendASCII("automation_helper.js")
                         .value())
          .c_str()));

  // Specify the capture file.
  args.push_back(base::StringPrintf(
      "%s", FilePathToUTF8(capture_file_path.value()).c_str()));
  if (!RunWebPageReplayCmd("replay", args, &web_page_replay_server_))
    return false;

  // Sleep 20 seconds to wait for the web page replay server to start.
  // TODO(crbug.com/847910): create a process std stream reader class to use the
  // process output to determine when the server is ready
  base::RunLoop wpr_launch_waiter;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, wpr_launch_waiter.QuitClosure(),
      base::TimeDelta::FromSeconds(20));
  wpr_launch_waiter.Run();

  if (!web_page_replay_server_.IsValid()) {
    ADD_FAILURE() << "Failed to start the WPR replay server!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::StopWebPageReplayServer() {
  if (web_page_replay_server_.IsValid()) {
    if (!web_page_replay_server_.Terminate(0, true)) {
      ADD_FAILURE() << "Failed to terminate the WPR replay server!";
      return false;
    }
  }

  // The test server hasn't started, no op.
  return true;
}

bool TestRecipeReplayer::RunWebPageReplayCmdAndWaitForExit(
    const std::string& cmd,
    const std::vector<std::string>& args,
    const base::TimeDelta& timeout) {
  base::Process process;
  int exit_code;

  if (RunWebPageReplayCmd(cmd, args, &process) && process.IsValid() &&
      process.WaitForExitWithTimeout(timeout, &exit_code) && exit_code == 0) {
    return true;
  }

  ADD_FAILURE() << "Failed to run WPR command: '" << cmd << "'!";
  return false;
}

bool TestRecipeReplayer::RunWebPageReplayCmd(
    const std::string& cmd,
    const std::vector<std::string>& args,
    base::Process* process) {
  // Allow the function to block. Otherwise the subsequent call to
  // base::PathExists will fail. base::PathExists must be called from
  // a scope that allows blocking.
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::LaunchOptions options = base::LaunchOptionsForTest();
  base::FilePath exe_dir;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &exe_dir)) {
    ADD_FAILURE() << "Failed to extract the Chromium source directory!";
    return false;
  }

  base::FilePath web_page_replay_binary_dir = exe_dir.AppendASCII("third_party")
                                                  .AppendASCII("catapult")
                                                  .AppendASCII("telemetry")
                                                  .AppendASCII("telemetry")
                                                  .AppendASCII("bin");
  options.current_directory = web_page_replay_binary_dir;

#if defined(OS_WIN)
  base::FilePath wpr_executable_binary =
      base::FilePath(FILE_PATH_LITERAL("win"))
          .AppendASCII("AMD64")
          .AppendASCII("wpr.exe");
#elif defined(OS_MACOSX)
  base::FilePath wpr_executable_binary =
      base::FilePath(FILE_PATH_LITERAL("mac"))
          .AppendASCII("x86_64")
          .AppendASCII("wpr");
#elif defined(OS_POSIX)
  base::FilePath wpr_executable_binary =
      base::FilePath(FILE_PATH_LITERAL("linux"))
          .AppendASCII("x86_64")
          .AppendASCII("wpr");
#else
#error Plaform is not supported.
#endif
  base::CommandLine full_command(
      web_page_replay_binary_dir.Append(wpr_executable_binary));
  full_command.AppendArg(cmd);

  // Ask web page replay to use the custom certificate and key files used to
  // make the web page captures.
  // The capture files used in these browser tests are also used on iOS to
  // test autofill.
  // The custom cert and key files are different from those of the offical
  // WPR releases. The custom files are made to work on iOS.
  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    ADD_FAILURE() << "Failed to extract the Chromium source directory!";
    return false;
  }

  base::FilePath web_page_replay_support_file_dir =
      src_dir.AppendASCII("components")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("autofill")
          .AppendASCII("web_page_replay_support_files");
  full_command.AppendArg(base::StringPrintf(
      "--https_cert_file=%s",
      FilePathToUTF8(
          web_page_replay_support_file_dir.AppendASCII("wpr_cert.pem").value())
          .c_str()));
  full_command.AppendArg(base::StringPrintf(
      "--https_key_file=%s",
      FilePathToUTF8(
          web_page_replay_support_file_dir.AppendASCII("wpr_key.pem").value())
          .c_str()));

  for (const auto arg : args)
    full_command.AppendArg(arg);

  LOG(INFO) << full_command.GetArgumentsString();

  *process = base::LaunchProcess(full_command, options);
  return true;
}

bool TestRecipeReplayer::ReplayRecordedActions(
    const base::FilePath& recipe_file_path) {
  // Read the text of the recipe file.
  base::ThreadRestrictions::SetIOAllowed(true);
  std::string json_text;
  if (!base::ReadFileToString(recipe_file_path, &json_text)) {
    ADD_FAILURE() << "Failed to read recipe file '" << recipe_file_path << "'!";
    return false;
  }

  // Convert the file text into a json object.
  std::unique_ptr<base::DictionaryValue> recipe = base::DictionaryValue::From(
      base::JSONReader().ReadToValueDeprecated(json_text));
  if (!recipe) {
    ADD_FAILURE() << "Failed to deserialize json text!";
    return false;
  }

  if (!InitializeBrowserToExecuteRecipe(recipe))
    return false;

  // Iterate through and execute each action in the recipe.
  base::Value* action_list_container = recipe->FindKey("actions");
  if (!action_list_container) {
    ADD_FAILURE() << "Failed to extract action list from the recipe!";
    return false;
  }

  if (base::Value::Type::LIST != action_list_container->type()) {
    ADD_FAILURE() << "The recipe's actions object is not a list!";
    return false;
  }

  base::Value::ListStorage& action_list = action_list_container->GetList();

  for (auto it_action = action_list.begin(); it_action != action_list.end();
       ++it_action) {
    base::DictionaryValue* action;
    if (!it_action->GetAsDictionary(&action)) {
      ADD_FAILURE()
          << "Failed to extract an individual action from the recipe!";
      return false;
    }

    base::Value* type_container = action->FindKey("type");
    if (!type_container) {
      ADD_FAILURE() << "Failed to extract action type from the recipe!";
      return false;
    }
    if (base::Value::Type::STRING != type_container->type()) {
      ADD_FAILURE() << "Action type is not a string!";
      return false;
    }
    std::string type = type_container->GetString();

    if (base::CompareCaseInsensitiveASCII(type, "autofill") == 0) {
      if (!ExecuteAutofillAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "click") == 0) {
      if (!ExecuteClickAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "coolOff") == 0) {
      if (!ExecuteCoolOffAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "executeScript") == 0) {
      if (!ExecuteRunCommandAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "hover") == 0) {
      if (!ExecuteHoverAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "loadPage") == 0) {
      if (!ExecuteForceLoadPage(*action))
        return false;
      // Load page is an no-op action.
    } else if (base::CompareCaseInsensitiveASCII(type, "pressEnter") == 0) {
      if (!ExecutePressEnterAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "pressEscape") == 0) {
      if (!ExecutePressEscapeAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "savePassword") == 0) {
      if (!ExecuteSavePasswordAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "select") == 0) {
      if (!ExecuteSelectDropdownAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "type") == 0) {
      if (!ExecuteTypeAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "typePassword") == 0) {
      if (!ExecuteTypePasswordAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "updatePassword") == 0) {
      if (!ExecuteUpdatePasswordAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "validateField") == 0) {
      if (!ExecuteValidateFieldValueAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(
                   type, "validateNoSavePasswordPrompt") == 0) {
      if (!ExecuteValidateNoSavePasswordPromptAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(
                   type, "validatePasswordSaveFallback") == 0) {
      if (!ExecuteValidateSaveFallbackAction(*action))
        return false;
    } else if (base::CompareCaseInsensitiveASCII(type, "waitFor") == 0) {
      if (!ExecuteWaitForStateAction(*action))
        return false;
    } else {
      ADD_FAILURE() << "Unrecognized action type: " << type;
    }
  }  // end foreach action

  // Dismiss the beforeUnloadDialog if the last page of the test has a
  // beforeUnload function.
  if (recipe->FindKey("dismissBeforeUnload")) {
    NavigateAwayAndDismissBeforeUnloadDialog();
  }

  return true;
}

// Functions for deserializing and executing actions from the test recipe
// JSON object.
bool TestRecipeReplayer::InitializeBrowserToExecuteRecipe(
    const std::unique_ptr<base::DictionaryValue>& recipe) {
  // Setup any saved address and credit card at the start of the test.
  const base::Value* autofill_profile_container =
      recipe->FindKey("autofillProfile");

  if (autofill_profile_container &&
      !SetupSavedAutofillProfile(*autofill_profile_container))
    return false;

  // Setup any saved passwords at the start of the test.
  const base::Value* saved_password_container =
      recipe->FindKey("passwordManagerProfiles");

  if (saved_password_container &&
      !SetupSavedPasswords(*saved_password_container))
    return false;

  // Extract the starting URL from the test recipe.
  base::Value* starting_url_container = recipe->FindKey("startingURL");
  if (!starting_url_container) {
    ADD_FAILURE() << "Failed to extract the starting url from the recipe!";
    return false;
  }

  if (base::Value::Type::STRING != starting_url_container->type()) {
    ADD_FAILURE() << "Starting url is not a string!";
    return false;
  }

  std::string starting_url = starting_url_container->GetString();

  // Navigate to the starting URL, wait for the page to complete loading.
  PageActivityObserver page_activity_observer(GetWebContents());
  if (!content::ExecuteScript(GetWebContents(),
                              base::StringPrintf("window.location.href = '%s';",
                                                 starting_url.c_str()))) {
    ADD_FAILURE() << "Failed to navigate Chrome to '" << starting_url << "'!";
    return false;
  }

  page_activity_observer.WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteAutofillAction(
    const base::DictionaryValue& action) {
  std::string xpath;
  if (!GetTargetHTMLElementXpathFromAction(action, &xpath))
    return false;

  int visibility_enum_val;
  if (!GetTargetHTMLElementVisibilityEnumFromAction(action,
                                                    &visibility_enum_val))
    return false;

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  std::vector<std::string> frame_path;
  if (!GetIFramePathFromAction(action, &frame_path))
    return false;

  if (!WaitForElementToBeReady(xpath, visibility_enum_val, frame))
    return false;

  VLOG(1) << "Invoking Chrome Autofill on `" << xpath << "`.";
  PageActivityObserver page_activity_observer(frame);
  // Clear the input box first, in case a previous value is there.
  // If the text input box is not clear, pressing the down key will not
  // bring up the autofill suggestion box.
  // This can happen on sites that requires the user to sign in. After
  // signing in, the site fills the form with the user's profile
  // information.
  if (!ExecuteJavaScriptOnElementByXpath(
          frame, xpath,
          "automation_helper.setInputElementValue(target, ``);")) {
    ADD_FAILURE() << "Failed to clear the input field value!";
    return false;
  }

  if (!feature_action_executor()->AutofillForm(
          xpath, frame_path, kAutofillActionNumRetries, frame))
    return false;
  page_activity_observer.WaitTillPageIsIdle(
      kAutofillActionWaitForVisualUpdateTimeout);
  return true;
}

bool TestRecipeReplayer::ExecuteClickAction(
    const base::DictionaryValue& action) {
  std::string xpath;
  if (!GetTargetHTMLElementXpathFromAction(action, &xpath))
    return false;

  int visibility_enum_val;
  if (!GetTargetHTMLElementVisibilityEnumFromAction(action,
                                                    &visibility_enum_val))
    return false;

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  std::vector<std::string> frame_path;
  if (!GetIFramePathFromAction(action, &frame_path))
    return false;

  if (!WaitForElementToBeReady(xpath, visibility_enum_val, frame))
    return false;

  VLOG(1) << "Left mouse clicking `" << xpath << "`.";
  PageActivityObserver page_activity_observer(frame);
  if (!ScrollElementIntoView(xpath, frame))
    return false;

  gfx::Rect rect;
  if (!GetBoundingRectOfTargetElement(xpath, frame, &rect))
    return false;
  if (!SimulateLeftMouseClickAt(rect.CenterPoint(), frame))
    return false;

  page_activity_observer.WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteCoolOffAction(
    const base::DictionaryValue& action) {
  base::RunLoop heart_beat;
  base::TimeDelta cool_off_time = cool_off_action_timeout;
  const base::Value* pause_time_container = action.FindKey("pauseTimeSec");
  if (pause_time_container) {
    if (pause_time_container->type() != base::Value::Type::INTEGER) {
      ADD_FAILURE() << "Pause time is not an integer!";
      return false;
    }
    int seconds = pause_time_container->GetInt();
    cool_off_time = base::TimeDelta::FromSeconds(seconds);
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, heart_beat.QuitClosure(), cool_off_time);
  VLOG(1) << "Pausing execution for '" << cool_off_time.InSeconds()
          << "' seconds";
  heart_beat.Run();

  return true;
}

bool TestRecipeReplayer::ExecuteHoverAction(
    const base::DictionaryValue& action) {
  std::string xpath;
  if (!GetTargetHTMLElementXpathFromAction(action, &xpath))
    return false;

  int visibility_enum_val;
  if (!GetTargetHTMLElementVisibilityEnumFromAction(action,
                                                    &visibility_enum_val))
    return false;

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  if (!WaitForElementToBeReady(xpath, visibility_enum_val, frame))
    return false;

  VLOG(1) << "Hovering over `" << xpath << "`.";
  PageActivityObserver page_activity_observer(frame);

  if (!ScrollElementIntoView(xpath, frame))
    return false;

  gfx::Rect rect;
  if (!GetBoundingRectOfTargetElement(xpath, frame, &rect))
    return false;

  if (!SimulateMouseHoverAt(frame, rect.CenterPoint()))
    return false;

  if (!page_activity_observer.WaitForVisualUpdate()) {
    ADD_FAILURE() << "The page did not respond to a mouse hover action!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::ExecuteForceLoadPage(
    const base::DictionaryValue& action) {
  const base::Value* force_load_container = action.FindKey("force");
  const base::Value* url_container = action.FindKey("url");
  if (!force_load_container) {
    // Nothing to do, load should have been made by previous action
    return true;
  }

  if (force_load_container->type() != base::Value::Type::BOOLEAN) {
    ADD_FAILURE() << "Force load is not a bool!";
    return false;
  }

  bool shouldForce = force_load_container->GetBool();
  if (!shouldForce)
    return true;

  if (!url_container || url_container->type() != base::Value::Type::STRING) {
    ADD_FAILURE() << "Force load url could not be parsed";
    return false;
  }
  std::string url = url_container->GetString();
  VLOG(1) << "Making explicit URL redirect to '" << url << "'";
  ui_test_utils::NavigateToURL(browser_, GURL(url));

  PageActivityObserver page_activity_observer(GetWebContents());
  page_activity_observer.WaitTillPageIsIdle();

  return true;
}

bool TestRecipeReplayer::ExecutePressEnterAction(
    const base::DictionaryValue& action) {
  std::string xpath;
  if (!GetTargetHTMLElementXpathFromAction(action, &xpath))
    return false;

  int visibility_enum_val;
  if (!GetTargetHTMLElementVisibilityEnumFromAction(action,
                                                    &visibility_enum_val))
    return false;

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  std::vector<std::string> frame_path;
  if (!GetIFramePathFromAction(action, &frame_path))
    return false;

  if (!WaitForElementToBeReady(xpath, visibility_enum_val, frame))
    return false;

  VLOG(1) << "Pressing 'Enter' on `" << xpath << "`.";
  PageActivityObserver page_activity_observer(frame);
  if (!PlaceFocusOnElement(xpath, frame_path, frame))
    return false;

  ui::DomKey key = ui::DomKey::ENTER;
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  SimulateKeyPress(content::WebContents::FromRenderFrameHost(frame), key, code,
                   key_code, false, false, false, false);
  page_activity_observer.WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecutePressEscapeAction(
    const base::DictionaryValue& action) {
  ui::DomKey key = ui::DomKey::ESCAPE;
  ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  SimulateKeyPress(GetWebContents(), key, code, key_code, false, false, false,
                   false);
  VLOG(1) << "Pressing 'Esc' in the current frame";
  PageActivityObserver page_activity_observer(GetWebContents());
  page_activity_observer.WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteRunCommandAction(
    const base::DictionaryValue& action) {
  // Extract the list of JavaScript commands into a vector.
  std::vector<std::string> commands;

  const base::Value* commands_list_container = action.FindKey("commands");
  if (!commands_list_container) {
    ADD_FAILURE()
        << "Failed to extract the list of commands from the run command "
        << "action!";
    return false;
  }

  if (base::Value::Type::LIST != commands_list_container->type()) {
    ADD_FAILURE() << "commands is not an array!";
    return false;
  }

  const base::Value::ListStorage& commands_list =
      commands_list_container->GetList();
  for (auto it_command = commands_list.begin();
       it_command != commands_list.end(); ++it_command) {
    if (base::Value::Type::STRING != it_command->type()) {
      ADD_FAILURE() << "command is not a string!";
      return false;
    }
    commands.push_back(it_command->GetString());
  }

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame)) {
    return false;
  }

  VLOG(1) << "Running JavaScript commands on the page.";

  // Execute the commands.
  PageActivityObserver page_activity_observer(frame);
  for (const std::string& command : commands) {
    if (!content::ExecuteScript(frame, command)) {
      ADD_FAILURE() << "Failed to execute JavaScript command `" << command
                    << "`!";
      return false;
    }
    // Wait in case the JavaScript command triggers page load or layout
    // changes.
    page_activity_observer.WaitTillPageIsIdle();
  }

  return true;
}

bool TestRecipeReplayer::ExecuteSavePasswordAction(
    const base::DictionaryValue& action) {
  VLOG(1) << "Save password.";

  if (!feature_action_executor()->SavePassword())
    return false;

  bool stored_cred;
  if (!HasChromeStoredCredential(action, &stored_cred))
    return false;

  if (!stored_cred) {
    ADD_FAILURE() << "Chrome did not save the credential!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::ExecuteSelectDropdownAction(
    const base::DictionaryValue& action) {
  const base::Value* index_container = action.FindKey("index");
  if (!index_container) {
    ADD_FAILURE()
        << "Failed to extract selection index from the select action!";
    return false;
  }

  if (base::Value::Type::INTEGER != index_container->type()) {
    ADD_FAILURE() << "Selection index is not an integer!";
    return false;
  }

  int index = index_container->GetInt();

  std::string xpath;
  if (!GetTargetHTMLElementXpathFromAction(action, &xpath))
    return false;

  int visibility_enum_val;
  if (!GetTargetHTMLElementVisibilityEnumFromAction(action,
                                                    &visibility_enum_val))
    return false;

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  if (!WaitForElementToBeReady(xpath, visibility_enum_val, frame))
    return false;

  VLOG(1) << "Select option '" << index << "' from `" << xpath << "`.";
  PageActivityObserver page_activity_observer(frame);
  if (!ExecuteJavaScriptOnElementByXpath(
          frame, xpath,
          base::StringPrintf(
              "automation_helper"
              "  .selectOptionFromDropDownElementByIndex(target, %d);",
              index_container->GetInt()))) {
    ADD_FAILURE() << "Failed to select drop down option with JavaScript!";
    return false;
  }

  page_activity_observer.WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteTypeAction(
    const base::DictionaryValue& action) {
  const base::Value* value_container = action.FindKey("value");
  if (!value_container) {
    ADD_FAILURE() << "Failed to extract value from the type action!";
    return false;
  }

  if (base::Value::Type::STRING != value_container->type()) {
    ADD_FAILURE() << "Value is not a string!";
    return false;
  }

  std::string value = value_container->GetString();

  std::string xpath;
  if (!GetTargetHTMLElementXpathFromAction(action, &xpath))
    return false;

  int visibility_enum_val;
  if (!GetTargetHTMLElementVisibilityEnumFromAction(action,
                                                    &visibility_enum_val))
    return false;

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  if (!WaitForElementToBeReady(xpath, visibility_enum_val, frame))
    return false;

  VLOG(1) << "Typing '" << value << "' inside `" << xpath << "`.";
  PageActivityObserver page_activity_observer(frame);
  if (!ExecuteJavaScriptOnElementByXpath(
          frame, xpath,
          base::StringPrintf(
              "automation_helper.setInputElementValue(target, `%s`);",
              value.c_str()))) {
    ADD_FAILURE() << "Failed to type inside input element with JavaScript!";
    return false;
  }

  page_activity_observer.WaitTillPageIsIdle();
  return true;
}

bool TestRecipeReplayer::ExecuteTypePasswordAction(
    const base::DictionaryValue& action) {
  std::string xpath;
  if (!GetTargetHTMLElementXpathFromAction(action, &xpath))
    return false;

  int visibility_enum_val;
  if (!GetTargetHTMLElementVisibilityEnumFromAction(action,
                                                    &visibility_enum_val))
    return false;

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  std::vector<std::string> frame_path;
  if (!GetIFramePathFromAction(action, &frame_path))
    return false;

  if (!WaitForElementToBeReady(xpath, visibility_enum_val, frame))
    return false;

  const base::Value* value_container = action.FindKey("value");
  if (!value_container) {
    ADD_FAILURE() << "Failed to extract the value from the type password"
                  << " action!";
    return false;
  }

  if (base::Value::Type::STRING != value_container->type()) {
    ADD_FAILURE() << "Value is not a string!";
    return false;
  }

  std::string value = value_container->GetString();

  // Clear the password field first, in case a previous value is there.
  if (!ExecuteJavaScriptOnElementByXpath(
          frame, xpath,
          "automation_helper.setInputElementValue(target, ``);")) {
    ADD_FAILURE() << "Failed to execute JavaScript to clear the input value!";
    return false;
  }

  if (!PlaceFocusOnElement(xpath, frame_path, frame))
    return false;

  VLOG(1) << "Typing '" << value << "' inside `" << xpath << "`.";

  const char* c_array = value.c_str();
  for (size_t index = 0; index < value.size(); index++) {
    ui::DomKey key = ui::DomKey::FromCharacter(c_array[index]);
    ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
    ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
    SimulateKeyPress(content::WebContents::FromRenderFrameHost(frame), key,
                     code, key_code, false, false, false, false);
  }

  return true;
}

bool TestRecipeReplayer::ExecuteUpdatePasswordAction(
    const base::DictionaryValue& action) {
  VLOG(1) << "Update password.";

  if (!feature_action_executor()->UpdatePassword())
    return false;

  bool stored_cred;
  if (!HasChromeStoredCredential(action, &stored_cred))
    return false;

  if (!stored_cred) {
    ADD_FAILURE() << "Chrome did not update the credential!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::ExecuteValidateFieldValueAction(
    const base::DictionaryValue& action) {
  std::string xpath;
  if (!GetTargetHTMLElementXpathFromAction(action, &xpath))
    return false;

  int visibility_enum_val;
  if (!GetTargetHTMLElementVisibilityEnumFromAction(action,
                                                    &visibility_enum_val))
    return false;

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  // If we're just validating we don't care about on_top-ness, as copied from
  // chrome/test/data/web_page_replay_go_helper_scripts/automation_helper.js
  // to TestRecipeReplayer::DomElementReadyState enum
  // So remove (DomElementReadyState::kReadyStateOnTop)
  if (visibility_enum_val & kReadyStateOnTop)
    visibility_enum_val -= kReadyStateOnTop;

  if (!WaitForElementToBeReady(xpath, visibility_enum_val, frame))
    return false;

  const base::Value* autofill_prediction_container =
      action.FindKey("expectedAutofillType");
  if (autofill_prediction_container) {
    if (base::Value::Type::STRING != autofill_prediction_container->type()) {
      ADD_FAILURE() << "Autofill prediction is not a string!";
      return false;
    }

    // If we are validating the value of a Chrome autofilled field, print the
    // Chrome Autofill's field annotation for debugging purpose.
    std::string title;
    if (GetElementProperty(frame, xpath, "return target.getAttribute('title');",
                           &title)) {
      VLOG(1) << title;
    } else {
      ADD_FAILURE()
          << "Failed to obtain the field's Chrome Autofill annotation!";
    }

    std::string expected_autofill_prediction_type =
        autofill_prediction_container->GetString();
    VLOG(1) << "Checking the field `" << xpath << "` has the autofill type '"
            << expected_autofill_prediction_type << "'";
    ExpectElementPropertyEquals(
        frame, xpath.c_str(),
        "return target.getAttribute('autofill-prediction');",
        expected_autofill_prediction_type, true);
  }

  const base::Value* expected_value_container = action.FindKey("expectedValue");
  if (!expected_value_container) {
    ADD_FAILURE() << "Failed to extract the expected value field from the "
                     "validate field value action!";
    return false;
  }

  if (base::Value::Type::STRING != expected_value_container->type()) {
    ADD_FAILURE() << "Expected value is not a string!";
    return false;
  }

  std::string expected_value = expected_value_container->GetString();

  VLOG(1) << "Checking the field `" << xpath << "`.";
  ExpectElementPropertyEquals(frame, xpath.c_str(), "return target.value;",
                              expected_value);
  return true;
}

bool TestRecipeReplayer::ExecuteValidateNoSavePasswordPromptAction(
    const base::DictionaryValue& action) {
  VLOG(1) << "Verify that the page hasn't shown a save password prompt.";
  EXPECT_FALSE(feature_action_executor()->HasChromeShownSavePasswordPrompt());
  return true;
}

bool TestRecipeReplayer::ExecuteValidateSaveFallbackAction(
    const base::DictionaryValue& action) {
  VLOG(1) << "Verify that Chrome shows the save fallback icon in the omnibox.";
  EXPECT_TRUE(feature_action_executor()->WaitForSaveFallback());
  return true;
}

bool TestRecipeReplayer::ExecuteWaitForStateAction(
    const base::DictionaryValue& action) {
  // Extract the list of JavaScript assertions into a vector.
  std::vector<std::string> state_assertions;
  const base::Value* assertions_list_container = action.FindKey("assertions");
  if (!assertions_list_container) {
    ADD_FAILURE()
        << "Failed to extract assertions from the wait for state action!";
    return false;
  }

  if (base::Value::Type::LIST != assertions_list_container->type()) {
    ADD_FAILURE() << "Assertions is not a list!";
    return false;
  }

  const base::Value::ListStorage& assertions_list =
      assertions_list_container->GetList();
  for (const base::Value& assertion : assertions_list) {
    if (base::Value::Type::STRING != assertion.type()) {
      ADD_FAILURE() << "Assertion is not a string!";
      return false;
    }

    state_assertions.push_back(assertion.GetString());
  }

  content::RenderFrameHost* frame;
  if (!GetTargetFrameFromAction(action, &frame))
    return false;

  VLOG(1) << "Waiting for page to reach a state.";

  // Wait for all of the assertions to become true on the current page.
  return WaitForStateChange(frame, state_assertions, default_action_timeout);
}

bool TestRecipeReplayer::GetTargetHTMLElementXpathFromAction(
    const base::DictionaryValue& action,
    std::string* xpath) {
  xpath->clear();
  const base::Value* xpath_container = action.FindKey("selector");
  if (!xpath_container) {
    ADD_FAILURE() << "Failed to extract the xpath selector from action!";
    return false;
  }

  if (base::Value::Type::STRING != xpath_container->type()) {
    ADD_FAILURE() << "Xpath selector is not a string!";
    return false;
  }

  *xpath = xpath_container->GetString();
  return true;
}

bool TestRecipeReplayer::GetTargetHTMLElementVisibilityEnumFromAction(
    const base::DictionaryValue& action,
    int* visibility_enum_val) {
  const base::Value* visibility_container = action.FindKey("visibility");
  if (!visibility_container) {
    // By default, set the visibility to (visible | enabled | on_top), as
    // defined in
    // chrome/test/data/web_page_replay_go_helper_scripts/automation_helper.js
    *visibility_enum_val = 7;
    return true;
  }

  if (base::Value::Type::INTEGER != visibility_container->type()) {
    ADD_FAILURE() << "visibility property is not an integer!";
    return false;
  }

  *visibility_enum_val = visibility_container->GetInt();
  return true;
}

bool TestRecipeReplayer::GetTargetFrameFromAction(
    const base::DictionaryValue& action,
    content::RenderFrameHost** frame) {
  const base::Value* iframe_container = action.FindKey("context");
  if (!iframe_container) {
    ADD_FAILURE() << "Failed to extract the iframe context from action!";
    return false;
  }

  const base::DictionaryValue* iframe;
  if (!iframe_container->GetAsDictionary(&iframe)) {
    ADD_FAILURE() << "Failed to extract the iframe context object!";
    return false;
  }

  const base::Value* is_iframe_container = iframe->FindKey("isIframe");
  if (!is_iframe_container) {
    ADD_FAILURE()
        << "Failed to extract the isIframe field from the iframe context!";
    return false;
  }

  if (base::Value::Type::BOOLEAN != is_iframe_container->type()) {
    ADD_FAILURE() << "isIframe is not a boolean value!";
    return false;
  }

  if (!is_iframe_container->GetBool()) {
    *frame = GetWebContents()->GetMainFrame();
    return true;
  }

  const base::Value* frame_name_container =
      iframe->FindPath({"browserTest", "name"});
  const base::Value* frame_origin_container =
      iframe->FindPath({"browserTest", "origin"});
  const base::Value* frame_url_container =
      iframe->FindPath({"browserTest", "url"});
  IFrameWaiter iframe_waiter(GetWebContents());

  if (frame_name_container != nullptr &&
      base::Value::Type::STRING != frame_name_container->type()) {
    ADD_FAILURE() << "Iframe name is not a string!";
    return false;
  }

  if (frame_origin_container != nullptr &&
      base::Value::Type::STRING != frame_origin_container->type()) {
    ADD_FAILURE() << "Iframe origin is not a string!";
    return false;
  }

  if (frame_url_container != nullptr &&
      base::Value::Type::STRING != frame_url_container->type()) {
    ADD_FAILURE() << "Iframe url is not a string!";
    return false;
  }

  if (frame_name_container != nullptr) {
    std::string frame_name = frame_name_container->GetString();
    *frame = iframe_waiter.WaitForFrameMatchingName(frame_name);
  } else if (frame_origin_container != nullptr) {
    std::string frame_origin = frame_origin_container->GetString();
    *frame = iframe_waiter.WaitForFrameMatchingOrigin(GURL(frame_origin));
  } else if (frame_url_container != nullptr) {
    std::string frame_url = frame_url_container->GetString();
    *frame = iframe_waiter.WaitForFrameMatchingUrl(GURL(frame_url));
  } else {
    ADD_FAILURE() << "The recipe does not specify a way to find the iframe!";
  }

  if (frame == nullptr) {
    ADD_FAILURE() << "Failed to find iframe!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::GetIFramePathFromAction(
    const base::DictionaryValue& action,
    std::vector<std::string>* iframe_path) {
  *iframe_path = std::vector<std::string>();

  const base::Value* iframe_container = action.FindKey("context");
  if (!iframe_container) {
    ADD_FAILURE() << "Failed to extract the iframe context from action!";
    return false;
  }

  const base::DictionaryValue* iframe;
  if (!iframe_container->GetAsDictionary(&iframe)) {
    ADD_FAILURE() << "Failed to extract the iframe context object!";
    return false;
  }

  const base::Value* iframe_path_container = iframe->FindKey("path");
  if (!iframe_path_container) {
    // If the action does not have a path container, it would mean that:
    // 1. The target frame is the top level frame.
    // 2. The target frame is an iframe, but it is the top-level frame in its
    //    rendering process.
    return true;
  }

  if (base::Value::Type::LIST != iframe_path_container->type()) {
    ADD_FAILURE() << "The action's iframe path is not a list!";
    return false;
  }

  const base::Value::ListStorage& iframe_xpath_list =
      iframe_path_container->GetList();
  for (auto it_xpath = iframe_xpath_list.begin();
       it_xpath != iframe_xpath_list.end(); ++it_xpath) {
    std::string xpath;
    if (!it_xpath->GetAsString(&xpath)) {
      ADD_FAILURE() << "Failed to extract the iframe xpath from action!";
      return false;
    }
    iframe_path->push_back(xpath);
  }

  return true;
}

bool TestRecipeReplayer::GetIFrameOffsetFromIFramePath(
    const std::vector<std::string>& iframe_path,
    content::RenderFrameHost* frame,
    gfx::Vector2d* offset) {
  *offset = gfx::Vector2d(0, 0);

  for (auto it_xpath = iframe_path.begin(); it_xpath != iframe_path.end();
       it_xpath++) {
    content::RenderFrameHost* parent_frame = frame->GetParent();
    if (parent_frame == nullptr) {
      ADD_FAILURE() << "Trying to iterate past the top level frame!";
      return false;
    }

    gfx::Rect rect;
    if (!GetBoundingRectOfTargetElement(*it_xpath, parent_frame, &rect)) {
      ADD_FAILURE() << "Failed to extract position of iframe with xpath `"
                    << *it_xpath << "`!";
      return false;
    }

    *offset += rect.OffsetFromOrigin();
    frame = parent_frame;
  }

  return true;
}

bool TestRecipeReplayer::WaitForElementToBeReady(
    const std::string& xpath,
    const int visibility_enum_val,
    content::RenderFrameHost* frame) {
  std::vector<std::string> state_assertions;
  state_assertions.push_back(base::StringPrintf(
      "return automation_helper.isElementWithXpathReady(`%s`, %d);",
      xpath.c_str(), visibility_enum_val));
  return WaitForStateChange(frame, state_assertions, default_action_timeout);
}

bool TestRecipeReplayer::WaitForStateChange(
    content::RenderFrameHost* frame,
    const std::vector<std::string>& state_assertions,
    const base::TimeDelta& timeout) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  PageActivityObserver page_activity_observer(
      content::WebContents::FromRenderFrameHost(frame));
  while (!AllAssertionsPassed(frame, state_assertions)) {
    if (base::TimeTicks::Now() - start_time > timeout) {
      ADD_FAILURE() << "State change hasn't completed within timeout.";
      return false;
    }
    page_activity_observer.WaitTillPageIsIdle();
  }
  return true;
}

bool TestRecipeReplayer::AllAssertionsPassed(
    const content::ToRenderFrameHost& frame,
    const std::vector<std::string>& assertions) {
  for (const std::string& assertion : assertions) {
    bool assertion_passed = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        frame,
        base::StringPrintf("window.domAutomationController.send("
                           "    (function() {"
                           "      try {"
                           "        %s"
                           "      } catch (ex) {}"
                           "      return false;"
                           "    })());",
                           assertion.c_str()),
        &assertion_passed));
    if (!assertion_passed) {
      VLOG(1) << "'" << assertion << "' failed!";
      return false;
    }
  }
  return true;
}

bool TestRecipeReplayer::ExecuteJavaScriptOnElementByXpath(
    const content::ToRenderFrameHost& frame,
    const std::string& element_xpath,
    const std::string& execute_function_body,
    const base::TimeDelta& time_to_wait_for_element) {
  std::string js(base::StringPrintf(
      "try {"
      "  var element = automation_helper.getElementByXpath(`%s`);"
      "  (function(target) { %s })(element);"
      "} catch(ex) {}",
      element_xpath.c_str(), execute_function_body.c_str()));
  return ExecuteScript(frame, js);
}

bool TestRecipeReplayer::GetElementProperty(
    const content::ToRenderFrameHost& frame,
    const std::string& element_xpath,
    const std::string& get_property_function_body,
    std::string* property) {
  return ExecuteScriptAndExtractString(
      frame,
      base::StringPrintf(
          "window.domAutomationController.send("
          "    (function() {"
          "      try {"
          "        var element = function() {"
          "          return automation_helper.getElementByXpath(`%s`);"
          "        }();"
          "        return function(target){%s}(element);"
          "      } catch (ex) {}"
          "      return 'Exception encountered';"
          "    })());",
          element_xpath.c_str(), get_property_function_body.c_str()),
      property);
}

bool TestRecipeReplayer::ExpectElementPropertyEquals(
    const content::ToRenderFrameHost& frame,
    const std::string& element_xpath,
    const std::string& get_property_function_body,
    const std::string& expected_value,
    bool ignoreCase) {
  std::string value;
  if (!GetElementProperty(frame, element_xpath, get_property_function_body,
                          &value)) {
    ADD_FAILURE() << "Failed to extract element property! " << element_xpath
                  << ", " << get_property_function_body;
    return false;
  }

  if (ignoreCase) {
    EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(expected_value, value))
        << "Field xpath: `" << element_xpath << "`, "
        << "Expected: " << expected_value << ", actual: " << value;
  } else {
    EXPECT_EQ(expected_value, value)
        << "Field xpath: `" << element_xpath << "`, ";
  }
  return true;
}

bool TestRecipeReplayer::ScrollElementIntoView(
    const std::string& element_xpath,
    content::RenderFrameHost* frame) {
  const std::string scroll_target_js(base::StringPrintf(
      "try {"
      "  const element = automation_helper.getElementByXpath(`%s`);"
      "  element.scrollIntoView({"
      "    block: 'center', inline: 'center'});"
      "  window.domAutomationController.send(true);"
      "} catch(ex) {"
      "  window.domAutomationController.send(false);"
      "}",
      element_xpath.c_str()));

  bool succeeded = false;
  if (!ExecuteScriptAndExtractBool(frame, scroll_target_js, &succeeded)) {
    ADD_FAILURE() << "Failed to scroll the element into view with JavaScript!";
    return false;
  }

  return true;
}

bool TestRecipeReplayer::PlaceFocusOnElement(
    const std::string& element_xpath,
    const std::vector<std::string> iframe_path,
    content::RenderFrameHost* frame) {
  if (!ScrollElementIntoView(element_xpath, frame))
    return false;

  const std::string focus_on_target_field_js(base::StringPrintf(
      "try {"
      "  function onFocusHandler(event) {"
      "    event.target.removeEventListener(event.type, arguments.callee);"
      "    window.domAutomationController.send(true);"
      "  }"
      "  const element = automation_helper.getElementByXpath(`%s`);"
      "  if (document.activeElement === element) {"
      "    window.domAutomationController.send(true);"
      "  } else {"
      "    element.addEventListener('focus', onFocusHandler);"
      "    element.focus();"
      "  }"
      "  setTimeout(() => {"
      "    element.removeEventListener('focus', onFocusHandler);"
      "    window.domAutomationController.send(false);"
      "  }, 1000);"
      "} catch(ex) {"
      "  window.domAutomationController.send(false);"
      "}",
      element_xpath.c_str()));

  bool focused = false;
  if (!ExecuteScriptAndExtractBool(frame, focus_on_target_field_js, &focused)) {
    ADD_FAILURE() << "Failed to place focus on the element with JavaScript!";
    return false;
  }

  if (focused) {
    return true;
  } else {
    // Failing focusing on an element through script, use the less preferred
    // method of left mouse clicking the element.
    gfx::Rect rect;
    if (!GetBoundingRectOfTargetElement(element_xpath, iframe_path, frame,
                                        &rect))
      return false;

    return SimulateLeftMouseClickAt(rect.CenterPoint(), frame);
  }
}

bool TestRecipeReplayer::GetBoundingRectOfTargetElement(
    const std::string& target_element_xpath,
    content::RenderFrameHost* frame,
    gfx::Rect* output_rect) {
  std::string rect_str;
  const std::string get_element_bounding_rect_js(base::StringPrintf(
      "window.domAutomationController.send("
      "    (function() {"
      "       try {"
      "         const element = automation_helper.getElementByXpath(`%s`);"
      "         const rect = element.getBoundingClientRect();"
      "         return Math.round(rect.left) + ',' + "
      "                Math.round(rect.top) + ',' + "
      "                Math.round(rect.width) + ',' + "
      "                Math.round(rect.height);"
      "       } catch(ex) {}"
      "       return '';"
      "    })());",
      target_element_xpath.c_str()));

  if (!content::ExecuteScriptAndExtractString(
          frame, get_element_bounding_rect_js, &rect_str)) {
    ADD_FAILURE()
        << "Failed to run script to extract target element's bounding rect!";
    return false;
  }

  if (rect_str.empty()) {
    ADD_FAILURE() << "Failed to extract target element's bounding rect!";
    return false;
  }

  // Parse the bounding rect string to extract the element coordinates.
  std::istringstream rect_stream(rect_str);
  std::string token;
  if (!std::getline(rect_stream, token, ',')) {
    ADD_FAILURE() << "Failed to extract target element's x coordinate from "
                  << "the string `" << rect_str << "`!";
    return false;
  }

  output_rect->set_x(std::stoi(token));

  if (!std::getline(rect_stream, token, ',')) {
    ADD_FAILURE() << "Failed to extract target element's y coordinate from "
                  << "the string `" << rect_str << "`!";
    return false;
  }

  output_rect->set_y(std::stoi(token));

  if (!std::getline(rect_stream, token, ',')) {
    ADD_FAILURE() << "Failed to extract target element's width from "
                  << "the string `" << rect_str << "`!";
    return false;
  }

  output_rect->set_width(std::stoi(token));

  if (!std::getline(rect_stream, token, ',')) {
    ADD_FAILURE() << "Failed to extract target element's height from "
                  << "the string `" << rect_str << "`!";
    return false;
  }

  output_rect->set_height(std::stoi(token));

  return true;
}

bool TestRecipeReplayer::GetBoundingRectOfTargetElement(
    const std::string& target_element_xpath,
    const std::vector<std::string> iframe_path,
    content::RenderFrameHost* frame,
    gfx::Rect* output_rect) {
  gfx::Vector2d offset;
  if (!GetIFrameOffsetFromIFramePath(iframe_path, frame, &offset))
    return false;
  if (!GetBoundingRectOfTargetElement(target_element_xpath, frame, output_rect))
    return false;

  *output_rect += offset;
  return true;
}

bool TestRecipeReplayer::SimulateLeftMouseClickAt(
    const gfx::Point& point,
    content::RenderFrameHost* render_frame_host) {
  content::RenderWidgetHostView* view = render_frame_host->GetView();
  if (!SimulateMouseHoverAt(render_frame_host, point))
    return false;

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(point.x(), point.y());

  // Mac needs positionInScreen for events to plugins.
  gfx::Rect offset =
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetContainerBounds();
  mouse_event.SetPositionInScreen(point.x() + offset.x(),
                                  point.y() + offset.y());
  mouse_event.click_count = 1;
  content::RenderWidgetHost* widget = view->GetRenderWidgetHost();

  widget->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  widget->ForwardMouseEvent(mouse_event);
  return true;
}

bool TestRecipeReplayer::SimulateMouseHoverAt(
    content::RenderFrameHost* render_frame_host,
    const gfx::Point& point) {
  gfx::Rect offset =
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetContainerBounds();
  gfx::Point reset_mouse =
      gfx::Point(offset.x() + point.x(), offset.y() + point.y());
  if (!ui_test_utils::SendMouseMoveSync(reset_mouse)) {
    ADD_FAILURE() << "Failed to position the mouse!";
    return false;
  }
  return true;
}

void TestRecipeReplayer::NavigateAwayAndDismissBeforeUnloadDialog() {
  content::PrepContentsForBeforeUnloadTest(GetWebContents());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  app_modal::JavaScriptAppModalDialog* alert =
      ui_test_utils::WaitForAppModalDialog();
  alert->native_dialog()->AcceptAppModalDialog();
}

bool TestRecipeReplayer::HasChromeStoredCredential(
    const base::DictionaryValue& action,
    bool* stored_cred) {
  const base::Value* orgin_container = action.FindKey("origin");

  if (!orgin_container) {
    ADD_FAILURE() << "Failed to extract the origin from the action!";
    return false;
  }

  if (base::Value::Type::STRING != orgin_container->type()) {
    ADD_FAILURE() << "Origin is not a string!";
    return false;
  }

  const base::Value* user_name_container = action.FindKey("userName");

  if (!user_name_container) {
    ADD_FAILURE() << "Failed to extract the user name from the action!";
    return false;
  }

  if (base::Value::Type::STRING != user_name_container->type()) {
    ADD_FAILURE() << "User name is not a string!";
    return false;
  }

  const base::Value* password_container = action.FindKey("password");

  if (!password_container) {
    ADD_FAILURE() << "Failed to extract the password from the action!";
    return false;
  }

  if (base::Value::Type::STRING != password_container->type()) {
    ADD_FAILURE() << "Password is not a string!";
    return false;
  }

  *stored_cred = feature_action_executor()->HasChromeStoredCredential(
      orgin_container->GetString(), user_name_container->GetString(),
      password_container->GetString());

  return true;
}

bool TestRecipeReplayer::SetupSavedAutofillProfile(
    const base::Value& saved_autofill_profile_container) {
  if (base::Value::Type::LIST != saved_autofill_profile_container.type()) {
    ADD_FAILURE() << "Save Autofill Profile is not a list!";
    return false;
  }

  const base::Value::ListStorage& profile_entries_list =
      saved_autofill_profile_container.GetList();
  for (auto it_entry = profile_entries_list.begin();
       it_entry != profile_entries_list.end(); ++it_entry) {
    const base::DictionaryValue* entry;
    if (!it_entry->GetAsDictionary(&entry)) {
      ADD_FAILURE() << "Failed to extract an entry!";
      return false;
    }

    const base::Value* type_container = entry->FindKey("type");
    if (base::Value::Type::STRING != type_container->type()) {
      ADD_FAILURE() << "Type is not a string!";
      return false;
    }
    const std::string type = type_container->GetString();

    const base::Value* value_container = entry->FindKey("value");
    if (base::Value::Type::STRING != value_container->type()) {
      ADD_FAILURE() << "Value is not a string!";
      return false;
    }
    const std::string value = value_container->GetString();

    if (!feature_action_executor()->AddAutofillProfileInfo(type, value)) {
      return false;
    }
  }

  // Skip this step if autofill profile is empty.
  // Only Autofill Captured Sites test recipes will have non-empty autofill
  // profiles. Recipes for other captured sites tests will have empty autofill
  // profiles. This block prevents these other tests from failing because
  // the test feature action executor does not know how to setup the autofill
  // profile.
  if (profile_entries_list.size() == 0) {
    return true;
  }

  return feature_action_executor()->SetupAutofillProfile();
}

bool TestRecipeReplayer::SetupSavedPasswords(
    const base::Value& saved_password_list_container) {
  if (base::Value::Type::LIST != saved_password_list_container.type()) {
    ADD_FAILURE() << "Saved Password List is not a list!";
    return false;
  }

  const base::Value::ListStorage& saved_password_list =
      saved_password_list_container.GetList();
  for (auto it_password = saved_password_list.begin();
       it_password != saved_password_list.end(); ++it_password) {
    const base::DictionaryValue* cred;
    if (!it_password->GetAsDictionary(&cred)) {
      ADD_FAILURE() << "Failed to extract a saved password!";
      return false;
    }

    const base::Value* origin_container = cred->FindKey("website");
    if (base::Value::Type::STRING != origin_container->type()) {
      ADD_FAILURE() << "Website is not a string!";
      return false;
    }
    const std::string origin = origin_container->GetString();

    const base::Value* username_container = cred->FindKey("username");
    if (base::Value::Type::STRING != username_container->type()) {
      ADD_FAILURE() << "User name is not a string!";
      return false;
    }
    const std::string username = username_container->GetString();

    const base::Value* password_container = cred->FindKey("password");
    if (base::Value::Type::STRING != password_container->type()) {
      ADD_FAILURE() << "User name is not a string!";
      return false;
    }
    const std::string password = password_container->GetString();

    if (!feature_action_executor()->AddCredential(origin, username, password)) {
      return false;
    }
  }

  return true;
}

// TestRecipeReplayChromeFeatureActionExecutor --------------------------------
TestRecipeReplayChromeFeatureActionExecutor::
    TestRecipeReplayChromeFeatureActionExecutor() {}
TestRecipeReplayChromeFeatureActionExecutor::
    ~TestRecipeReplayChromeFeatureActionExecutor() {}

bool TestRecipeReplayChromeFeatureActionExecutor::AutofillForm(
    const std::string& focus_element_css_selector,
    const std::vector<std::string> iframe_path,
    const int attempts,
    content::RenderFrameHost* frame) {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor::AutofillForm "
                   "is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::AddAutofillProfileInfo(
    const std::string& field_type,
    const std::string& field_value) {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::AddAutofillProfileInfo is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::SetupAutofillProfile() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::SetupAutofillProfile is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::AddCredential(
    const std::string& origin,
    const std::string& username,
    const std::string& password) {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor::AddCredential"
                   " is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::SavePassword() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor::SavePassword"
                   " is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::UpdatePassword() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::UpdatePassword is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::WaitForSaveFallback() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::WaitForSaveFallback is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::
    HasChromeShownSavePasswordPrompt() {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::HasChromeShownSavePasswordPrompt is not implemented!";
  return false;
}

bool TestRecipeReplayChromeFeatureActionExecutor::HasChromeStoredCredential(
    const std::string& origin,
    const std::string& username,
    const std::string& password) {
  ADD_FAILURE() << "TestRecipeReplayChromeFeatureActionExecutor"
                   "::HasChromeStoredCredential is not implemented!";
  return false;
}

}  // namespace captured_sites_test_utils
