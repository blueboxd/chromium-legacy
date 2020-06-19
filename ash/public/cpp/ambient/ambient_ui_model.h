// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_UI_MODEL_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_UI_MODEL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

// Enumeration of UI visibility states.
enum class AmbientUiVisibility {
  kShown,
  kHidden,
  kClosed,
};

// Enumeration of ambient UI modes.
enum class AmbientUiMode {
  kLockScreenUi,
  kInSessionUi,
};

// A checked observer which receives notification of changes to the Ambient Mode
// UI model.
class ASH_PUBLIC_EXPORT AmbientUiModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the Ambient Mode UI visibility changed.
  virtual void OnAmbientUiVisibilityChanged(AmbientUiVisibility visibility) = 0;
};

// Models the Ambient Mode UI.
class ASH_PUBLIC_EXPORT AmbientUiModel {
 public:
  static AmbientUiModel* Get();

  AmbientUiModel();
  AmbientUiModel(const AmbientUiModel&) = delete;
  AmbientUiModel& operator=(AmbientUiModel&) = delete;
  ~AmbientUiModel();

  void AddObserver(AmbientUiModelObserver* observer);
  void RemoveObserver(AmbientUiModelObserver* observer);

  // Updates current UI visibility and notifies all subscribers.
  void SetUiVisibility(AmbientUiVisibility visibility);

  // Updates current UI mode.
  void SetUiMode(AmbientUiMode ui_mode);

  AmbientUiVisibility ui_visibility() const { return ui_visibility_; }

  AmbientUiMode ui_mode() const { return ui_mode_; }

 private:
  void NotifyAmbientUiVisibilityChanged();

  AmbientUiVisibility ui_visibility_ = AmbientUiVisibility::kClosed;
  AmbientUiMode ui_mode_ = AmbientUiMode::kLockScreenUi;

  base::ObserverList<AmbientUiModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_UI_MODEL_H_
