// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
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

namespace {

// Minimum scale factor to capture thumbnail images at. At 1.0x we want to
// slightly over-sample the image so that it looks good for multiple uses and
// cropped to different dimensions.
constexpr float kMinThumbnailScaleFactor = 1.5f;

gfx::Size GetMinimumThumbnailSize() {
  // Minimum thumbnail dimension (in DIP) for tablet tabstrip previews.
  constexpr int kMinThumbnailDimensionForTablet = 175;

  // Compute minimum sizes for multiple uses of the thumbnail - currently,
  // tablet tabstrip previews and tab hover card preview images.
  gfx::Size min_target_size = TabStyle::GetPreviewImageSize();
  min_target_size.SetToMax(
      {kMinThumbnailDimensionForTablet, kMinThumbnailDimensionForTablet});

  return min_target_size;
}

// Manages increment/decrement of video capture state on a WebContents.
// Acquires (if possible) on construction, releases (if acquired) on
// destruction.
class ScopedThumbnailCapture {
 public:
  explicit ScopedThumbnailCapture(
      content::WebContentsObserver* web_contents_observer)
      : web_contents_observer_(web_contents_observer) {
    auto* const contents = web_contents_observer->web_contents();
    if (contents) {
      contents->IncrementCapturerCount(
          gfx::ScaleToFlooredSize(GetMinimumThumbnailSize(),
                                  kMinThumbnailScaleFactor),
          /* stay_hidden */ true);
      captured_ = true;
    }
  }

  ~ScopedThumbnailCapture() {
    auto* const contents = web_contents_observer_->web_contents();
    if (captured_ && contents)
      contents->DecrementCapturerCount(/* stay_hidden */ true);
  }

 private:
  // We track a web contents observer because it's an easy way to see if the
  // web contents has disappeared without having to add another observer.
  content::WebContentsObserver* const web_contents_observer_;
  bool captured_ = false;
};

}  // anonymous namespace

// ThumbnailTabHelper::CaptureType ---------------------------------------

enum class ThumbnailTabHelper::CaptureType {
  // The image was copied directly from a visible RenderWidgetHostView.
  kCopyFromView = 0,
  // The image is a frame from a background tab video capturer.
  kVideoFrame = 1,

  kMaxValue = kVideoFrame,
};

// ThumbnailTabHelper::TabStateTracker ---------------------------

