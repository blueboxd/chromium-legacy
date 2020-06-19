// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WIDGET_MESSAGES_H_
#define CONTENT_COMMON_WIDGET_MESSAGES_H_

// IPC messages for controlling painting and input events.

#include "base/optional.h"
#include "base/time/time.h"
#include "cc/input/touch_action.h"
#include "content/common/common_param_traits_macros.h"
#include "content/common/content_param_traits.h"
#include "content/common/content_to_visible_time_reporter.h"
#include "content/common/visual_properties.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "ui/base/ime/text_input_action.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START WidgetMsgStart

// Traits for WebDeviceEmulationParams.
IPC_STRUCT_TRAITS_BEGIN(blink::WebFloatRect)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebSize)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebDeviceEmulationParams)
  IPC_STRUCT_TRAITS_MEMBER(screen_position)
  IPC_STRUCT_TRAITS_MEMBER(screen_size)
  IPC_STRUCT_TRAITS_MEMBER(view_position)
  IPC_STRUCT_TRAITS_MEMBER(device_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(view_size)
  IPC_STRUCT_TRAITS_MEMBER(scale)
  IPC_STRUCT_TRAITS_MEMBER(viewport_offset)
  IPC_STRUCT_TRAITS_MEMBER(viewport_scale)
  IPC_STRUCT_TRAITS_MEMBER(screen_orientation_angle)
  IPC_STRUCT_TRAITS_MEMBER(screen_orientation_type)
IPC_STRUCT_TRAITS_END()

//
// Browser -> Renderer Messages.
//

// Sent to inform the renderer to invoke a context menu.
// The parameter specifies the location in the render widget's coordinates.
IPC_MESSAGE_ROUTED2(WidgetMsg_ShowContextMenu,
                    ui::MenuSourceType,
                    gfx::Point /* location where menu should be shown */)

// Tells the render widget to close.
// Expects a Close_ACK message when finished.
IPC_MESSAGE_ROUTED0(WidgetMsg_Close)

// Enables device emulation. See WebDeviceEmulationParams for description.
IPC_MESSAGE_ROUTED1(WidgetMsg_EnableDeviceEmulation,
                    blink::WebDeviceEmulationParams /* params */)

// Disables device emulation, enabled previously by EnableDeviceEmulation.
IPC_MESSAGE_ROUTED0(WidgetMsg_DisableDeviceEmulation)

// Sent to inform the widget that it was hidden.  This allows it to reduce its
// resource utilization.
IPC_MESSAGE_ROUTED0(WidgetMsg_WasHidden)

// Tells the render view that it is no longer hidden (see WasHidden).
IPC_MESSAGE_ROUTED3(WidgetMsg_WasShown,
                    base::TimeTicks /* show_request_timestamp */,
                    bool /* was_evicted */,
                    base::Optional<content::RecordContentToVisibleTimeRequest>
                    /* record_tab_switch_time_request */)

// Activate/deactivate the RenderWidget (i.e., set its controls' tint
// accordingly, etc.).
IPC_MESSAGE_ROUTED1(WidgetMsg_SetActive, bool /* active */)

// Reply to WidgetHostMsg_RequestSetBounds, WidgetHostMsg_ShowWidget, and
// FrameHostMsg_ShowCreatedWindow, to inform the renderer that the browser has
// processed the bounds-setting.  The browser may have ignored the new bounds,
// but it finished processing.  This is used because the renderer keeps a
// temporary cache of the widget position while these asynchronous operations
// are in progress.
IPC_MESSAGE_ROUTED0(WidgetMsg_SetBounds_ACK)

// Updates a RenderWidget's visual properties. This should include all
// geometries and compositing inputs so that they are updated atomically.
IPC_MESSAGE_ROUTED1(WidgetMsg_UpdateVisualProperties,
                    content::VisualProperties /* visual_properties */)

// Informs the RenderWidget of its position on the user's screen, as well as
// the position of the native window holding the RenderWidget.
// TODO(danakj): These should be part of UpdateVisualProperties.
IPC_MESSAGE_ROUTED2(WidgetMsg_UpdateScreenRects,
                    gfx::Rect /* widget_screen_rect */,
                    gfx::Rect /* window_screen_rect */)

// Sent by a parent frame to notify its child about the state of the child's
// intersection with the parent's viewport, primarily for use by the
// IntersectionObserver API. Also see FrameHostMsg_UpdateViewportIntersection.
IPC_MESSAGE_ROUTED1(WidgetMsg_SetViewportIntersection,
                    blink::ViewportIntersectionState /* intersection_state */)


// Sent by the browser to synchronize with the next compositor frame by
// requesting an ACK be queued. Used only for tests.
IPC_MESSAGE_ROUTED1(WidgetMsg_WaitForNextFrameForTests,
                    int /* main_frame_thread_observer_routing_id */)

//
// Renderer -> Browser Messages.
//

// Sent by the renderer process to request that the browser close the widget.
// This corresponds to the window.close() API, and the browser may ignore
// this message.  Otherwise, the browser will generate a WidgetMsg_Close
// message to close the widget.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_Close)

// Sent in response to a WidgetMsg_UpdateScreenRects so that the renderer can
// throttle these messages.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_UpdateScreenRects_ACK)

// Sent by the renderer process to request that the browser change the bounds of
// the widget. This corresponds to the window.resizeTo() and window.moveTo()
// APIs, and the browser may ignore this message.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_RequestSetBounds, gfx::Rect /* bounds */)

// Sends a set of queued messages that were being held until the next
// CompositorFrame is being submitted from the renderer. These messages are
// sent before the OnRenderFrameMetadataChanged message is sent (via mojo) and
// before the CompositorFrame is sent to the viz service. The |frame_token|
// will match the token in the about-to-be-submitted CompositorFrame.
IPC_MESSAGE_ROUTED2(WidgetHostMsg_FrameSwapMessages,
                    uint32_t /* frame_token */,
                    std::vector<IPC::Message> /* messages */)

// Indicates that the render widget has been closed in response to a
// Close message.
IPC_MESSAGE_CONTROL1(WidgetHostMsg_Close_ACK, int /* old_route_id */)

// Sent in reply to WidgetMsg_WaitForNextFrameForTests.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_WaitForNextFrameForTests_ACK)

#endif  //  CONTENT_COMMON_WIDGET_MESSAGES_H_
