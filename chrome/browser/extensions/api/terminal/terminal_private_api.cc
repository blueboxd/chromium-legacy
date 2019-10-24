// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/terminal_private.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/process_proxy/process_proxy_registry.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"

namespace terminal_private = extensions::api::terminal_private;
namespace OnTerminalResize =
    extensions::api::terminal_private::OnTerminalResize;
namespace OpenTerminalProcess =
    extensions::api::terminal_private::OpenTerminalProcess;
namespace CloseTerminalProcess =
    extensions::api::terminal_private::CloseTerminalProcess;
namespace SendInput = extensions::api::terminal_private::SendInput;
namespace AckOutput = extensions::api::terminal_private::AckOutput;

using crostini::mojom::InstallerState;

namespace {

const char kCroshName[] = "crosh";
const char kCroshCommand[] = "/usr/bin/crosh";
// We make stubbed crosh just echo back input.
const char kStubbedCroshCommand[] = "cat";

const char kVmShellName[] = "vmshell";
const char kVmShellCommand[] = "/usr/bin/vsh";

const char kSwitchOwnerId[] = "owner_id";
const char kSwitchVmName[] = "vm_name";
const char kSwitchTargetContainer[] = "target_container";
const char kSwitchStartupId[] = "startup_id";

// Copies the value of |switch_name| if present from |src| to |dst|.  If not
// present, uses |default_value|.  Returns the value set into |dst|.
std::string GetSwitch(const base::CommandLine* src,
                      base::CommandLine* dst,
                      const std::string& switch_name,
                      const std::string& default_value) {
  std::string result = src->HasSwitch(switch_name)
                           ? src->GetSwitchValueASCII(switch_name)
                           : default_value;
  dst->AppendSwitchASCII(switch_name, result);
  return result;
}

void NotifyProcessOutput(content::BrowserContext* browser_context,
                         int tab_id,
                         const std::string& terminal_id,
                         const std::string& output_type,
                         const std::string& output) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&NotifyProcessOutput, browser_context, tab_id,
                                  terminal_id, output_type, output));
    return;
  }

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->AppendInteger(tab_id);
  args->AppendString(terminal_id);
  args->AppendString(output_type);
  args->AppendString(output);

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(browser_context);
  if (event_router) {
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::TERMINAL_PRIVATE_ON_PROCESS_OUTPUT,
        terminal_private::OnProcessOutput::kEventName, std::move(args)));
    event_router->BroadcastEvent(std::move(event));
  }
}

// Returns tab ID, or window session ID (for platform apps) for |web_contents|.
int GetTabOrWindowSessionId(content::BrowserContext* browser_context,
                            content::WebContents* web_contents) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id >= 0)
    return tab_id;
  extensions::AppWindow* window =
      extensions::AppWindowRegistry::Get(browser_context)
          ->GetAppWindowForWebContents(web_contents);
  return window ? window->session_id().id() : -1;
}

