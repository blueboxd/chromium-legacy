// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_FRAME_WIDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_FRAME_WIDGET_H_

#include "third_party/blink/public/web/web_swap_result.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace cc {
class AnimationHost;
class Layer;
class PaintImage;
}  // namespace cc

namespace blink {

// In interface exposed within Blink from local root frames that provides
// local-root specific things related to compositing and input.
class PLATFORM_EXPORT FrameWidget {
 public:
  virtual ~FrameWidget();

  // Returns the WebWidgetClient, which is implemented outside of blink.
  virtual WebWidgetClient* Client() const = 0;

  // Returns the compositors's AnimationHost for the widget.
  virtual cc::AnimationHost* AnimationHost() const = 0;

  // Set the browser's behavior when overscroll happens, e.g. whether to glow
  // or navigate.
  virtual void SetOverscrollBehavior(
      const cc::OverscrollBehavior& overscroll_behavior) = 0;

  // Posts a task with the given delay, then calls ScheduleAnimation() on the
  // Client().
  virtual void RequestAnimationAfterDelay(const base::TimeDelta&) = 0;

  // Sets the root layer. The |layer| can be null when detaching the root layer.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> layer) = 0;

  // Used to update the active selection bounds. Pass a default-constructed
  // LayerSelection to clear it.
  virtual void RegisterSelection(cc::LayerSelection selection) = 0;

  // Image decode functionality.
  virtual void RequestDecode(const cc::PaintImage&,
                             base::OnceCallback<void(bool)>) = 0;

  // Forwards to WebFrameWidget::NotifySwapAndPresentationTime().
  // The |callback| will be fired when the corresponding renderer frame is
  // submitted (still called "swapped") to the display compositor (either with
  // DidSwap or DidNotSwap).
  virtual void NotifySwapAndPresentationTimeInBlink(
      WebReportTimeCallback swap_callback,
      WebReportTimeCallback presentation_callback) = 0;

  // Enable or disable BeginMainFrameNotExpected signals from the compositor,
  // which are consumed by the blink scheduler.
  virtual void RequestBeginMainFrameNotExpected(bool request) = 0;

  // A stable numeric Id for the local root's compositor. For tracing/debugging
  // purposes.
  virtual int GetLayerTreeId() = 0;

  // Set or get what event handlers exist in the document contained in the
  // WebWidget in order to inform the compositor thread if it is able to handle
  // an input event, or it needs to pass it to the main thread to be handled.
  // The class is the type of input event, and for each class there is a
  // properties defining if the compositor thread can handle the event.
  virtual void SetEventListenerProperties(cc::EventListenerClass,
                                          cc::EventListenerProperties) = 0;
  virtual cc::EventListenerProperties EventListenerProperties(
      cc::EventListenerClass) const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_FRAME_WIDGET_H_