// Stores information about the state of the current WebContents and renderer.
class ThumbnailTabHelper::TabStateTracker : public content::WebContentsObserver,
                                            public ThumbnailImage::Delegate {
 public:
  TabStateTracker(ThumbnailTabHelper* thumbnail_tab_helper,
                  content::WebContents* contents)
      : content::WebContentsObserver(contents),
        thumbnail_tab_helper_(thumbnail_tab_helper) {
    visible_ =
        (web_contents()->GetVisibility() == content::Visibility::VISIBLE);
  }
  ~TabStateTracker() override = default;

  // Returns the host view associated with the current web contents, or null if
  // none.
  content::RenderWidgetHostView* GetView() {
    auto* const contents = web_contents();
    return contents ? contents->GetRenderViewHost()->GetWidget()->GetView()
                    : nullptr;
  }

  // Returns true if we are capturing thumbnails from a tab and should continue
  // to do so, false if we should stop.
  bool ShouldContinueVideoCapture() const { return scoped_capture_ != nullptr; }

  // Records that a frame has been captured. Allows us to hold off on ending
  // cooldown until a frame of a webpage has been captured.
  void OnFrameCaptured(CaptureType capture_type) {
    if (tab_state_ == TabState::kCaptureCooldown &&
        capture_type == CaptureType::kVideoFrame) {
      captured_cooldown_frame_ = true;
    }
  }

 private:
  // Represents the lifecycle of capturing a page navigation as a thumbnail.
  // Order of existing elements is invariant and should not be changed.
  enum class TabState : int {
    // We start here. Nothing can happen in this state.
    kNoPage = 0,
    // The WebContents is navigating to a new page.
    kNavigating,
    // Navigation is complete. We can at any point request a renderer by
    // incrementing the capture count.
    kNavigationComplete,
    // Navigation is complete and we'd like to start capturing video.
    kCaptureRequested,
    // We are actively capturing video. This lasts until either the page becomes
    // visible or finishes loading.
    kCapturingVideo,
    // The page has finished loading and we are still capturing video for a bit
    // to make sure we catch the final layout.
    kCaptureCooldown,
    // This page is loaded. The only time we will capture a loaded page is when
    // it transitions from visible to not visible.
    kPageLoaded,

    kMaxValue = kPageLoaded
  };

  void set_tab_state(TabState state) { tab_state_ = state; }

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override {
    const bool new_visible = (visibility == content::Visibility::VISIBLE);
    if (new_visible == visible_)
      return;

    visible_ = new_visible;
    if (!visible_ && tab_state_ == TabState::kPageLoaded) {
      thumbnail_tab_helper_->CaptureThumbnailOnTabHidden();
    } else if (tab_state_ >= TabState::kNavigationComplete &&
               tab_state_ <= TabState::kCaptureCooldown) {
      UpdateCaptureState();
    }
  }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInMainFrame())
      return;
    set_tab_state(TabState::kNavigating);
    StopCapture();
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInMainFrame())
      return;
    if (tab_state_ < TabState::kNavigationComplete)
      UpdateCaptureState();
  }

  void RenderViewReady() override {
    if (tab_state_ < TabState::kCapturingVideo)
      UpdateCaptureState();
  }

  void DocumentOnLoadCompletedInMainFrame() override {
    if (tab_state_ == TabState::kCapturingVideo)
      UpdateCaptureState();
  }

  void WebContentsDestroyed() override {
    StopCapture();
    tab_state_ = TabState::kNoPage;
  }

  // ThumbnailImage::Delegate:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {
    if (is_being_observed == is_being_observed_)
      return;

    is_being_observed_ = is_being_observed;
    if (tab_state_ >= TabState::kNavigationComplete &&
        tab_state_ <= TabState::kCapturingVideo) {
      UpdateCaptureState();
    }
  }

  // Transitions the state tracker to the correct state any time after
  // navigation is complete, given the tab's observed state, visibility, loading
  // status, etc.
  void UpdateCaptureState() {
    if (web_contents()->IsBeingDestroyed())
      return;

    const bool is_loaded =
        web_contents()->IsDocumentOnLoadCompletedInMainFrame();

    // For now, don't force-load background pages. This is not ideal. We would
    // like to grab frames from background pages to make hover cards and the
    // "Mohnstrudel" touch/tablet tabstrip more responsive by pre-loading
    // thumbnails from those pages. However, this currently results in a number
    // of test failures and a possible violation of an assumption made by the
    // renderer.
    // TODO(crbug.com/1073141): Figure out how to force-render backgorund tabs.
    // This bug has detailed descriptions of steps we might take to make capture
    // more flexible in this area.
    if (!is_being_observed_ && tab_state_ <= TabState::kNavigationComplete) {
      set_tab_state(TabState::kNavigationComplete);
      return;
    }

    // Tabs that are visible and unobserved are not captured.
    if (!is_being_observed_ && visible_) {
      set_tab_state(TabState::kNavigationComplete);
      StopCapture();
      return;
    }

    // If there is no render view associated with a tab, we can only request
    // capture.
    if (!GetView()) {
      set_tab_state(TabState::kCaptureRequested);
      RequestCapture();
      return;
    }

    // Just in case - we don't want to lose the renderer if someone decides to
    // unload the page.
    RequestCapture();

    // If we are not done loading this page, go into the standard capture state.
    if (!is_loaded) {
      set_tab_state(TabState::kCapturingVideo);
      thumbnail_tab_helper_->StartVideoCapture();
      return;
    }

    // We are done loading the page and may need to transition into the cooldown
    // state. If we're already there, we're done.
    if (tab_state_ == TabState::kCaptureCooldown)
      return;

    captured_cooldown_frame_ = false;
    cooldown_retry_count_ = 0U;
    set_tab_state(TabState::kCaptureCooldown);
    thumbnail_tab_helper_->StartVideoCapture();

    if (cooldown_timer_.IsRunning()) {
      cooldown_timer_.Reset();
    } else {
      constexpr base::TimeDelta kCooldownDelay =
          base::TimeDelta::FromMilliseconds(500);
      cooldown_timer_.Start(
          FROM_HERE, kCooldownDelay,
          base::BindRepeating(&TabStateTracker::OnCooldownEnded,
                              base::Unretained(this)));
    }
  }

  void OnCooldownEnded() {
    if (tab_state_ != TabState::kCaptureCooldown)
      return;

    constexpr size_t kMaxCooldownRetries = 3;
    if (!captured_cooldown_frame_ &&
        cooldown_retry_count_ < kMaxCooldownRetries) {
      cooldown_timer_.Reset();
      return;
    }

    set_tab_state(TabState::kPageLoaded);
    StopCapture();
  }

  void RequestCapture() {
    if (!scoped_capture_)
      scoped_capture_ = std::make_unique<ScopedThumbnailCapture>(this);
  }

  void StopCapture() {
    cooldown_timer_.AbandonAndStop();
    thumbnail_tab_helper_->StopVideoCapture();
    scoped_capture_.reset();
  }

  // The last known visibility WebContents visibility.
  bool visible_;

  // Is the thumbnail being observed?
  bool is_being_observed_ = false;

  // Has a frame been captured during cooldown?
  bool captured_cooldown_frame_ = false;
  size_t cooldown_retry_count_ = 0U;

  // Where we are in the page lifecycle.
  TabState tab_state_ = TabState::kNoPage;

  // Scoped request for video capture. Ensures we always decrement the counter
  // once per increment.
  std::unique_ptr<ScopedThumbnailCapture> scoped_capture_;

  ThumbnailTabHelper* const thumbnail_tab_helper_;

  base::RetainingOneShotTimer cooldown_timer_;
};