class CrostiniRestartObserver
    : public crostini::CrostiniManager::RestartObserver {
 public:
  CrostiniRestartObserver(
      base::RepeatingCallback<void(const std::string&)> print,
      base::OnceClosure callback)
      : print_(std::move(print)), callback_(std::move(callback)) {}

  void OnCrostiniRestarted(crostini::CrostiniResult result) {
    if (result != crostini::CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Error starting crostini for terminal: "
                 << static_cast<int>(result);
      PrintWithTimestamp(base::StringPrintf(
          "Error starting penguin container: %d\r\n", result));
    } else {
      PrintWithTimestamp("Ready\r\n");
    }
    std::move(callback_).Run();
    delete this;
  }

 private:
  // crostini::CrostiniManager::RestartObserver
  void OnStageStarted(InstallerState stage) override {
    switch (stage) {
      case InstallerState::kStart:
        PrintWithTimestamp("Chrome OS " + version_info::GetVersionNumber() +
                           " " +
                           chromeos::version_loader::GetVersion(
                               chromeos::version_loader::VERSION_FULL) +
                           "\r\n");
        PrintWithTimestamp("Starting terminal...\r\n");
        break;
      case InstallerState::kInstallImageLoader:
        PrintWithTimestamp("Checking cros-termina component... ");
        break;
      case InstallerState::kStartConcierge:
        PrintWithTimestamp("Starting VM controller... ");
        break;
      case InstallerState::kCreateDiskImage:
        PrintWithTimestamp("Creating termina VM image... ");
        break;
      case InstallerState::kStartTerminaVm:
        PrintWithTimestamp("Starting termina VM... ");
        break;
      case InstallerState::kCreateContainer:
        PrintWithTimestamp("Creating penguin container... ");
        break;
      case InstallerState::kSetupContainer:
        PrintWithTimestamp("Checking penguin container setup... ");
        break;
      case InstallerState::kStartContainer:
        PrintWithTimestamp("Starting penguin container... ");
        break;
      case InstallerState::kFetchSshKeys:
        PrintWithTimestamp("Fetching penguin container ssh keys... ");
        break;
      case InstallerState::kMountContainer:
        PrintWithTimestamp("Mounting penguin container sshfs... ");
        break;
    }
  }
  void OnComponentLoaded(crostini::CrostiniResult result) override {
    PrintCrostiniResult(result);
  }
  void OnConciergeStarted(bool success) override { PrintSuccess(success); }
  void OnDiskImageCreated(bool success,
                          vm_tools::concierge::DiskImageStatus status,
                          int64_t disk_size_available) override {
    PrintSuccess(success);
  }
  void OnVmStarted(bool success) override { PrintSuccess(success); }
  void OnContainerDownloading(int32_t download_percent) override { Print("."); }
  void OnContainerCreated(crostini::CrostiniResult result) override {
    PrintCrostiniResult(result);
  }
  void OnContainerSetup(bool success) override { PrintSuccess(success); }
  void OnContainerStarted(crostini::CrostiniResult result) override {
    PrintCrostiniResult(result);
  }
  void OnSshKeysFetched(bool success) override { PrintSuccess(success); }
  void OnContainerMounted(bool success) override { PrintSuccess(success); }

  void Print(const std::string& output) { print_.Run(output); }
  void PrintWithTimestamp(const std::string& output) {
    base::Time::Exploded exploded;
    base::Time::Now().LocalExplode(&exploded);
    Print(base::StringPrintf(
        "%04d-%02d-%02d %02d:%02d:%02d.%03d %s", exploded.year, exploded.month,
        exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
        exploded.millisecond, output.c_str()));
  }
  void Println(const std::string& output) { Print(output + "\r\n"); }
  void PrintCrostiniResult(crostini::CrostiniResult result) {
    if (result == crostini::CrostiniResult::SUCCESS) {
      Println("done");
    } else {
      Println(base::StringPrintf("error=%d", result));
    }
  }
  void PrintSuccess(bool success) { Println(success ? "done" : "error"); }

  base::RepeatingCallback<void(const std::string& output)> print_;
  base::OnceClosure callback_;
};

}  // namespace

