// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/thumbnails/thumbnail_page_event_adapter.h"
#include "chrome/browser/ui/thumbnails/thumbnail_page_observer.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class ThumbnailTabHelper
    : public content::WebContentsUserData<ThumbnailTabHelper>,
      public content::WebContentsObserver,
      public viz::mojom::FrameSinkVideoConsumer,
      public ThumbnailImage::Delegate {
 public:
  ~ThumbnailTabHelper() override;

  scoped_refptr<ThumbnailImage> thumbnail() const { return thumbnail_; }

 private:
  class ThumanailImageImpl;
  friend class content::WebContentsUserData<ThumbnailTabHelper>;
  friend class ThumanailImageImpl;

  explicit ThumbnailTabHelper(content::WebContents* contents);

  // ThumbnailImage::Delegate:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override;

  bool ShouldKeepUpdatingThumbnail() const;

  void StartVideoCapture();
  void StopVideoCapture();
  void CaptureThumbnailOnTabSwitch();
  void StoreThumbnail(const SkBitmap& bitmap);

  content::RenderWidgetHostView* GetView();

  gfx::Size GetThumbnailSize() const;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // viz::mojom::FrameSinkVideoConsumer:
  void OnFrameCaptured(
      base::ReadOnlySharedMemoryRegion data,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      ::viz::mojom::FrameSinkVideoConsumerFrameCallbacksPtr callbacks) override;
  void OnStopped() override;

  // The last known visibility WebContents visibility.
  content::Visibility last_visibility_;

  // Whether a thumbnail was captured while the tab was loaded, since the tab
  // was last hidden.
  bool captured_loaded_thumbnail_since_tab_hidden_ = false;

  // The estimated percentage of the most recent frame being captured that
  // consists of possible right and bottom scroll bars.
  float last_frame_scrollbar_percent_ = 0.0f;

  // Captures frames from the WebContents while it's hidden. The capturer count
  // of the WebContents is incremented/decremented when a capturer is set/unset.
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;

  // The thumbnail maintained by this instance.
  scoped_refptr<ThumbnailImage> thumbnail_ =
      base::MakeRefCounted<ThumbnailImage>(this);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<ThumbnailTabHelper>
      weak_factory_for_thumbnail_on_tab_hidden_{this};

  DISALLOW_COPY_AND_ASSIGN(ThumbnailTabHelper);
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