// ThumbnailTabHelper ----------------------------------------------------

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : state_(std::make_unique<TabStateTracker>(this, contents)),
      thumbnail_(base::MakeRefCounted<ThumbnailImage>(state_.get())) {}

ThumbnailTabHelper::~ThumbnailTabHelper() {
  StopVideoCapture();
}

// Called when a thumbnail is published to observers. Records what
// method was used to capture the thumbnail.
//
// static
void ThumbnailTabHelper::RecordCaptureType(CaptureType type) {
  UMA_HISTOGRAM_ENUMERATION("Tab.Preview.CaptureType", type);
}

void ThumbnailTabHelper::CaptureThumbnailOnTabHidden() {
  const base::TimeTicks time_of_call = base::TimeTicks::Now();

  // Ignore previous requests to capture a thumbnail on tab switch.
  weak_factory_for_thumbnail_on_tab_hidden_.InvalidateWeakPtrs();

  // Get the WebContents' main view. Note that during shutdown there may not be
  // a view to capture.
  content::RenderWidgetHostView* const source_view = state_->GetView();
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
      base::BindOnce(&ThumbnailTabHelper::StoreThumbnailForTabSwitch,
                     weak_factory_for_thumbnail_on_tab_hidden_.GetWeakPtr(),
                     time_of_call));
}

void ThumbnailTabHelper::StoreThumbnailForTabSwitch(base::TimeTicks start_time,
                                                    const SkBitmap& bitmap) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Tab.Preview.TimeToStoreAfterTabSwitch",
                             base::TimeTicks::Now() - start_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromSeconds(1), 50);
  StoreThumbnail(CaptureType::kCopyFromView, bitmap);
}

void ThumbnailTabHelper::StoreThumbnail(CaptureType type,
                                        const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (bitmap.drawsNothing())
    return;

  RecordCaptureType(type);
  state_->OnFrameCaptured(type);
  thumbnail_->AssignSkBitmap(bitmap);
}

void ThumbnailTabHelper::StartVideoCapture() {
  if (video_capturer_)
    return;

  // This pointer can become null before this method is called - see
  // RenderWidgetHost::GetView() for details.
  content::RenderWidgetHostView* const source_view = state_->GetView();
  if (!source_view)
    return;

  // Get the source size and scale.
  const float scale_factor = source_view->GetDeviceScaleFactor();
  const gfx::Size source_size = source_view->GetViewBounds().size();
  if (source_size.IsEmpty())
    return;

  start_video_capture_time_ = base::TimeTicks::Now();

  // Figure out how large we want the capture target to be.
  last_frame_capture_info_ =
      GetInitialCaptureInfo(source_size, scale_factor,
                            /* include_scrollbars_in_capture */ true);

  const gfx::Size& target_size = last_frame_capture_info_.target_size;
  constexpr int kMaxFrameRate = 3;
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
    video_capturer_->Stop();
    video_capturer_.reset();
  }

  start_video_capture_time_ = base::TimeTicks();
}

void ThumbnailTabHelper::OnFrameCaptured(
    base::ReadOnlySharedMemoryRegion data,
    ::media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<::viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  CHECK(video_capturer_);
  const base::TimeTicks time_of_call = base::TimeTicks::Now();

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

  if (start_video_capture_time_ != base::TimeTicks()) {
    UMA_HISTOGRAM_TIMES("Tab.Preview.TimeToFirstUsableFrameAfterStartCapture",
                        time_of_call - start_video_capture_time_);
    start_video_capture_time_ = base::TimeTicks();
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
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Tab.Preview.TimeToStoreAfterFrameReceived",
        base::TimeTicks::Now() - time_of_call,
        base::TimeDelta::FromMicroseconds(10),
        base::TimeDelta::FromMilliseconds(10), 50);
    StoreThumbnail(CaptureType::kVideoFrame, cropped_frame);
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

  scale_factor = std::max(scale_factor, kMinThumbnailScaleFactor);

  // Minimum thumbnail dimension (in DIP) for tablet tabstrip previews.
  const gfx::Size smallest_thumbnail = GetMinimumThumbnailSize();
  const int smallest_dimension =
      scale_factor *
      std::min(smallest_thumbnail.width(), smallest_thumbnail.height());

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
  const gfx::Size min_target_size =
      gfx::ScaleToFlooredSize(smallest_thumbnail, scale_factor);

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
