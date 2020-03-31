// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/public/common/switches.h"

namespace switches {

// Makes WebLayer Shell use the given path for its data directory.
const char kWebLayerUserDataDir[] = "weblayer-user-data-dir";

// Makes WebLayer use a fake permission controller delegate which grants all
// permissions.
const char kWebLayerFakePermissions[] = "weblayer-fake-permissions";

}  // namespace switches
