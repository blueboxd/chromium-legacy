// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/tablet_mode.h"

#include "ash/public/cpp/tablet_mode_toggle_observer.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace ash {

namespace {

// Singleton delegate instance.
TabletMode::Delegate* g_delegate = nullptr;

}  // namespace

TabletMode::Delegate::Delegate() {
  DCHECK(!g_delegate);
  g_delegate = this;
}

TabletMode::Delegate::~Delegate() {
  DCHECK_EQ(g_delegate, this);
  g_delegate = nullptr;
}

TabletMode* TabletMode::Get() {
  static base::NoDestructor<TabletMode> instance;
  return instance.get();
}

TabletMode::TabletMode() = default;
TabletMode::~TabletMode() = default;

bool TabletMode::InTabletMode() const {
  // |g_delegate| could be null in unit tests.
  if (!g_delegate)
    return false;

  return g_delegate->InTabletMode();
}

void TabletMode::SetEnabledForTest(bool enabled) {
  DCHECK(g_delegate);
  g_delegate->SetEnabledForTest(enabled);
}

void TabletMode::AddObserver(TabletModeToggleObserver* observer) {
  observers_.AddObserver(observer);
}

void TabletMode::RemoveObserver(TabletModeToggleObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TabletMode::NotifyTabletModeChanged() {
  const bool in_tablet_mode = InTabletMode();
  for (auto& observer : observers_)
    observer.OnTabletModeToggled(in_tablet_mode);
}

}  // namespace ash
