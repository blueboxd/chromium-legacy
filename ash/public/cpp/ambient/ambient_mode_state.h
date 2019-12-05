// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_MODE_STATE_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_MODE_STATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

// An interface implemented by classes who want to be notified about ambient
// state change event.
class ASH_PUBLIC_EXPORT AmbientModeStateObserver
    : public base::CheckedObserver {
 public:
  // Invoked when the Ambient Mode has been enabled/disabled by user
  // entering/exiting this mode.
  virtual void OnAmbientModeEnabled(bool enabled) = 0;
};

// A class that stores Ambient Mode related states.
class ASH_PUBLIC_EXPORT AmbientModeState {
 public:
  static AmbientModeState* Get();

  AmbientModeState();
  AmbientModeState(const AmbientModeState&) = delete;
  AmbientModeState& operator=(AmbientModeState&) = delete;
  ~AmbientModeState();

  void AddObserver(AmbientModeStateObserver* observer);
  void RemoveObserver(AmbientModeStateObserver* observer);

  // Sets the enabled/disabled state for Ambient Mode.
  void SetAmbientModeEnabled(bool enabled);

  // Returns the enabled/disabled state for Ambient Mode.
  bool enabled() const { return enabled_; }

 private:
  void NotifyAmbientModeEnabled();

  // Whether Ambient Mode is enabled.
  bool enabled_ = false;

  base::ObserverList<AmbientModeStateObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_MODE_STATE_H_
