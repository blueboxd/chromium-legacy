// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_H_
#define CHROME_UPDATER_UPDATE_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace update_client {
enum class Error;
}  // namespace update_client

namespace updater {

struct RegistrationRequest;
struct RegistrationResponse;

// The UpdateService is the cross-platform core of the updater.
// All functions and callbacks must be called on the same sequence.
class UpdateService : public base::RefCountedThreadSafe<UpdateService> {
 public:
  using Result = update_client::Error;

  // Possible states for updating an app.
  enum class UpdateState {
    // This value represents the absence of a state. No update request has
    // yet been issued.
    kUnknown = 0,

    // This update has not been started, but has been requested.
    kNotStarted = 1,

    // The engine began issuing an update check request.
    kCheckingForUpdates = 2,

    // The engine began downloading an update.
    kDownloading = 3,

    // The engine began running installation scripts.
    kInstalling = 4,

    // The engine found and installed an update for this product. The update
    // is complete and the state will not change.
    kUpdated = 100,

    // The engine checked for updates. This product is already up to date.
    // No update has been installed for this product. The update is complete
    // and the state will not change.
    kNoUpdate = 101,

    // The engine encountered an error updating this product. The update has
    // halted and the state will not change.
    kUpdateError = 102,
  };

  // Urgency of the update service invocation.
  enum class Priority {
    // The caller has not set a valid priority value.
    kUnknown = 0,

    // The user is not waiting for this update.
    kBackground = 1,

    // The user actively requested this update.
    kForeground = 2,
  };

  using StateChangeCallback = base::RepeatingCallback<void(UpdateState)>;
  using Callback = base::OnceCallback<void(Result)>;

  // Registers given request to the updater.
  virtual void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(const RegistrationResponse&)> callback) = 0;

  // Initiates an update check for all registered applications. Receives state
  // change notifications through the repeating |state_update| callback.
  // Calls |callback| once  the operation is complete.
  virtual void UpdateAll(StateChangeCallback state_update,
                         Callback callback) = 0;

  // Updates specified product. This update may be on-demand.
  //
  // Args:
  //   |app_id|: ID of app to update.
  //   |priority|: Priority for processing this update.
  //   |state_update|: The callback will be invoked every time the update
  //     changes state when the engine starts. It will be called on the
  //     sequence used by the update service, so this callback must not block.
  //     It will not be called again after the update has reached a terminal
  //     state. It will not be called after the completion |callback| is posted.
  //   |callback|: Posted after the update stops, successfully or otherwise.
  //
  // |state_update| arg:
  //    UpdateState: the new state of this update request.
  //
  // |callback| arg:
  //    Result: the final result from the update engine.
  virtual void Update(const std::string& app_id,
                      Priority priority,
                      StateChangeCallback state_update,
                      Callback callback) = 0;

  // Provides a way to commit data or clean up resources before the task
  // scheduler is shutting down.
  virtual void Uninitialize() = 0;

 protected:
  friend class base::RefCountedThreadSafe<UpdateService>;

  virtual ~UpdateService() = default;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_H_
