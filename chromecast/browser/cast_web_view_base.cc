// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view_base.h"

#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/lru_renderer_cache.h"
#include "chromecast/browser/renderer_prelauncher.h"
#include "content/public/browser/site_instance.h"

namespace chromecast {

CastWebViewBase::CastWebViewBase(const CreateParams& create_params,
                                 CastWebService* web_service)
    : delegate_(create_params.delegate),
      web_service_(web_service),
      shutdown_delay_(create_params.shutdown_delay),
      renderer_pool_(create_params.renderer_pool),
      prelaunch_url_(create_params.prelaunch_url),
      renderer_prelauncher_(nullptr),
      site_instance_(nullptr) {
  DCHECK(web_service_);
  if (prelaunch_url_.is_valid()) {
    if (renderer_pool_ == RendererPool::OVERLAY) {
      renderer_prelauncher_ =
          web_service_->overlay_renderer_cache()->TakeRendererPrelauncher(
              prelaunch_url_);
    } else {
      renderer_prelauncher_ = std::make_unique<RendererPrelauncher>(
          web_service_->browser_context(), prelaunch_url_);
    }
  }
  if (renderer_prelauncher_) {
    renderer_prelauncher_->Prelaunch();
    site_instance_ = renderer_prelauncher_->site_instance();
  }
}

CastWebViewBase::~CastWebViewBase() {
  if (renderer_prelauncher_ && prelaunch_url_.is_valid() &&
      renderer_pool_ == RendererPool::OVERLAY) {
    web_service_->overlay_renderer_cache()->ReleaseRendererPrelauncher(
        prelaunch_url_);
  }
  for (Observer& observer : observer_list_) {
    observer.OnPageDestroyed(this);
  }
}

void CastWebViewBase::ForceClose() {
  shutdown_delay_ = base::TimeDelta();
  cast_web_contents()->ClosePage();
}

void CastWebViewBase::AddObserver(CastWebViewBase::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CastWebViewBase::RemoveObserver(CastWebViewBase::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

base::TimeDelta CastWebViewBase::shutdown_delay() const {
  return shutdown_delay_;
}

}  // namespace chromecast
