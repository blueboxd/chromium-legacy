// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_OVERLAY_OVERLAY_WINDOW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_OVERLAY_OVERLAY_WINDOW_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/overlay_window.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_observer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

class OverlayWindowAndroid : public content::OverlayWindow,
                             public ui::WindowAndroidObserver {
 public:
  explicit OverlayWindowAndroid(
      content::PictureInPictureWindowController* controller);
  ~OverlayWindowAndroid() override;

  void OnActivityStart(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jwindow_android);
  void Destroy(JNIEnv* env);

  // ui::WindowAndroidObserver implementation.
  void OnCompositingDidCommit() override {}
  void OnRootWindowVisibilityChanged(bool visible) override {}
  void OnAttachCompositor() override {}
  void OnDetachCompositor() override {}
  void OnAnimate(base::TimeTicks frame_begin_time) override {}
  void OnActivityStopped() override;
  void OnActivityStarted() override {}
  void OnCursorVisibilityChanged(bool visible) override {}
  void OnFallbackCursorModeToggled(bool is_on) override {}

  // OverlayWindow implementation.
  bool IsActive() override;
  void Close() override;
  void ShowInactive() override {}
  void Hide() override;
  bool IsVisible() override;
  bool IsAlwaysOnTop() override;
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
  ui::WindowAndroid* window_android_;

  content::PictureInPictureWindowController* controller_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_OVERLAY_OVERLAY_WINDOW_ANDROID_H_
