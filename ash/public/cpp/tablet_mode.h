// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TABLET_MODE_H_
#define ASH_PUBLIC_CPP_TABLET_MODE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace ash {

class TabletModeController;
class TabletModeToggleObserver;
class TestTabletModeDelegate;

// TabletMode info that is maintained by Ash and allows Chrome to be informed
// when it changes.
class ASH_PUBLIC_EXPORT TabletMode final {
 public:
  // Delegate interface to access tablet mode and change it.
  class Delegate {
   public:
    // Invoked to get tablet mode. Returns true if the system is in tablet mode.
    virtual bool InTabletMode() const = 0;

    // Invoked to force tablet mode for test.
    virtual void SetEnabledForTest(bool enabled) = 0;

   protected:
    Delegate();
    virtual ~Delegate();
  };

  // Returns the singleton instance.
  static TabletMode* Get();

  // Returns true if the system is in tablet mode.
  bool InTabletMode() const;

  void SetEnabledForTest(bool enabled);

  void AddObserver(TabletModeToggleObserver* observer);
  void RemoveObserver(TabletModeToggleObserver* observer);

 private:
  friend class base::NoDestructor<TabletMode>;
  friend class TabletModeController;
  friend class TestTabletModeDelegate;

  TabletMode();
  ~TabletMode();

  // Invoked by TabletModeController to notify tablet mode is changed.
  void NotifyTabletModeChanged();

  base::ObserverList<TabletModeToggleObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(TabletMode);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TABLET_MODE_H_
