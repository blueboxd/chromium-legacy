// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_ARCHIVE_PUBLISHER_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_ARCHIVE_PUBLISHER_IMPL_H_

#include <cstdint>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_types.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

class ArchiveManager;

class OfflinePageArchivePublisherImpl : public OfflinePageArchivePublisher {
 public:
  class Delegate {
   public:
    Delegate() = default;

    // Returns true if a system download manager is available on this platform.
    virtual bool IsDownloadManagerInstalled();

    // Returns the download manager ID of the download, which we will place in
    // the offline pages database as part of the offline page item.
    // TODO(petewil): it might make sense to move all these params into a
    // struct.
    virtual int64_t AddCompletedDownload(const std::string& title,
                                         const std::string& description,
                                         const std::string& path,
                                         int64_t length,
                                         const std::string& uri,
                                         const std::string& referer);

    // Returns the number of pages removed.
    virtual int Remove(
        const std::vector<int64_t>& android_download_manager_ids);

   private:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
  };

  explicit OfflinePageArchivePublisherImpl(ArchiveManager* archive_manager);
  ~OfflinePageArchivePublisherImpl() override;

  void SetDelegateForTesting(Delegate* delegate);

  // OfflinePageArchivePublisher implementation.
  void PublishArchive(
      const OfflinePageItem& offline_page,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      PublishArchiveDoneCallback publish_done_callback) const override;

  void UnpublishArchives(
      const std::vector<int64_t>& download_manager_ids) const override;

 private:
  ArchiveManager* archive_manager_;
  Delegate* delegate_;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_ARCHIVE_PUBLISHER_IMPL_H_
