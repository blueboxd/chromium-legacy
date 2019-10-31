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
#include "components/history/core/common/thumbnail_score.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      last_visibility_(web_contents()->GetVisibility()) {}

ThumbnailTabHelper::~ThumbnailTabHelper() {
  DCHECK(!video_capturer_);
}

void ThumbnailTabHelper::ThumbnailImageBeingObservedChanged(
    bool is_being_observed) {
  if (is_being_observed_ != is_being_observed) {
    is_being_observed_ = is_being_observed;
    if (is_being_observed && !captured_loaded_thumbnail_since_tab_hidden_)
      StartVideoCapture();
  }
}

bool ThumbnailTabHelper::ShouldKeepUpdatingThumbnail() const {
  if (!is_being_observed_)
    return false;

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
  if (source_size.IsEmpty())
    return;

  const float scale_factor = source_view->GetDeviceScaleFactor();
  ThumbnailCaptureInfo copy_info = GetInitialCaptureInfo(
      source_size, scale_factor, /* include_scrollbars_in_capture */ false);

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
  if (video_capturer_)
    return;

  // Get the WebContents' main view.
  content::RenderWidgetHostView* const source_view = GetView();
  if (!source_view)
    return;

  // Get the source size and scale.
  const float scale_factor = source_view->GetDeviceScaleFactor();
  const gfx::Size source_size = source_view->GetViewBounds().size();
  if (source_size.IsEmpty())
    return;

  // Figure out how large we want the capture target to be.
  last_frame_capture_info_ =
      GetInitialCaptureInfo(source_size, scale_factor,
                            /* include_scrollbars_in_capture */ true);

  const gfx::Size& target_size = last_frame_capture_info_.target_size;
  web_contents()->IncrementCapturerCount(target_size);
  constexpr int kMaxFrameRate = 5;
  video_capturer_ = source_view->CreateVideoCapturer();
  video_capturer_->SetResolutionConstraints(target_size, target_size, false);
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
    mojo::PendingRemote<::viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  CHECK(video_capturer_);

  if (!ShouldKeepUpdatingThumbnail())
    StopVideoCapture();

  mojo::Remote<::viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks_remote(std::move(callbacks));

  // Process captured image.
  if (!data.IsValid()) {
    callbacks_remote->Done();
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
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        releaser;
  };

  // Subtract back out the scroll bars if we decided there was enough canvas to
  // account for them and still have a decent preview image.
  const float scale_ratio = float{content_rect.width()} /
                            float{last_frame_capture_info_.copy_rect.width()};

  const gfx::Insets original_scroll_insets =
      last_frame_capture_info_.scrollbar_insets;
  const gfx::Insets scroll_insets(
      0, 0, std::round(original_scroll_insets.width() * scale_ratio),
      std::round(original_scroll_insets.height() * scale_ratio));
  gfx::Rect effective_content_rect = content_rect;
  effective_content_rect.Inset(scroll_insets);

  const gfx::Size bitmap_size(content_rect.right(), content_rect.bottom());
  SkBitmap frame;
  frame.installPixels(
      SkImageInfo::MakeN32(bitmap_size.width(), bitmap_size.height(),
                           kPremul_SkAlphaType,
                           info->color_space->ToSkColorSpace()),
      pixels,
      media::VideoFrame::RowBytes(media::VideoFrame::kARGBPlane,
                                  info->pixel_format, info->coded_size.width()),
      [](void* addr, void* context) {
        delete static_cast<FramePinner*>(context);
      },
      new FramePinner{std::move(mapping), callbacks_remote.Unbind()});
  frame.setImmutable();

  SkBitmap cropped_frame;
  if (frame.extractSubset(&cropped_frame,
                          gfx::RectToSkIRect(effective_content_rect))) {
    StoreThumbnail(cropped_frame);
  }
}

void ThumbnailTabHelper::OnStopped() {}

// static
ThumbnailTabHelper::ThumbnailCaptureInfo
ThumbnailTabHelper::GetInitialCaptureInfo(const gfx::Size& source_size,
                                          float scale_factor,
                                          bool include_scrollbars_in_capture) {
  ThumbnailCaptureInfo capture_info;
  capture_info.source_size = source_size;

  // Minimum scale factor to capture thumbnail images at. At 1.0x we want to
  // slightly over-sample the image so that it looks good for multiple uses and
  // cropped to different dimensions.
  constexpr float kMinThumbnailScaleFactor = 1.5f;
  scale_factor = std::max(scale_factor, kMinThumbnailScaleFactor);

  // Minimum thumbnail dimension (in DIP) for tablet tabstrip previews.
  constexpr int kMinThumbnailDimensionForTablet = 175;
  const gfx::Size preview_size = TabStyle::GetPreviewImageSize();
  const int smallest_dimension =
      scale_factor *
      std::max(kMinThumbnailDimensionForTablet,
               std::min(preview_size.width(), preview_size.height()));

  // Clip the pixels that will commonly hold a scrollbar, which looks bad in
  // thumbnails - but only if that wouldn't make the thumbnail too small. We
  // can't just use gfx::scrollbar_size() because that reports default system
  // scrollbar width which is different from the width used in web rendering.
  const int scrollbar_size_dip =
      ui::NativeTheme::GetInstanceForWeb()
          ->GetPartSize(ui::NativeTheme::Part::kScrollbarVerticalTrack,
                        ui::NativeTheme::State::kNormal,
                        ui::NativeTheme::ExtraParams())
          .width();
  // Round up to make sure any scrollbar pixls are eliminated. It's better to
  // lose a single pixel of content than having a single pixel of scrollbar.
  const int scrollbar_size = std::ceil(scale_factor * scrollbar_size_dip);
  if (source_size.width() - scrollbar_size > smallest_dimension)
    capture_info.scrollbar_insets.set_right(scrollbar_size);
  if (source_size.height() - scrollbar_size > smallest_dimension)
    capture_info.scrollbar_insets.set_bottom(scrollbar_size);

  // Calculate the region to copy from.
  capture_info.copy_rect = gfx::Rect(source_size);
  if (!include_scrollbars_in_capture)
    capture_info.copy_rect.Inset(capture_info.scrollbar_insets);

  // Compute minimum sizes for multiple uses of the thumbnail - currently,
  // tablet tabstrip previews and tab hover card preview images.
  gfx::Size min_target_size = TabStyle::GetPreviewImageSize();
  min_target_size.SetToMax(
      {kMinThumbnailDimensionForTablet, kMinThumbnailDimensionForTablet});
  min_target_size = gfx::ScaleToFlooredSize(min_target_size, scale_factor);

  // Calculate the target size to be the smallest size which meets the minimum
  // requirements but has the same aspect ratio as the source (with or without
  // scrollbars).
  const float width_ratio =
      float{capture_info.copy_rect.width()} / min_target_size.width();
  const float height_ratio =
      float{capture_info.copy_rect.height()} / min_target_size.height();
  const float scale_ratio = std::min(width_ratio, height_ratio);
  capture_info.target_size =
      scale_ratio <= 1.0f
          ? capture_info.copy_rect.size()
          : gfx::ScaleToCeiledSize(capture_info.copy_rect.size(),
                                   1.0f / scale_ratio);

  return capture_info;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ThumbnailTabHelper)
