// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_started_animation.h"
#include "chrome/browser/download/offline_item_model.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/animation/animation.h"

namespace {

// Delay before we show a transient download.
const int64_t kDownloadShowDelayInSeconds = 2;

}  // namespace

DownloadShelf::DownloadShelf(Browser* browser, Profile* profile)
    : browser_(browser),
      profile_(profile),
      should_show_on_unhide_(false),
      is_hidden_(false) {}

DownloadShelf::~DownloadShelf() {}

void DownloadShelf::AddDownload(DownloadUIModel::DownloadUIModelPtr model) {
  DCHECK(model);
  if (model->ShouldRemoveFromShelfWhenComplete()) {
    // If we are going to remove the download from the shelf upon completion,
    // wait a few seconds to see if it completes quickly. If it's a small
    // download, then the user won't have time to interact with it.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DownloadShelf::ShowDownloadById,
                       weak_ptr_factory_.GetWeakPtr(), model->GetContentId()),
        GetTransientDownloadShowDelay());
  } else {
    ShowDownload(std::move(model));
  }
}

void DownloadShelf::Open() {
  if (is_hidden_)
    should_show_on_unhide_ = true;
  else
    DoOpen();
}

void DownloadShelf::Close() {
  if (is_hidden_)
    should_show_on_unhide_ = false;
  else
    DoClose();
}

void DownloadShelf::Hide() {
  if (is_hidden_)
    return;
  is_hidden_ = true;
  if (IsShowing()) {
    should_show_on_unhide_ = true;
    DoHide();
  }
}

void DownloadShelf::Unhide() {
  if (!is_hidden_)
    return;
  is_hidden_ = false;
  if (should_show_on_unhide_) {
    should_show_on_unhide_ = false;
    DoUnhide();
  }
}

base::TimeDelta DownloadShelf::GetTransientDownloadShowDelay() const {
  return base::TimeDelta::FromSeconds(kDownloadShowDelayInSeconds);
}

void DownloadShelf::ShowDownload(DownloadUIModel::DownloadUIModelPtr download) {
  if (download->GetState() == download::DownloadItem::COMPLETE &&
      download->ShouldRemoveFromShelfWhenComplete())
    return;

  if (!DownloadCoreServiceFactory::GetForBrowserContext(download->profile())
           ->IsShelfEnabled())
    return;

  bool should_show_download_started_animation =
      download->ShouldShowDownloadStartedAnimation();

  if (is_hidden_)
    Unhide();
  Open();
  DoShowDownload(std::move(download));

  // browser_ can be null for tests.
  if (!browser_)
    return;

  // Show the download started animation if:
  // - Download started animation is enabled for this download. It is disabled
  //   for "Save As" downloads and extension installs, for example.
  // - The browser has an active visible WebContents. (browser isn't minimized,
  //   or running under a test etc.)
  // - Rich animations are enabled.
  content::WebContents* shelf_tab =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (should_show_download_started_animation && shelf_tab &&
      platform_util::IsVisible(shelf_tab->GetNativeView()) &&
      gfx::Animation::ShouldRenderRichAnimation()) {
    DownloadStartedAnimation::Show(shelf_tab);
  }
}

void DownloadShelf::ShowDownloadById(
    const offline_items_collection::ContentId& id) {
  if (OfflineItemUtils::IsDownload(id)) {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(profile_);
    if (!download_manager)
      return;

    download::DownloadItem* download =
        download_manager->GetDownloadByGuid(id.id);
    if (!download)
      return;

    ShowDownload(DownloadItemModel::Wrap(download));
  } else {
    offline_items_collection::OfflineContentAggregator* aggregator =
        OfflineContentAggregatorFactory::GetForKey(profile_->GetProfileKey());
    if (!aggregator)
      return;

    aggregator->GetItemById(
        id, base::BindOnce(&DownloadShelf::OnGetDownloadDoneForOfflineItem,
                           weak_ptr_factory_.GetWeakPtr()));
  }
}

void DownloadShelf::OnGetDownloadDoneForOfflineItem(
    const base::Optional<offline_items_collection::OfflineItem>& item) {
  if (!item.has_value())
    return;

  OfflineItemModelManager* manager =
      OfflineItemModelManagerFactory::GetForBrowserContext(profile_);
  ShowDownload(OfflineItemModel::Wrap(manager, item.value()));
}
