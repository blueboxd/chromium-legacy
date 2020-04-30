// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_
#define UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <random>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/events/events_export.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/request_queue.h"
#include "ui/gfx/x/x11_types.h"

using Time = unsigned long;
using XEvent = union _XEvent;
using XID = unsigned long;
using XWindow = unsigned long;

namespace gfx {
class Point;
}

namespace ui {

class X11HotplugEventHandler;
class XScopedEventSelector;
class PlatformEventDispatcher;
class ScopedXEventDispatcher;

// The XEventDispatcher interface is used in two different ways: the first is
// when classes want to receive XEvent directly and second is to say if classes,
// which also implement the PlatformEventDispatcher interface, are able to
// process next translated from XEvent to ui::Event events. Only used with Ozone
// X11 currently.
class EVENTS_EXPORT XEventDispatcher {
 public:
  // Sends XEvent to XEventDispatcher for handling. Returns true if the XEvent
  // was dispatched, otherwise false. After the first XEventDispatcher returns
  // true XEvent dispatching stops.
  virtual bool DispatchXEvent(XEvent* xevent) = 0;

  // XEventDispatchers can be used to test if they are able to process next
  // translated event sent by a PlatformEventSource. If so, they must make a
  // promise internally to process next event sent by PlatformEventSource.
  virtual void CheckCanDispatchNextPlatformEvent(XEvent* xev);

  // Tells that an event has been dispatched and an event handling promise must
  // be removed.
  virtual void PlatformEventDispatchFinished();

  // Returns PlatformEventDispatcher if this XEventDispatcher is associated with
  // a PlatformEventDispatcher as well. Used to explicitly add a
  // PlatformEventDispatcher during a call from an XEventDispatcher to
  // AddXEventDispatcher.
  virtual PlatformEventDispatcher* GetPlatformEventDispatcher();

 protected:
  virtual ~XEventDispatcher() {}
};

// XEventObserver can be installed on a X11EventSource, and it
// receives all events that are dispatched to the dispatchers.
class EVENTS_EXPORT XEventObserver {
 public:
  // Called before the dispatchers receive the event.
  virtual void WillProcessXEvent(XEvent* event) = 0;

  // Called after the event has been dispatched.
  virtual void DidProcessXEvent(XEvent* event) = 0;

 protected:
  virtual ~XEventObserver() {}
};

// Responsible for notifying X11EventSource when new XEvents are available to
// be processed/dispatched.
class X11EventWatcher {
 public:
  X11EventWatcher() = default;
  virtual ~X11EventWatcher() = default;

  // Starts watching for X Events and feeding them into X11EventSource to be
  // processed, through XEventSource::ProcessXEvent(), as they come in.
  virtual void StartWatching() = 0;

  // Stops watching for X Events.
  virtual void StopWatching() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(X11EventWatcher);
};

// A temporary XEventDispatcher can be installed on a X11EventSource that
// overrides all installed event dispatchers, and always gets a chance to
// dispatch the event first, similar to what PlatformEventSource does with
// ScopedEventDispatcher. When this object is destroyed, it removes the
// override-dispatcher, and restores the previous override-dispatcher.
class EVENTS_EXPORT ScopedXEventDispatcher {
 public:
  ScopedXEventDispatcher(XEventDispatcher** scoped_dispatcher,
                         XEventDispatcher* new_dispatcher);
  ~ScopedXEventDispatcher();

  operator XEventDispatcher*() const { return original_; }

 private:
  XEventDispatcher* original_;
  base::AutoReset<XEventDispatcher*> restore_;

