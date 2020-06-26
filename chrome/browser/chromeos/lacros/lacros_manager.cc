// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/lacros_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/lacros/ash_chrome_service_impl.h"
#include "chrome/browser/chromeos/lacros/lacros_loader.h"
#include "chrome/browser/chromeos/lacros/lacros_util.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/mojom/base/binder.mojom.h"

namespace chromeos {
namespace {

// Pointer to the global instance of LacrosManager.
LacrosManager* g_instance = nullptr;

base::FilePath LacrosLogPath() {
  return lacros_util::GetUserDataDir().Append("lacros.log");
}

// TODO(https://crbug.com/1091863): This logic is not robust against the
// situation where Lacros has been killed, but another process was spawned
// with the same pid. This logic also relies on I/O, which we'd like to avoid
// if possible.
bool IsLacrosChromeInProc(base::ProcessId pid,
                          const base::FilePath& lacros_path) {
  // We avoid using WaitForExitWithTimeout() since that can block for up to
  // 256ms. Instead, we check existence of /proc/<pid>/cmdline and check for a
  // match against lacros_path_. This logic assumes that lacros_path_ is a fully
  // qualified path.
  base::FilePath cmdline_filepath("/proc");
  cmdline_filepath = cmdline_filepath.Append(base::NumberToString(pid));
  cmdline_filepath = cmdline_filepath.Append("cmdline");
  base::File cmdline_file = base::File(
      cmdline_filepath, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!cmdline_file.IsValid())
    return false;
  std::string expected_cmdline = lacros_path.value();
  size_t expected_length = expected_cmdline.size();
  char data[1000];
  int size_read = cmdline_file.Read(0, data, 1000);
  if (static_cast<size_t>(size_read) < expected_length)
    return false;
  return expected_cmdline.compare(0, expected_length, data, expected_length) ==
         0;
}

bool StartBackground(base::ProcessId pid, const base::FilePath& lacros_path) {
  bool already_running =
      pid != base::kNullProcessId && IsLacrosChromeInProc(pid, lacros_path);

  if (!already_running) {
    // Only delete the old log file if lacros is not running. If it's already
    // running, then the subsequent call to base::LaunchProcess opens a new
    // window, and we do not want to delete the existing log file.
    // TODO(erikchen): Currently, launching a second instance of chrome deletes
    // the existing log file, even though the new instance quickly exits.
    base::DeleteFile(LacrosLogPath());
  }

  return already_running;
}

std::string GetXdgRuntimeDir() {
  // If ash-chrome was given an environment variable, use it.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string xdg_runtime_dir;
  if (env->GetVar("XDG_RUNTIME_DIR", &xdg_runtime_dir))
    return xdg_runtime_dir;

  // Otherwise provide the default for Chrome OS devices.
  return "/run/chrome";
}

}  // namespace

// static
LacrosManager* LacrosManager::Get() {
  return g_instance;
}

LacrosManager::LacrosManager(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : component_manager_(manager) {
  DCHECK(!g_instance);
  g_instance = this;

  // Wait to query the flag until the user has entered the session. Enterprise
  // devices restart Chrome during login to apply flags. We don't want to run
  // the flag-off cleanup logic until we know we have the final flag state.
  session_manager::SessionManager::Get()->AddObserver(this);
}

LacrosManager::~LacrosManager() {
  // Unregister, just in case the manager is destroyed before
  // OnUserSessionStarted() is called.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  // Try to kill the lacros-chrome binary.
  if (lacros_process_.IsValid())
    lacros_process_.Terminate(/*ignored=*/0, /*wait=*/false);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

bool LacrosManager::IsReady() const {
  return !lacros_path_.empty();
}

void LacrosManager::SetLoadCompleteCallback(LoadCompleteCallback callback) {
  load_complete_callback_ = std::move(callback);
}

void LacrosManager::Start() {
  if (!lacros_util::IsLacrosAllowed())
    return;

  if (lacros_path_.empty()) {
    LOG(WARNING) << "lacros component image not yet available";
    return;
  }

  // Because we haven't yet handled process termination of lacros-chrome,
  // lacros_process_ may point to a stale process. Check it by looking at
  // procfs in a background task runner in addition.
  // TODO(hidehiko): Handle the process termination correctly after mojo
  // connection available.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  base::ProcessId pid =
      lacros_process_.IsValid() ? lacros_process_.Pid() : base::kNullProcessId;
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&StartBackground, pid, lacros_path_),
      base::BindOnce(&LacrosManager::StartForeground,
                     weak_factory_.GetWeakPtr()));
}

void LacrosManager::StartForeground(bool already_running) {
  DCHECK(!lacros_path_.empty());

  std::string chrome_path = lacros_path_.MaybeAsASCII() + "/chrome";
  LOG(WARNING) << "Launching lacros-chrome at " << chrome_path;

  base::LaunchOptions options;
  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = GetXdgRuntimeDir();

  std::string api_key;
  if (google_apis::HasAPIKeyConfigured())
    api_key = google_apis::GetAPIKey();
  else
    api_key = google_apis::GetNonStableAPIKey();
  options.environment["GOOGLE_API_KEY"] = api_key;
  options.environment["GOOGLE_DEFAULT_CLIENT_ID"] =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  options.environment["GOOGLE_DEFAULT_CLIENT_SECRET"] =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);

