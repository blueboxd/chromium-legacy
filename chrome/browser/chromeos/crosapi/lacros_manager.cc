// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/lacros_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/crosapi/ash_chrome_service_impl.h"
#include "chrome/browser/chromeos/crosapi/lacros_loader.h"
#include "chrome/browser/chromeos/crosapi/lacros_util.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/mojom/base/binder.mojom.h"

// TODO(crbug.com/1101667): Currently, this source has log spamming
// by LOG(WARNING) for non critical errors to make it easy
// to debug and develop. Get rid of the log spamming
// when it gets stable enough.

namespace {

// Pointer to the global instance of LacrosManager.
LacrosManager* g_instance = nullptr;

base::FilePath LacrosLogPath() {
  return lacros_util::GetUserDataDir().Append("lacros.log");
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

void TerminateLacrosChrome(base::Process process) {
  // Here, lacros-chrome process may crashed, or be in the shutdown procedure.
  // Give some amount of time for the collection. In most cases,
  // this wait captures the process termination.
  constexpr base::TimeDelta kGracefulShutdownTimeout =
      base::TimeDelta::FromSeconds(5);
  if (process.WaitForExitWithTimeout(kGracefulShutdownTimeout, nullptr))
    return;

  // Here, the process is not yet terminated.
  // This happens if some critical error happens on the mojo connection,
  // while both ash-chrome and lacros-chrome are still alive.
  // Terminate the lacros-chrome.
  bool success = process.Terminate(/*exit_code=*/0, /*wait=*/true);
  LOG_IF(ERROR, !success) << "Failed to terminate the lacros-chrome.";
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
  return state_ != State::NOT_INITIALIZED && state_ != State::LOADING &&
         state_ != State::UNAVAILABLE;
}

void LacrosManager::SetLoadCompleteCallback(LoadCompleteCallback callback) {
  load_complete_callback_ = std::move(callback);
}

void LacrosManager::NewWindow() {
  if (!lacros_util::IsLacrosAllowed())
    return;

  if (!IsReady()) {
    LOG(WARNING) << "lacros component image not yet available";
    return;
  }
  DCHECK(!lacros_path_.empty());

  if (state_ == State::TERMINATING) {
    LOG(WARNING) << "lacros-chrome is terminating, so cannot start now";
    return;
  }

  if (state_ == State::STOPPED) {
    // If lacros-chrome is not running, launch it.
    bool succeeded = Start();
    LOG_IF(ERROR, !succeeded)
        << "lacros-chrome failed to launch. Cannot open a window";
    return;
  }

  DCHECK(lacros_chrome_service_.is_connected());
  lacros_chrome_service_->NewWindow(base::DoNothing());
}

bool LacrosManager::Start() {
  DCHECK_EQ(state_, State::STOPPED);
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

  // Set up Mojo channel.
  base::CommandLine command_line(argv);
  mojo::PlatformChannel channel;
  channel.PrepareToPassRemoteEndpoint(&options, &command_line);

  // Create the lacros-chrome subprocess.
  base::RecordAction(base::UserMetricsAction("Lacros.Launch"));
  // If lacros_process_ already exists, because it does not call waitpid(2),
  // the process will never be collected.
  lacros_process_ = base::LaunchProcess(command_line, options);
  if (!lacros_process_.IsValid()) {
    LOG(ERROR) << "Failed to launch lacros-chrome";
    return false;
  }
  state_ = State::STARTING;
  LOG(WARNING) << "Launched lacros-chrome with pid " << lacros_process_.Pid();

  // Invite the lacros-chrome to the mojo universe, and bind
  // LacrosChromeService and AshChromeService interfaces to each other.
  channel.RemoteProcessLaunchAttempted();
  mojo::OutgoingInvitation invitation;
  mojo::Remote<mojo_base::mojom::Binder> binder(
      mojo::PendingRemote<mojo_base::mojom::Binder>(
          invitation.AttachMessagePipe(0), /*version=*/0));
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 lacros_process_.Handle(),
                                 channel.TakeLocalEndpoint());
  binder->Bind(lacros_chrome_service_.BindNewPipeAndPassReceiver());
  lacros_chrome_service_.set_disconnect_handler(base::BindOnce(
      &LacrosManager::OnMojoDisconnected, weak_factory_.GetWeakPtr()));
  lacros_chrome_service_->RequestAshChromeServiceReceiver(
      base::BindOnce(&LacrosManager::OnAshChromeServiceReceiverReceived,
                     weak_factory_.GetWeakPtr()));
  return true;
}

void LacrosManager::OnAshChromeServiceReceiverReceived(
    mojo::PendingReceiver<crosapi::mojom::AshChromeService> pending_receiver) {
  DCHECK_EQ(state_, State::STARTING);
  ash_chrome_service_ =
      std::make_unique<AshChromeServiceImpl>(std::move(pending_receiver));
  state_ = State::RUNNING;
  LOG(WARNING) << "Connection to lacros-chrome is established.";
}

void LacrosManager::OnMojoDisconnected() {
  DCHECK(state_ == State::STARTING || state_ == State::RUNNING);
  LOG(WARNING)
      << "Mojo to lacros-chrome is disconnected. Terminating lacros-chrome";
  state_ = State::TERMINATING;

  lacros_chrome_service_.reset();
  ash_chrome_service_ = nullptr;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&TerminateLacrosChrome, std::move(lacros_process_)),
      base::BindOnce(&LacrosManager::OnLacrosChromeTerminated,
                     weak_factory_.GetWeakPtr()));
}

void LacrosManager::OnLacrosChromeTerminated() {
  DCHECK_EQ(state_, State::TERMINATING);
  LOG(WARNING) << "Lacros-chrome is terminated";
  state_ = State::STOPPED;
}

void LacrosManager::OnUserSessionStarted(bool is_primary_user) {
  DCHECK_EQ(state_, State::NOT_INITIALIZED);

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
    state_ = State::LOADING;
    lacros_loader_->Load(base::BindOnce(&LacrosManager::OnLoadComplete,
                                        weak_factory_.GetWeakPtr()));
  } else {
    state_ = State::UNAVAILABLE;
    lacros_loader_->Unload();
  }
}

void LacrosManager::OnLoadComplete(const base::FilePath& path) {
  DCHECK_EQ(state_, State::LOADING);

  lacros_path_ = path;
  state_ = path.empty() ? State::UNAVAILABLE : State::STOPPED;
  if (load_complete_callback_) {
    const bool success = !path.empty();
    std::move(load_complete_callback_).Run(success);
  }
}