namespace extensions {

TerminalPrivateOpenTerminalProcessFunction::
    TerminalPrivateOpenTerminalProcessFunction() {}

TerminalPrivateOpenTerminalProcessFunction::
    ~TerminalPrivateOpenTerminalProcessFunction() {}

ExtensionFunction::ResponseAction
TerminalPrivateOpenTerminalProcessFunction::Run() {
  std::unique_ptr<OpenTerminalProcess::Params> params(
      OpenTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const std::string& user_id_hash =
      extensions::ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          browser_context());
  content::WebContents* caller_contents = GetSenderWebContents();
  if (!caller_contents)
    return RespondNow(Error("No web contents."));

  // Passed to terminalPrivate.ackOutput, which is called from the API's custom
  // bindings after terminalPrivate.onProcessOutput is dispatched. It is used to
  // determine whether ackOutput call should be handled or not. ackOutput will
  // be called from every web contents in which a onProcessOutput listener
  // exists (because the API custom bindings hooks are run in every web contents
  // with a listener). Only ackOutput called from the web contents that has the
  // target terminal instance should be handled.
  // TODO(tbarzic): Instead of passing tab/app window session id around, keep
  //     mapping from web_contents to terminal ID running in it. This will be
  //     needed to fix crbug.com/210295.
  int tab_id = GetTabOrWindowSessionId(browser_context(), caller_contents);
  if (tab_id < 0)
    return RespondNow(Error("Not called from a tab or app window"));

  // Passing --crosh-command overrides any JS process name.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kCroshCommand)) {
    OpenProcess(user_id_hash, tab_id,
                {command_line->GetSwitchValueASCII(switches::kCroshCommand)});

  } else if (params->process_name == kCroshName) {
    // command=crosh: use '/usr/bin/crosh' on a device, 'cat' otherwise.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      OpenProcess(user_id_hash, tab_id, {kCroshCommand});
    } else {
      OpenProcess(user_id_hash, tab_id, {kStubbedCroshCommand});
    }

  } else if (params->process_name == kVmShellName) {
    // command=vmshell: ensure --owner_id, --vm_name, and --target_container are
    // set and the specified vm/container is running.
    base::CommandLine vmshell_cmd({kVmShellCommand});
    std::vector<std::string> args = {kVmShellCommand};
    if (params->args)
      args.insert(args.end(), params->args->begin(), params->args->end());
    base::CommandLine params_args(args);
    std::string owner_id =
        GetSwitch(&params_args, &vmshell_cmd, kSwitchOwnerId, user_id_hash);
    std::string vm_name = GetSwitch(&params_args, &vmshell_cmd, kSwitchVmName,
                                    crostini::kCrostiniDefaultVmName);
    std::string container_name =
        GetSwitch(&params_args, &vmshell_cmd, kSwitchTargetContainer,
                  crostini::kCrostiniDefaultContainerName);
    std::string startup_id = params_args.GetSwitchValueASCII(kSwitchStartupId);

    auto open_process =
        base::BindOnce(&TerminalPrivateOpenTerminalProcessFunction::OpenProcess,
                       this, user_id_hash, tab_id, vmshell_cmd.argv());
    auto* observer = new CrostiniRestartObserver(
        base::BindRepeating(&NotifyProcessOutput, browser_context(), tab_id,
                            startup_id,
                            api::terminal_private::ToString(
                                api::terminal_private::OUTPUT_TYPE_STDOUT)),
        std::move(open_process));
    crostini::CrostiniManager::GetForProfile(
        Profile::FromBrowserContext(browser_context()))
        ->RestartCrostini(
            vm_name, container_name,
            base::BindOnce(&CrostiniRestartObserver::OnCrostiniRestarted,
                           base::Unretained(observer)),
            observer);
  } else {
    // command=[unrecognized].
    return RespondNow(Error("Invalid process name: " + params->process_name));
  }
  return RespondLater();
}

void TerminalPrivateOpenTerminalProcessFunction::OpenProcess(
    const std::string& user_id_hash,
    int tab_id,
    const std::vector<std::string>& arguments) {
  DCHECK(!arguments.empty());
  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateOpenTerminalProcessFunction::OpenOnRegistryTaskRunner,
          this, base::Bind(&NotifyProcessOutput, browser_context(), tab_id),
          base::Bind(
              &TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread,
              this),
          arguments, user_id_hash));
}

void TerminalPrivateOpenTerminalProcessFunction::OpenOnRegistryTaskRunner(
    const ProcessOutputCallback& output_callback,
    const OpenProcessCallback& callback,
    const std::vector<std::string>& arguments,
    const std::string& user_id_hash) {
  chromeos::ProcessProxyRegistry* registry =
      chromeos::ProcessProxyRegistry::Get();
  const base::CommandLine cmdline{arguments};

  std::string terminal_id;
  bool success = registry->OpenProcess(cmdline, user_id_hash, output_callback,
                                       &terminal_id);

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(callback, success, terminal_id));
}

