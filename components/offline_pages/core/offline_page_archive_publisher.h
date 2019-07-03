// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ARCHIVE_PUBLISHER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ARCHIVE_PUBLISHER_H_

#include <cstdint>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_types.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

// The result of publishing an offline page to Downloads.
struct PublishArchiveResult {
  SavePageResult move_result;
  base::FilePath new_file_path;
  int64_t download_id;
};

// Interface of a class responsible for publishing offline page archives to
// downloads.
class OfflinePageArchivePublisher {
 public:
  using PublishArchiveDoneCallback =
      base::OnceCallback<void(const OfflinePageItem& /* offline_page */,
                              PublishArchiveResult /* archive_result */)>;

  virtual ~OfflinePageArchivePublisher() {}

  // Publishes the page on a background thread, then returns to the
  // OfflinePageModelTaskified's done callback.
  virtual void PublishArchive(
      const OfflinePageItem& offline_page,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      PublishArchiveDoneCallback publish_done_callback) const = 0;

  // Removes the archives identified by |download_manager_ids| from downloads.
  virtual void UnpublishArchives(
      const std::vector<int64_t>& download_manager_ids) const = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ARCHIVE_PUBLISHER_H_
