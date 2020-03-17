// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PREFS_H_
#define CHROME_UPDATER_PREFS_H_

#include <memory>

class PrefService;

namespace updater {

std::unique_ptr<PrefService> CreatePrefService();

// Commits prefs changes to storage. This function should only be called
// when the changes must be written immediately, for instance, during program
// shutdown. The function must be called in the scope of a task executor.
void PrefsCommitPendingWrites(PrefService* pref_service);

}  // namespace updater

#endif  // CHROME_UPDATER_PREFS_H_
