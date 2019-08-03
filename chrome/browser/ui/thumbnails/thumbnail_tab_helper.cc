// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skia_util.h"

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      last_visibility_(web_contents()->GetVisibility()) {}

ThumbnailTabHelper::~ThumbnailTabHelper() {
  DCHECK(!video_capturer_);
}

void ThumbnailTabHelper::ThumbnailImageBeingObservedChanged(
    bool is_being_observed) {
  if (is_being_observed) {
    if (!captured_loaded_thumbnail_since_tab_hidden_)
      StartVideoCapture();
  } else {
    StopVideoCapture();
  }
}

bool ThumbnailTabHelper::ShouldKeepUpdatingThumbnail() const {
  auto* tab_load_tracker = resource_coordinator::TabLoadTracker::Get();
  if (tab_load_tracker &&
      tab_load_tracker->GetLoadingState(web_contents()) !=
          resource_coordinator::TabLoadTracker::LoadingState::LOADED) {
    return true;
  }

  return false;
}

void ThumbnailTabHelper::CaptureThumbnailOnTabSwitch() {
  // Ignore previous requests to capture a thumbnail on tab switch.
  weak_factory_for_thumbnail_on_tab_hidden_.InvalidateWeakPtrs();

  // Get the WebContents' main view. Note that during shutdown there may not be
  // a view to capture.
  content::RenderWidgetHostView* const source_view = GetView();
  if (!source_view)
    return;

  // Note: this is the size in pixels on-screen, not the size in DIPs.
  gfx::Size source_size = source_view->GetViewBounds().size();
  // Clip the pixels that will commonly hold a scrollbar, which looks bad in
  // thumbnails.
  const float scale_factor = source_view->GetDeviceScaleFactor();
  const int scrollbar_size = gfx::scrollbar_size() * scale_factor;
  source_size.Enlarge(-scrollbar_size, -scrollbar_size);

  if (source_size.IsEmpty())
    return;

  const gfx::Size desired_size = TabStyle::GetPreviewImageSize();
  thumbnails::CanvasCopyInfo copy_info =
      thumbnails::GetCanvasCopyInfo(source_size, scale_factor, desired_size);
  source_view->CopyFromSurface(
      copy_info.copy_rect, copy_info.target_size,
      base::BindOnce(&ThumbnailTabHelper::StoreThumbnail,
                     weak_factory_for_thumbnail_on_tab_hidden_.GetWeakPtr()));
}

void ThumbnailTabHelper::StoreThumbnail(const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (bitmap.drawsNothing())
    return;

  thumbnail_->AssignSkBitmap(bitmap);

  // Remember that a thumbnail was captured while the tab was loaded.
  auto* tab_load_tracker = resource_coordinator::TabLoadTracker::Get();
  if (tab_load_tracker &&
      tab_load_tracker->GetLoadingState(web_contents()) ==
          resource_coordinator::TabLoadTracker::LoadingState::LOADED) {
    captured_loaded_thumbnail_since_tab_hidden_ = true;
  }
}

void ThumbnailTabHelper::StartVideoCapture() {
  if (video_capturer_) {
    // Already capturing: We're already forcing rendering. Clear the capturer.
    video_capturer_->Stop();
    video_capturer_ = nullptr;
  } else {
    // *Not* already capturing: Force rendering.
    web_contents()->IncrementCapturerCount(TabStyle::GetPreviewImageSize());
  }

  // Get the WebContents' main view.
  content::RenderWidgetHostView* const source_view = GetView();
  if (!source_view) {
    web_contents()->DecrementCapturerCount();
    return;
  }

  // Start capturing.
  const gfx::Size desired_size = TabStyle::GetPreviewImageSize();
  constexpr int kMaxFrameRate = 5;
  video_capturer_ = source_view->CreateVideoCapturer();
  video_capturer_->SetResolutionConstraints(desired_size, desired_size, true);
  video_capturer_->SetAutoThrottlingEnabled(false);
  video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());
  video_capturer_->SetFormat(media::PIXEL_FORMAT_ARGB,
                             gfx::ColorSpace::CreateREC709());
  video_capturer_->SetMinCapturePeriod(base::TimeDelta::FromSeconds(1) /
                                       kMaxFrameRate);
  video_capturer_->Start(this);
}

void ThumbnailTabHelper::StopVideoCapture() {
  if (video_capturer_) {
    web_contents()->DecrementCapturerCount();
    video_capturer_->Stop();
    video_capturer_ = nullptr;
  }
}

content::RenderWidgetHostView* ThumbnailTabHelper::GetView() {
  return web_contents()->GetRenderViewHost()->GetWidget()->GetView();
}

void ThumbnailTabHelper::OnVisibilityChanged(content::Visibility visibility) {
  if (last_visibility_ == content::Visibility::VISIBLE &&
      visibility != content::Visibility::VISIBLE) {
    captured_loaded_thumbnail_since_tab_hidden_ = false;
    CaptureThumbnailOnTabSwitch();
  }
  last_visibility_ = visibility;
}

void ThumbnailTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() && navigation_handle->HasCommitted())
    captured_loaded_thumbnail_since_tab_hidden_ = false;
}

void ThumbnailTabHelper::OnFrameCaptured(
    base::ReadOnlySharedMemoryRegion data,
    ::media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    ::viz::mojom::FrameSinkVideoConsumerFrameCallbacksPtr callbacks) {
  CHECK(video_capturer_);

  if (!ShouldKeepUpdatingThumbnail())
    StopVideoCapture();

  // Process captured image.
  if (!data.IsValid()) {
    callbacks->Done();
    return;
  }
  base::ReadOnlySharedMemoryMapping mapping = data.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Shared memory mapping failed.";
    return;
  }
  if (mapping.size() <
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size)) {
    DLOG(ERROR) << "Shared memory size was less than expected.";
    return;
  }
  if (!info->color_space) {
    DLOG(ERROR) << "Missing mandatory color space info.";
    return;
  }

  // The SkBitmap's pixels will be marked as immutable, but the installPixels()
  // API requires a non-const pointer. So, cast away the const.
  void* const pixels = const_cast<void*>(mapping.memory());

  // Call installPixels() with a |releaseProc| that: 1) notifies the capturer
  // that this consumer has finished with the frame, and 2) releases the shared
  // memory mapping.
  struct FramePinner {
    // Keeps the shared memory that backs |frame_| mapped.
    base::ReadOnlySharedMemoryMapping mapping;
    // Prevents FrameSinkVideoCapturer from recycling the shared memory that
    // backs |frame_|.
    viz::mojom::FrameSinkVideoConsumerFrameCallbacksPtr releaser;
  };

  const gfx::Size desired_size = TabStyle::GetPreviewImageSize();
  LOG(ERROR) << desired_size.ToString();

  SkBitmap frame;
  frame.installPixels(
      SkImageInfo::MakeN32(content_rect.width(), content_rect.height(),
                           kPremul_SkAlphaType,
                           info->color_space->ToSkColorSpace()),
      pixels,
      media::VideoFrame::RowBytes(media::VideoFrame::kARGBPlane,
                                  info->pixel_format, info->coded_size.width()),
      [](void* addr, void* context) {
        delete static_cast<FramePinner*>(context);
      },
      new FramePinner{std::move(mapping), std::move(callbacks)});
  frame.setImmutable();

  SkBitmap cropped_frame;
  frame.extractSubset(&cropped_frame, gfx::RectToSkIRect(content_rect));

  StoreThumbnail(cropped_frame);
}

void ThumbnailTabHelper::OnStopped() {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ThumbnailTabHelper)
