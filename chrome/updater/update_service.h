// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_H_
#define CHROME_UPDATER_UPDATE_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace update_client {
enum class Error;
class Configurator;
class UpdateClient;
}  // namespace update_client

namespace updater {

// The UpdateService is the cross-platform core of the updater.
// All methods must be called on the main thread. All callbacks are called on
// the main thread.
class UpdateService {
 public:
  explicit UpdateService(scoped_refptr<update_client::Configurator> config);

  // Update-checks all registered applications. Calls |callback| once the
  // operation is complete.
  void UpdateAll(base::OnceCallback<void(update_client::Error)> callback);

  ~UpdateService();

  UpdateService(const UpdateService&) = delete;
  UpdateService& operator=(const UpdateService&) = delete;

 private:
  scoped_refptr<update_client::Configurator> config_;
  scoped_refptr<update_client::UpdateClient> update_client_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_H_
