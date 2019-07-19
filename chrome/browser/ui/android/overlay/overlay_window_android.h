// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_OVERLAY_OVERLAY_WINDOW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_OVERLAY_OVERLAY_WINDOW_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/overlay_window.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

class OverlayWindowAndroid : public content::OverlayWindow {
 public:
  explicit OverlayWindowAndroid(
      content::PictureInPictureWindowController* controller);
  ~OverlayWindowAndroid() override;

  void OnActivityStart(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);
  void OnActivityDestroy(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  bool IsActive() override;
  void Close() override;
  void ShowInactive() override {}
  void Hide() override;
  bool IsVisible() override;
  bool IsAlwaysOnTop() override;
  // Retrieves the window's current bounds, including its window.
  gfx::Rect GetBounds() override;
  void UpdateVideoSize(const gfx::Size& natural_size) override {}
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetAlwaysHidePlayPauseButton(bool is_visible) override {}
  void SetMutedState(MutedState muted_state) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}
  cc::Layer* GetLayerForTesting() override;

 private:
  // A weak reference to Java PictureInPictureActivity object.
  JavaObjectWeakGlobalRef java_ref_;

  content::PictureInPictureWindowController* controller_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_OVERLAY_OVERLAY_WINDOW_ANDROID_H_