  options.kill_on_parent_death = true;

  // Paths are UTF-8 safe on Chrome OS.
  std::string user_data_dir = lacros_util::GetUserDataDir().AsUTF8Unsafe();

  std::vector<std::string> argv = {chrome_path,
                                   "--ozone-platform=wayland",
                                   "--user-data-dir=" + user_data_dir,
                                   "--enable-gpu-rasterization",
                                   "--enable-oop-rasterization",
                                   "--lang=en-US",
                                   "--enable-crashpad"};

  // We assume that if there's a custom chrome path, that this is a developer
  // and they want to enable logging.
  bool custom_chrome_path = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kLacrosChromePath);
  if (custom_chrome_path) {
    argv.push_back("--enable-logging");
    argv.push_back("--log-file=" + LacrosLogPath().value());
  }

  if (already_running) {
    // If Lacros is already running, then the new call to launch process spawns
    // a new window but does not create a lasting process.
    // TODO(erikchen): we should send a mojo signal to open a new tab rather
    // than going through the start flow again.
    base::LaunchProcess(argv, options);
  } else {
    // Set up Mojo channel.
    base::CommandLine command_line(argv);
    mojo::PlatformChannel channel;
    channel.PrepareToPassRemoteEndpoint(&options, &command_line);

    base::RecordAction(base::UserMetricsAction("Lacros.Launch"));
    // If lacros_process_ already exists, because it does not call waitpid(2),
    // the process will never be collected.
    // TODO(hidehiko): Fix the case by collecting the processes.
    lacros_process_ = base::LaunchProcess(command_line, options);

    // TODO(hidehiko): Clean up the set-up procedure.
    // Replacing the "already_running" case by Mojo call allows us to
    // simplify the code.
    if (lacros_process_.IsValid()) {
      channel.RemoteProcessLaunchAttempted();
      mojo::OutgoingInvitation invitation;
      mojo::Remote<mojo_base::mojom::Binder> binder(
          mojo::PendingRemote<mojo_base::mojom::Binder>(
              invitation.AttachMessagePipe(0), /*version=*/0));
      mojo::OutgoingInvitation::Send(std::move(invitation),
                                     lacros_process_.Handle(),
                                     channel.TakeLocalEndpoint());
      binder->Bind(lacros_chrome_service_.BindNewPipeAndPassReceiver());
      lacros_chrome_service_->RequestAshChromeServiceReceiver(
          base::BindOnce(&LacrosManager::OnAshChromeServiceReceiverReceived,
                         weak_factory_.GetWeakPtr()));
    }
  }
  LOG(WARNING) << "Launched lacros-chrome with pid " << lacros_process_.Pid();
}

void LacrosManager::OnAshChromeServiceReceiverReceived(
    mojo::PendingReceiver<lacros::mojom::AshChromeService> pending_receiver) {
  ash_chrome_service_ =
      std::make_unique<AshChromeServiceImpl>(std::move(pending_receiver));
}

void LacrosManager::OnUserSessionStarted(bool is_primary_user) {
  // Ensure this isn't called multiple times.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  // Must be checked after user session start because it depends on user type.
  if (!lacros_util::IsLacrosAllowed())
    return;

  // May be null in tests.
  if (!component_manager_)
    return;

  DCHECK(!lacros_loader_);
  lacros_loader_ = std::make_unique<LacrosLoader>(component_manager_);
  if (chromeos::features::IsLacrosSupportEnabled()) {
    lacros_loader_->Load(base::BindOnce(&LacrosManager::OnLoadComplete,
                                        weak_factory_.GetWeakPtr()));
  } else {
    lacros_loader_->Unload();
  }
}

void LacrosManager::OnLoadComplete(const base::FilePath& path) {
  lacros_path_ = path;
  if (load_complete_callback_) {
    const bool success = !path.empty();
    std::move(load_complete_callback_).Run(success);
  }
}

}  // namespace chromeos
