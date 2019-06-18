// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

using DownloadItem = download::DownloadItem;
using SimpleDownloadManagerCoordinator =
    download::SimpleDownloadManagerCoordinator;
using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;
using OfflineContentProvider = offline_items_collection::OfflineContentProvider;
using OfflineContentAggregator =
    offline_items_collection::OfflineContentAggregator;
using UpdateDelta = offline_items_collection::UpdateDelta;
using LaunchLocation = offline_items_collection::LaunchLocation;

class SkBitmap;

// This class handles the task of observing the downloads associated with a
// SimpleDownloadManagerCoordinator and notifies UI about updates about various
// downloads.
class DownloadOfflineContentProvider
    : public OfflineContentProvider,
      public DownloadItem::Observer,
      public SimpleDownloadManagerCoordinator::Observer {
 public:
  explicit DownloadOfflineContentProvider(OfflineContentAggregator* aggregator,
                                          const std::string& name_space);
  ~DownloadOfflineContentProvider() override;

  // Should be called when a DownloadManager is available.
  void SetSimpleDownloadManagerCoordinator(
      SimpleDownloadManagerCoordinator* manager);

  // OfflineContentProvider implmentation.
  void OpenItem(LaunchLocation location, const ContentId& id) override;
  void RemoveItem(const ContentId& id) override;
  void CancelDownload(const ContentId& id) override;
  void PauseDownload(const ContentId& id) override;
  void ResumeDownload(const ContentId& id, bool has_user_gesture) override;
  void GetItemById(
      const ContentId& id,
      OfflineContentProvider::SingleItemCallback callback) override;
  void GetAllItems(
      OfflineContentProvider::MultipleItemCallback callback) override;
  void GetVisualsForItem(
      const ContentId& id,
      GetVisualsOptions options,
      OfflineContentProvider::VisualsCallback callback) override;
  void GetShareInfoForItem(const ContentId& id,
                           ShareCallback callback) override;
  void RenameItem(const ContentId& id,
                  const std::string& name,
                  RenameCallback callback) override;
  void AddObserver(OfflineContentProvider::Observer* observer) override;
  void RemoveObserver(OfflineContentProvider::Observer* observer) override;

  // Entry point for associating this class with a download item. Must be called
  // for all new and in-progress downloads, after which this class will start
  // observing the given download.
  void OnDownloadStarted(DownloadItem* download_item);

 private:
  // DownloadItem::Observer overrides
  void OnDownloadUpdated(DownloadItem* item) override;
  void OnDownloadRemoved(DownloadItem* item) override;
  void OnDownloadDestroyed(DownloadItem* download) override;

  // SimpleDownloadManagerCoordinator::Observer overrides
  void OnManagerGoingDown() override;

  void GetAllDownloads(std::vector<DownloadItem*>* all_items);
  DownloadItem* GetDownload(const std::string& download_guid);
  void OnThumbnailRetrieved(const ContentId& id,
                            VisualsCallback callback,
                            const SkBitmap& bitmap);
  void AddCompletedDownload(DownloadItem* item);
  void AddCompletedDownloadDone(DownloadItem* item,
                                int64_t system_download_id,
                                bool can_resolve);
  void OnRenameDownloadCallbackDone(RenameCallback callback,
                                    DownloadItem* item,
                                    DownloadItem::DownloadRenameResult result);
  void UpdateObservers(const OfflineItem& item,
                       const base::Optional<UpdateDelta>& update_delta);

  base::ObserverList<OfflineContentProvider::Observer>::Unchecked observers_;
  OfflineContentAggregator* aggregator_;
  std::string name_space_;
  SimpleDownloadManagerCoordinator* manager_;

  // Tracks the completed downloads in the current session.
  std::set<std::string> completed_downloads_;

  base::WeakPtrFactory<DownloadOfflineContentProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadOfflineContentProvider);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_OFFLINE_CONTENT_PROVIDER_H_
