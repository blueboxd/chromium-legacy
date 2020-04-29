// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_
#define ASH_PUBLIC_CPP_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"
#include "components/arc/mojom/notifications.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

class ArcNotificationManagerBase;

class ASH_PUBLIC_EXPORT ArcNotificationsHostInitializer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when ARC notifications instance is ready.
    virtual void OnSetArcNotificationsInstance(
        ArcNotificationManagerBase* arc_notification_manager) = 0;

    // Invoked when the ArcNotificationsHostInitializer object (the thing that
    // this observer observes) will be destroyed. In response, the observer,
    // |this|, should call "RemoveObserver(this)", whether directly or
    // indirectly (e.g. via ScopedObserver::Remove).
    virtual void OnArcNotificationInitializerDestroyed(
        ArcNotificationsHostInitializer* initializer) = 0;
  };

  static ArcNotificationsHostInitializer* Get();

  virtual void SetArcNotificationsInstance(
      mojo::PendingRemote<arc::mojom::NotificationsInstance>
          arc_notification_instance) = 0;

  // Adds and removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  ArcNotificationsHostInitializer();
  virtual ~ArcNotificationsHostInitializer();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ARC_NOTIFICATIONS_HOST_INITIALIZER_H_
