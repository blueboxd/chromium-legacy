// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_web_view_default.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/browser/lru_renderer_cache.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace chromecast {

namespace {

uint32_t remove_data_mask =
    content::StoragePartition::REMOVE_DATA_MASK_APPCACHE |
    content::StoragePartition::REMOVE_DATA_MASK_COOKIES |
    content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
    content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
    content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE |
    content::StoragePartition::REMOVE_DATA_MASK_WEBSQL;

}  // namespace

CastWebService::CastWebService(content::BrowserContext* browser_context,
                               CastWebViewFactory* web_view_factory,
                               CastWindowManager* window_manager)
    : browser_context_(browser_context),
      web_view_factory_(web_view_factory),
      window_manager_(window_manager),
      overlay_renderer_cache_(
          std::make_unique<LRURendererCache>(browser_context_, 1)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(browser_context_);
  DCHECK(web_view_factory_);
  DCHECK(task_runner_);
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

CastWebService::~CastWebService() = default;

CastWebView::Scoped CastWebService::CreateWebView(
    const CastWebView::CreateParams& params,
    const GURL& initial_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto web_view = web_view_factory_->CreateWebView(params, this, initial_url);
  CastWebView::Scoped scoped(web_view.get(), [this](CastWebView* web_view) {
    OwnerDestroyed(web_view);
  });
  web_views_.insert(std::move(web_view));
  return scoped;
}

void CastWebService::FlushDomLocalStorage() {
  content::BrowserContext::ForEachStoragePartition(
      browser_context_,
      base::BindRepeating([](content::StoragePartition* storage_partition) {
        DVLOG(1) << "Starting DOM localStorage flush.";
        storage_partition->Flush();
      }));
}

void CastWebService::ClearLocalStorage(base::OnceClosure callback) {
  content::BrowserContext::ForEachStoragePartition(
      browser_context_,
      base::BindRepeating(
          [](base::OnceClosure cb, content::StoragePartition* partition) {
            auto cookie_delete_filter =
                network::mojom::CookieDeletionFilter::New();
            cookie_delete_filter->session_control =
                network::mojom::CookieDeletionSessionControl::IGNORE_CONTROL;
            partition->ClearData(
                remove_data_mask,
                content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                content::StoragePartition::OriginMatcherFunction(),
                std::move(cookie_delete_filter), true /*perform_cleanup*/,
                base::Time::Min(), base::Time::Max(), std::move(cb));
          },
          base::Passed(std::move(callback))));
}

void CastWebService::StopGpuProcess(base::OnceClosure callback) const {
  content::StopGpuProcess(std::move(callback));
}

void CastWebService::OwnerDestroyed(CastWebView* web_view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::WebContents* web_contents = web_view->web_contents();
  GURL url;
  if (web_contents) {
    url = web_contents->GetVisibleURL();
    // Suspend the MediaSession to free up media resources for the next content
    // window.
    content::MediaSession::Get(web_contents)
        ->Suspend(content::MediaSession::SuspendType::kSystem);
  }
  auto delay = web_view->shutdown_delay();
  if (delay <= base::TimeDelta()) {
    LOG(INFO) << "Immediately deleting CastWebView for " << url;
    DeleteWebView(web_view);
    return;
  }
  LOG(INFO) << "Deleting CastWebView for " << url << " in "
            << delay.InMilliseconds() << " milliseconds.";
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastWebService::DeleteWebView, weak_ptr_, web_view),
      delay);
}

void CastWebService::DeleteWebView(CastWebView* web_view) {
  LOG(INFO) << "Deleting CastWebView.";
  base::EraseIf(web_views_,
                [web_view](const std::unique_ptr<CastWebView>& ptr) {
                  return ptr.get() == web_view;
                });
}

}  // namespace chromecast
