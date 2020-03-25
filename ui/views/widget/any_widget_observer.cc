// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/any_widget_observer_singleton.h"

namespace views {

AnyWidgetObserver::AnyWidgetObserver(AnyWidgetObserver::Passkey passkey)
    : AnyWidgetObserver() {}
AnyWidgetObserver::AnyWidgetObserver(test::AnyWidgetTestPasskey passkey)
    : AnyWidgetObserver() {}

AnyWidgetObserver::AnyWidgetObserver() {
  internal::AnyWidgetObserverSingleton::GetInstance()->AddObserver(this);
}

AnyWidgetObserver::~AnyWidgetObserver() {
  internal::AnyWidgetObserverSingleton::GetInstance()->RemoveObserver(this);
}

#define PROPAGATE_NOTIFICATION(method, callback)   \
  void AnyWidgetObserver::method(Widget* widget) { \
    if (callback)                                  \
      callback.Run(widget);                        \
  }

PROPAGATE_NOTIFICATION(OnAnyWidgetInitialized, initialized_callback_)
PROPAGATE_NOTIFICATION(OnAnyWidgetShown, shown_callback_)
PROPAGATE_NOTIFICATION(OnAnyWidgetHidden, hidden_callback_)
PROPAGATE_NOTIFICATION(OnAnyWidgetClosing, closing_callback_)

#undef PROPAGATE_NOTIFICATION

}  // namespace views
