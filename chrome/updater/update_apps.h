// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_APPS_H_
#define CHROME_UPDATER_UPDATE_APPS_H_

#include "base/memory/scoped_refptr.h"

namespace update_client {
class Configurator;
}  // namespace update_client

namespace updater {

class UpdateService;

// A factory method to create an UpdateService class instance.
scoped_refptr<UpdateService> CreateUpdateService(
    scoped_refptr<update_client::Configurator> config);

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_APPS_H_