TerminalPrivateSendInputFunction::~TerminalPrivateSendInputFunction() {}

void TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread(
    bool success,
    const std::string& terminal_id) {
  if (!success) {
    Respond(Error("Failed to open process."));
    return;
  }
  Respond(OneArgument(std::make_unique<base::Value>(terminal_id)));
}

ExtensionFunction::ResponseAction TerminalPrivateSendInputFunction::Run() {
  std::unique_ptr<SendInput::Params> params(SendInput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateSendInputFunction::SendInputOnRegistryTaskRunner,
          this, params->id, params->input));
  return RespondLater();
}

void TerminalPrivateSendInputFunction::SendInputOnRegistryTaskRunner(
    const std::string& terminal_id,
    const std::string& text) {
  bool success =
      chromeos::ProcessProxyRegistry::Get()->SendInput(terminal_id, text);

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&TerminalPrivateSendInputFunction::RespondOnUIThread, this,
                     success));
}

void TerminalPrivateSendInputFunction::RespondOnUIThread(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

TerminalPrivateCloseTerminalProcessFunction::
    ~TerminalPrivateCloseTerminalProcessFunction() {}

ExtensionFunction::ResponseAction
TerminalPrivateCloseTerminalProcessFunction::Run() {
  std::unique_ptr<CloseTerminalProcess::Params> params(
      CloseTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&TerminalPrivateCloseTerminalProcessFunction::
                                    CloseOnRegistryTaskRunner,
                                this, params->id));

  return RespondLater();
}

void TerminalPrivateCloseTerminalProcessFunction::CloseOnRegistryTaskRunner(
    const std::string& terminal_id) {
  bool success =
      chromeos::ProcessProxyRegistry::Get()->CloseProcess(terminal_id);

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread, this,
          success));
}

void TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread(
    bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

TerminalPrivateOnTerminalResizeFunction::
    ~TerminalPrivateOnTerminalResizeFunction() {}

ExtensionFunction::ResponseAction
TerminalPrivateOnTerminalResizeFunction::Run() {
  std::unique_ptr<OnTerminalResize::Params> params(
      OnTerminalResize::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TerminalPrivateOnTerminalResizeFunction::
                         OnResizeOnRegistryTaskRunner,
                     this, params->id, params->width, params->height));

  return RespondLater();
}

void TerminalPrivateOnTerminalResizeFunction::OnResizeOnRegistryTaskRunner(
    const std::string& terminal_id,
    int width,
    int height) {
  bool success = chromeos::ProcessProxyRegistry::Get()->OnTerminalResize(
      terminal_id, width, height);

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread, this,
          success));
}

void TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

TerminalPrivateAckOutputFunction::~TerminalPrivateAckOutputFunction() {}

ExtensionFunction::ResponseAction TerminalPrivateAckOutputFunction::Run() {
  std::unique_ptr<AckOutput::Params> params(AckOutput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* caller_contents = GetSenderWebContents();
  if (!caller_contents)
    return RespondNow(Error("No web contents."));

  int tab_id = GetTabOrWindowSessionId(browser_context(), caller_contents);
  if (tab_id < 0)
    return RespondNow(Error("Not called from a tab or app window"));

  if (tab_id != params->tab_id)
    return RespondNow(NoArguments());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateAckOutputFunction::AckOutputOnRegistryTaskRunner,
          this, params->id));

  return RespondNow(NoArguments());
}

void TerminalPrivateAckOutputFunction::AckOutputOnRegistryTaskRunner(
    const std::string& terminal_id) {
  chromeos::ProcessProxyRegistry::Get()->AckOutput(terminal_id);
}

}  // namespace extensions
