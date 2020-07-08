// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/persistence/browser_persister_file_utils.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/base32/base32.h"
#include "components/sessions/core/command_storage_backend.h"
#include "content/public/browser/browser_thread.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/browser_list.h"
#include "weblayer/browser/profile_impl.h"

namespace weblayer {
namespace {

bool RemoveBrowserPersistenceStorageOnBackgroundThread(
    const base::FilePath& path,
    base::flat_set<std::string> ids) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  bool all_succeeded = true;
  for (const std::string& id : ids) {
    DCHECK(!id.empty());
    base::FilePath persistence_path = BuildPathForBrowserPersister(path, id);
    if (!base::DeleteFile(persistence_path))
      all_succeeded = false;
  }
  return all_succeeded;
}

}  // namespace

base::flat_set<std::string> GetBrowserPersistenceIdsOnBackgroundThread(
    const base::FilePath& path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::flat_set<std::string> ids;
  base::FilePath matching_path = base::FilePath().AppendASCII(
      std::string(BrowserImpl::kPersistenceFilePrefix) + std::string("*"));
  base::FileEnumerator iter(path, /* recursive */ false,
                            base::FileEnumerator::FILES, matching_path.value());
  for (base::FilePath name = iter.Next(); !name.empty(); name = iter.Next()) {
    // The name is base32 encoded, which is ascii.
    const std::string base_name = iter.GetInfo().GetName().MaybeAsASCII();
    if (base_name.size() <= base::size(BrowserImpl::kPersistenceFilePrefix))
      continue;

    const std::string encoded_id =
        base_name.substr(base::size(BrowserImpl::kPersistenceFilePrefix) - 1);
    const std::string decoded_id = base32::Base32Decode(encoded_id);
    if (!decoded_id.empty() &&
        sessions::CommandStorageBackend::IsValidFile(name)) {
      ids.insert(decoded_id);
    }
  }
  return ids;
}

base::FilePath BuildPathForBrowserPersister(const base::FilePath& base_path,
                                            const std::string& browser_id) {
  DCHECK(!browser_id.empty());
  const std::string encoded_name = base32::Base32Encode(browser_id);
  return base_path.AppendASCII(BrowserImpl::kPersistenceFilePrefix +
                               encoded_name);
}

void RemoveBrowserPersistenceStorageImpl(
    ProfileImpl* profile,
    base::OnceCallback<void(bool)> done_callback,
    base::flat_set<std::string> ids) {
  // Remove any ids that are actively in use.
  for (BrowserImpl* browser : BrowserList::GetInstance()->browsers()) {
    if (browser->profile() == profile)
      ids.erase(browser->GetPersistenceId());
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RemoveBrowserPersistenceStorageOnBackgroundThread,
                     profile->GetBrowserPersisterDataBaseDir(), std::move(ids)),
      std::move(done_callback));
}

}  // namespace weblayer
