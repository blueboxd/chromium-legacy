// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PREFS_H_
#define CHROME_UPDATER_PREFS_H_

#include <memory>

class PrefService;

namespace updater {

class ScopedPrefsLock;

extern const char kPrefQualified[];
extern const char kPrefSwapping[];
extern const char kPrefActiveVersion[];

class UpdaterPrefs {
 public:
  UpdaterPrefs(std::unique_ptr<ScopedPrefsLock> lock,
               std::unique_ptr<PrefService> prefs);
  UpdaterPrefs(const UpdaterPrefs&) = delete;
  UpdaterPrefs& operator=(const UpdaterPrefs&) = delete;
  ~UpdaterPrefs();

  PrefService* GetPrefService() const;

 private:
  std::unique_ptr<ScopedPrefsLock> lock_;
  std::unique_ptr<PrefService> prefs_;
};

// Open the global prefs. These prefs are protected by a mutex, and shared by
// all updaters on the system. Returns nullptr if the mutex cannot be acquired.
std::unique_ptr<UpdaterPrefs> CreateGlobalPrefs();

// Open the version-specific prefs. These prefs are not protected by any mutex
// and not shared with other versions of the updater.
std::unique_ptr<UpdaterPrefs> CreateLocalPrefs();

// Commits prefs changes to storage. This function should only be called
// when the changes must be written immediately, for instance, during program
// shutdown. The function must be called in the scope of a task executor.
void PrefsCommitPendingWrites(PrefService* pref_service);

}  // namespace updater

#endif  // CHROME_UPDATER_PREFS_H_
