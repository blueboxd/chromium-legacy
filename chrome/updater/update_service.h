// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_H_
#define CHROME_UPDATER_UPDATE_SERVICE_H_

#include "base/callback_forward.h"

namespace update_client {
enum class Error;
}  // namespace update_client

namespace updater {
struct RegistrationRequest;
struct RegistrationResponse;

// The UpdateService is the cross-platform core of the updater.
// All functions and callbacks must be called on the same sequence.
class UpdateService {
 public:
  UpdateService(const UpdateService&) = delete;
  UpdateService& operator=(const UpdateService&) = delete;

  virtual ~UpdateService() = default;

  // Registers given request to the updater.
  virtual void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(const RegistrationResponse&)> callback) = 0;

  // Update-checks all registered applications. Calls |callback| once the
  // operation is complete.
  virtual void UpdateAll(
      base::OnceCallback<void(update_client::Error)> callback) = 0;

 protected:
  UpdateService() = default;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_H_