  DISALLOW_COPY_AND_ASSIGN(ScopedXEventDispatcher);
};

// PlatformEventSource implementation for X11, both Ozone and non-Ozone.
// Receives X11 events from X11EventWatcher and sends them to registered
// {Platform,X}EventDispatchers. Handles receiving, pre-process, translation
// and post-processing of XEvents.
class EVENTS_EXPORT X11EventSource : public PlatformEventSource,
                                     x11::RequestQueue {
 public:
  explicit X11EventSource(XDisplay* display);
  ~X11EventSource() override;

  static bool HasInstance();
  static X11EventSource* GetInstance();

  // Called when there is a new XEvent available. Processes all (if any)
  // available X events.
  void DispatchXEvents();

  // Dispatches a given event immediately. This is to facilitate sequential
  // interaction between the gtk event loop (used for IME) and the
  // main X11 event loop.
  void DispatchXEventNow(XEvent* event);

  XDisplay* display() { return display_; }

  // Returns the timestamp of the event currently being dispatched.  Falls back
  // on GetCurrentServerTime() if there's no event being dispatched, or if the
  // current event does not have a timestamp.
  Time GetTimestamp();

  // Returns the root pointer location only if there is an event being
  // dispatched that contains that information.
  base::Optional<gfx::Point> GetRootCursorLocationFromCurrentEvent() const;

  // Explicitly asks the X11 server for the current timestamp, and updates
  // |last_seen_server_time_| with this value.
  Time GetCurrentServerTime();

  // Adds a XEvent dispatcher to the XEvent dispatcher list.
  // Also calls XEventDispatcher::GetPlatformEventDispatcher
  // to explicitly add this |dispatcher| to a list of PlatformEventDispatchers
  // in case if XEventDispatcher has a PlatformEventDispatcher. Thus,
  // there is no need to separately add self to the list of
  // PlatformEventDispatchers. This is needed because XEventDispatchers are
  // tested if they can receive an XEvent based on a XID target. If so, the
  // translated XEvent into a PlatformEvent is sent to that
  // PlatformEventDispatcher.
  void AddXEventDispatcher(XEventDispatcher* dispatcher);

  // Removes an XEvent dispatcher from the XEvent dispatcher list.
  // Also explicitly removes an XEventDispatcher from a PlatformEventDispatcher
  // list if the XEventDispatcher has a PlatformEventDispatcher.
  void RemoveXEventDispatcher(XEventDispatcher* dispatcher);

  void AddXEventObserver(XEventObserver* observer);
  void RemoveXEventObserver(XEventObserver* observer);

  // Installs a XEventDispatcher that receives all the events. The dispatcher
  // can process the event, or request that the default dispatchers be invoked
  // by returning false from its DispatchXEvent() override. The returned
  // ScopedXEventDispatcher object is a handler for the overridden dispatcher.
  // When this handler is destroyed, it removes the overridden dispatcher, and
  // restores the previous override-dispatcher (or null if there wasn't any).
  std::unique_ptr<ScopedXEventDispatcher> OverrideXEventDispatcher(
      XEventDispatcher* dispatcher);

  void ProcessXEvent(XEvent* xevent);

 protected:
  // Extracts cookie data from |xevent| if it's of GenericType, and dispatches
  // the event. This function also frees up the cookie data after dispatch is
  // complete.
  void ExtractCookieDataDispatchEvent(XEvent* xevent);

  // Handles updates after event has been dispatched.
  void PostDispatchEvent(XEvent* xevent);

 private:
  friend class ScopedXEventDispatcher;

  struct Request {
    Request(bool is_void, unsigned int sequence, ResponseCallback callback);
    Request(Request&& other);
    ~Request();

    const bool is_void;
    const unsigned int sequence;
    ResponseCallback callback;
  };

  // Tells XEventDispatchers, which can also have PlatformEventDispatchers, that
  // a translated event is going to be sent next, then dispatches the event and
  // notifies XEventDispatchers the event has been sent out and, most probably,
  // consumed.
  void DispatchPlatformEvent(const PlatformEvent& event, XEvent* xevent);

  // Sends XEvent to registered XEventDispatchers.
  void DispatchXEventToXEventDispatchers(XEvent* xevent);

  // PlatformEventSource:
  void StopCurrentEventStream() override;
  void OnDispatcherListChanged() override;

  // x11::RequestQueue
  void AddRequest(bool is_void,
                  unsigned int sequence,
                  ResponseCallback callback) override;

  void RestoreOverridenXEventDispatcher();

  static X11EventSource* instance_;

  std::unique_ptr<X11EventWatcher> watcher_;

  // The connection to the X11 server used to receive the events.
  XDisplay* display_;

  // Event currently being dispatched.
  XEvent* dispatching_event_;

  // State necessary for UpdateLastSeenServerTime
  bool dummy_initialized_;
  ::XWindow dummy_window_;
  ::XAtom dummy_atom_;
  std::unique_ptr<XScopedEventSelector> dummy_window_events_;

  // Keeps track of whether this source should continue to dispatch all the
  // available events.
  bool continue_stream_ = true;

  std::unique_ptr<X11HotplugEventHandler> hotplug_event_handler_;

  // Used to sample RTT measurements, with frequency 1/1000.
  std::default_random_engine generator_;
  std::uniform_int_distribution<int> distribution_;

  // Keep track of all XEventDispatcher to send XEvents directly to.
  base::ObserverList<XEventDispatcher>::Unchecked dispatchers_xevent_;

  base::ObserverList<XEventObserver>::Unchecked observers_;

  XEventDispatcher* overridden_dispatcher_ = nullptr;
  bool overridden_dispatcher_restored_ = false;

  std::queue<Request> requests_;

  DISALLOW_COPY_AND_ASSIGN(X11EventSource);
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_
