// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_CONSTANTS_H_
#define CHROME_UPDATER_WIN_CONSTANTS_H_

#include "base/strings/string16.h"

namespace updater {

// The prefix to use for global names in WIN32 API's. The prefix is necessary
// to avoid collision on kernel object names.
extern const base::char16 kGlobalPrefix[];

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_CONSTANTS_H_
