// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LACROS_LACROS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LACROS_LACROS_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "chromeos/lacros/mojom/lacros.mojom.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace component_updater {
class CrOSComponentManager;
}  // namespace component_updater

namespace chromeos {

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

  // Returns true if the binary is ready to launch. Typical usage is to check
  // IsReady(), then if it returns false, call SetLoadCompleteCallback() to be
  // notified when the download completes.
  bool IsReady() const;

  // Sets a callback to be called when the binary download completes. The
  // download may not be successful.
  using LoadCompleteCallback = base::OnceCallback<void(bool success)>;
  void SetLoadCompleteCallback(LoadCompleteCallback callback);

  // Starts the lacros-chrome binary.
  // This needs to be called after loading. The condition can be checked
  // IsReady(), and if not yet, SetLoadCompletionCallback can be used
  // to wait for the loading.
  void Start();

 private:
  // Starting Lacros requires a hop to a background thread. The flow is
  // Start(), then StartBackground() in (the anonymous namespace),
  // then StartForeground().
  // The parameter |already_running| refers to whether the Lacros binary is
  // already launched and running.
  void StartForeground(bool already_running);

  // Called when PendingReceiver of AshChromeService is passed from
  // lacros-chrome.
  void OnAshChromeServiceReceiverReceived(
      mojo::PendingReceiver<lacros::mojom::AshChromeService> pending_receiver);

  // session_manager::SessionManagerObserver:
  // Starts to load the lacros-chrome executable.
  void OnUserSessionStarted(bool is_primary_user) override;

  // Called on load completion.
  void OnLoadComplete(const base::FilePath& path);

  // May be null in tests.
  scoped_refptr<component_updater::CrOSComponentManager> component_manager_;

  std::unique_ptr<LacrosLoader> lacros_loader_;

  // Path to the lacros-chrome disk image directory.
  base::FilePath lacros_path_;

  // Called when the binary download completes.
  LoadCompleteCallback load_complete_callback_;

  // Process handle for the lacros-chrome process.
  // TODO(https://crbug.com/1091863): There is currently no notification for
  // when lacros-chrome is killed, so the underlying pid may be pointing at a
  // non-existent process, or a new, unrelated process with the same pid.
  base::Process lacros_process_;

  // Proxy to LacrosChromeService mojo service in lacros-chrome.
  // Available during lacros-chrome is running.
  mojo::Remote<lacros::mojom::LacrosChromeService> lacros_chrome_service_;

  // Implementation of AshChromeService Mojo APIs.
  // Instantiated on receiving the PendingReceiver from lacros-chrome.
  std::unique_ptr<AshChromeServiceImpl> ash_chrome_service_;

  base::WeakPtrFactory<LacrosManager> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LACROS_LACROS_MANAGER_H_
