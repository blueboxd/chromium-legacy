// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTER_FILE_UTILS_H_
#define WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTER_FILE_UTILS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"

namespace base {
class FilePath;
}

namespace weblayer {

class ProfileImpl;

// Returns the set of known persistence ids for the profile at |path|.
base::flat_set<std::string> GetBrowserPersistenceIdsOnBackgroundThread(
    const base::FilePath& path);

// Returns the path to save persistence information. |base_path| is the base
// path of the profile, and |browser_id| the persistence id.
base::FilePath BuildPathForBrowserPersister(const base::FilePath& base_path,
                                            const std::string& browser_id);

// Implementation of RemoveBrowserPersistenceStorage(). Tries to remove all
// the persistence files for the set of browser persistence ids.
void RemoveBrowserPersistenceStorageImpl(
    ProfileImpl* profile,
    base::OnceCallback<void(bool)> done_callback,
    base::flat_set<std::string> ids);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERSISTENCE_BROWSER_PERSISTER_FILE_UTILS_H_
