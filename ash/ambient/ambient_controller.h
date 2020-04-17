// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_CONTROLLER_H_

#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/model/photo_model.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_mode_state.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class PrefRegistrySimple;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AmbientContainerView;

// Class to handle all ambient mode functionalities.
class ASH_EXPORT AmbientController : public views::WidgetObserver,
                                     public AmbientModeStateObserver,
                                     public SessionObserver {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  AmbientController();
  ~AmbientController() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AmbientModeStateObserver:
  void OnAmbientModeEnabled(bool enabled) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // Creates and displays the ambient mode screen on top of the lock screen.
  void Show();
  // Destroys the ambient mode screen widget.
  void Destroy();
  // Toggle between show and destroy the ambient mode screen.
  // Should be removed once we delete the shortcut entry point.
  void Toggle();

  PhotoModel* photo_model() { return &photo_model_; }

  // Handles user interactions on the background photo. For now the behavior
  // is showing lock screen contents (login pod and media control view) on top
  // while fading-out the current shown image.
  void OnBackgroundPhotoEvents();

  AmbientContainerView* get_container_view_for_testing() {
    return container_view_;
  }

  const base::OneShotTimer& get_timer_for_testing() const {
    return refresh_timer_;
  }

  bool is_showing() const { return !!container_view_; }

 private:
  void CreateContainerView();
  void DestroyContainerView();
  void RefreshImage();
  void ScheduleRefreshImage();
  void GetNextImage();
  void OnPhotoDownloaded(bool success, const gfx::ImageSkia& image);

  void StartFadeOutAnimation();

  AmbientViewDelegateImpl delegate_{this};
  AmbientContainerView* container_view_ = nullptr;   // Owned by view hierarchy.
  PhotoModel photo_model_;
  AmbientModeState ambient_state_;
  base::OneShotTimer refresh_timer_;
  base::WeakPtrFactory<AmbientController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AmbientController);
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONTROLLER_H_
