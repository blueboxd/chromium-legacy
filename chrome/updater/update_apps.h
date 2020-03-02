// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_APPS_H_
#define CHROME_UPDATER_UPDATE_APPS_H_

#include <memory>

namespace updater {
class UpdateService;

// A factory method to create an UpdateService class instance.
std::unique_ptr<UpdateService> CreateUpdateService();

// Updates all registered applications.
int UpdateApps();

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_APPS_H_
