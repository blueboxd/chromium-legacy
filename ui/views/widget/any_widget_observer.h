// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_ANY_WIDGET_OBSERVER_H_
#define UI_VIEWS_WIDGET_ANY_WIDGET_OBSERVER_H_

#include "base/callback.h"
#include "base/observer_list_types.h"
#include "ui/views/views_export.h"

namespace views {

namespace internal {
class AnyWidgetObserverSingleton;
}

namespace test {
class AnyWidgetTestPasskey;
}

class Widget;

// AnyWidgetObserver is used when you want to observe widget changes but don't
// have an easy way to get a reference to the Widget in question, usually
// because it is created behind a layer of abstraction that hides the Widget.
//
// This class should only be used as a last resort - if you find yourself
// reaching for it in production code, it probably means some parts of your
// system aren't coupled enough or your API boundaries are hiding too much. You
// will need review from an owner of this class to add new uses of it, because
// it requires a Passkey to construct it - see //docs/patterns/passkey.md for
// details. Uses in tests can be done freely using
// views::test::AnyWidgetTestPasskey.
//
// This class is useful when doing something like this:
//
//    RunLoop run_loop;
//    AnyWidgetCallbackObserver observer(views::test::AnyWidgetTestPasskey{});
//    Widget* widget;
//    observer.set_shown_callback(
//        base::BindLambdaForTesting([&](Widget* w) {
//            if (w->GetName() == "MyWidget") {
//                widget = w;
//                run_loop.Quit();
//            }
//        }));
//    ThingThatCreatesAndShowsWidget();
//    run_loop.Run();
//
// with your widget getting the name "MyWidget" via InitParams::name.
// TODO(ellyjones): Add Widget::SetDebugName and add a remark about that here.
//
// This allows you to create a test that is robust even if
// ThingThatCreatesAndShowsWidget() later becomes asynchronous, takes a few
// milliseconds, etc.
class VIEWS_EXPORT AnyWidgetObserver : public base::CheckedObserver {
 public:
  using AnyWidgetCallback = base::RepeatingCallback<void(Widget*)>;

  class Passkey {
   private:
    Passkey();

    // Add friend classes here that are allowed to use AnyWidgetObserver in
    // production code.
  };

  // Using this in production requires an AnyWidgetObserver::Passkey, which can
  // only be constructed by a list of allowed friend classes...
  explicit AnyWidgetObserver(Passkey passkey);

  // ... but for tests or debugging, anyone can construct a
  // views::test::AnyWidgetTestPasskey.
  explicit AnyWidgetObserver(test::AnyWidgetTestPasskey passkey);

  ~AnyWidgetObserver() override;

  AnyWidgetObserver(const AnyWidgetObserver& other) = delete;
  AnyWidgetObserver& operator=(const AnyWidgetObserver& other) = delete;

  // This sets the callback for when the Widget has finished initialization and
  // is ready to use.  This is the first point at which the widget is useable.
  void set_initialized_callback(const AnyWidgetCallback& callback) {
    initialized_callback_ = callback;
  }

  // These set the callbacks for when the backing native widget has just been
  // asked to change visibility. Note that the native widget may or may not
  // actually be drawn on the screen when these callbacks are run, because there
  // are more layers of asynchronousness at the OS layer.
  void set_shown_callback(const AnyWidgetCallback& callback) {
    shown_callback_ = callback;
  }
  void set_hidden_callback(const AnyWidgetCallback& callback) {
    hidden_callback_ = callback;
  }

  // This sets the callback for when the Widget has decided that it is about to
  // close, but not yet begun to tear down the backing native Widget or the
  // RootView. This is the last point at which the Widget is in a consistent,
  // useable state.
  void set_closing_callback(const AnyWidgetCallback& callback) {
    closing_callback_ = callback;
  }

  // These two methods deliberately don't exist:
  //   void set_created_callback(...)
  //   void set_destroyed_callback(...)
  // They would be called respectively too early and too late in the Widget's
  // lifecycle for it to be usefully identified - at construction time the
  // Widget has no properties or contents yet (and therefore can't be
  // meaningfully identified as "your widget" for test purposes), and at
  // destruction time the Widget's state is already partly destroyed by the
  // closure process. Both methods are deliberately left out to avoid dangerous
  // designs based on them.

 private:
  friend class internal::AnyWidgetObserverSingleton;

  AnyWidgetObserver();

  // Called from the singleton to propagate events to each AnyWidgetObserver.
  void OnAnyWidgetInitialized(Widget* widget);
  void OnAnyWidgetShown(Widget* widget);
  void OnAnyWidgetHidden(Widget* widget);
  void OnAnyWidgetClosing(Widget* widget);

  AnyWidgetCallback initialized_callback_;
  AnyWidgetCallback shown_callback_;
  AnyWidgetCallback hidden_callback_;
  AnyWidgetCallback closing_callback_;
};

namespace test {

// A passkey class to allow tests to use AnyWidgetObserver without needing a
// views owner signoff.
class AnyWidgetTestPasskey {
 public:
  AnyWidgetTestPasskey() = default;
};

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_WIDGET_ANY_WIDGET_OBSERVER_H_
