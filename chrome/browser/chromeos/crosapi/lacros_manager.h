// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_LACROS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_LACROS_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace component_updater {
class CrOSComponentManager;
}  // namespace component_updater

class AshChromeServiceImpl;
class LacrosLoader;

// Manages the lifetime of lacros-chrome, and its loading status.
class LacrosManager : public session_manager::SessionManagerObserver {
 public:
  // Static getter of LacrosManager instance. In real use cases,
  // LacrosManager instance should be unique in the process.
  static LacrosManager* Get();

  explicit LacrosManager(
      scoped_refptr<component_updater::CrOSComponentManager> manager);

  LacrosManager(const LacrosManager&) = delete;
  LacrosManager& operator=(const LacrosManager&) = delete;

  ~LacrosManager() override;

  // Returns true if the binary is ready to launch or already launched.
  // Typical usage is to check IsReady(), then if it returns false,
  // call SetLoadCompleteCallback() to be notified when the download completes.
  bool IsReady() const;

  // Sets a callback to be called when the binary download completes. The
  // download may not be successful.
  using LoadCompleteCallback = base::OnceCallback<void(bool success)>;
  void SetLoadCompleteCallback(LoadCompleteCallback callback);

  // Opens the browser window in lacros-chrome.
  // If lacros-chrome is not yet launched, it triggers to launch.
  // This needs to be called after loading. The condition can be checked
  // IsReady(), and if not yet, SetLoadCompletionCallback can be used
  // to wait for the loading.
  // TODO(crbug.com/1101676): Notify callers the result of opening window
  // request. Because of asynchronous operations crossing processes,
  // there's no guarantee that the opening window request succeeds.
  // Currently, its condition and result are completely hidden behind this
  // class, so there's no way for callers to handle such error cases properly.
  // This design often leads the flakiness behavior of the product and testing,
  // so should be avoided.
  void Start();

 private:
  enum class State {
    // Lacros is not initialized yet.
    // Lacros-chrome loading depends on user type, so it needs to wait
    // for user session.
    NOT_INITIALIZED,

    // User session started, and now it's loading (downloading and installing)
    // lacros-chrome.
    LOADING,

    // Lacros-chrome is unavailable. I.e., failed to load for some reason
    // or disabled.
    UNAVAILABLE,

    // Lacros-chrome is loaded and ready for launching.
    STOPPED,

    // Lacros-chrome is launching.
    STARTING,

    // Mojo connection to lacros-chrome is established so, it's in
    // the running state.
    RUNNING,

    // Lacros-chrome is being terminated soon.
    TERMINATING,
  };

  // Starting Lacros requires a hop to a background thread. The flow is
  // Start(), then StartBackground() in (the anonymous namespace),
  // then StartForeground().
  // The parameter |already_running| refers to whether the Lacros binary is
  // already launched and running.
  void StartForeground(bool already_running);

  // Called when PendingReceiver of AshChromeService is passed from
  // lacros-chrome.
  void OnAshChromeServiceReceiverReceived(
      mojo::PendingReceiver<crosapi::mojom::AshChromeService> pending_receiver);

  // Called when the Mojo connection to lacros-chrome is disconnected.
  // It may be "just a Mojo error" or "lacros-chrome crash".
  // In either case, terminates lacros-chrome, because there's no longer a
  // way to communicate with lacros-chrome.
  void OnMojoDisconnected();

  // Called when lacros-chrome is terminated and successfully wait(2)ed.
  void OnLacrosChromeTerminated();

  // session_manager::SessionManagerObserver:
  // Starts to load the lacros-chrome executable.
  void OnUserSessionStarted(bool is_primary_user) override;

  // Called on load completion.
  void OnLoadComplete(const base::FilePath& path);

  State state_ = State::NOT_INITIALIZED;

  int num_pending_start_ = 0;

  // May be null in tests.
  scoped_refptr<component_updater::CrOSComponentManager> component_manager_;

  std::unique_ptr<LacrosLoader> lacros_loader_;

  // Path to the lacros-chrome disk image directory.
  base::FilePath lacros_path_;

  // Called when the binary download completes.
  LoadCompleteCallback load_complete_callback_;

  // Process handle for the lacros-chrome process.
  base::Process lacros_process_;

  // Proxy to LacrosChromeService mojo service in lacros-chrome.
  // Available during lacros-chrome is running.
  mojo::Remote<crosapi::mojom::LacrosChromeService> lacros_chrome_service_;

  // Implementation of AshChromeService Mojo APIs.
  // Instantiated on receiving the PendingReceiver from lacros-chrome.
  std::unique_ptr<AshChromeServiceImpl> ash_chrome_service_;

  base::WeakPtrFactory<LacrosManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_LACROS_MANAGER_H_
