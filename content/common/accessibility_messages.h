// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_
#define CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_

// IPC messages for accessibility.

#include "content/common/ax_content_node_data.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_param_traits.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/transform.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START AccessibilityMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::AXContentNodeData)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(actions)
  IPC_STRUCT_TRAITS_MEMBER(string_attributes)
  IPC_STRUCT_TRAITS_MEMBER(int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(float_attributes)
  IPC_STRUCT_TRAITS_MEMBER(bool_attributes)
  IPC_STRUCT_TRAITS_MEMBER(intlist_attributes)
  IPC_STRUCT_TRAITS_MEMBER(html_attributes)
  IPC_STRUCT_TRAITS_MEMBER(child_ids)
  IPC_STRUCT_TRAITS_MEMBER(relative_bounds)
  IPC_STRUCT_TRAITS_MEMBER(child_routing_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AXContentTreeData)
  IPC_STRUCT_TRAITS_MEMBER(tree_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(focused_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(mimetype)
  IPC_STRUCT_TRAITS_MEMBER(doctype)
  IPC_STRUCT_TRAITS_MEMBER(loaded)
  IPC_STRUCT_TRAITS_MEMBER(loading_progress)
  IPC_STRUCT_TRAITS_MEMBER(focus_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_is_backward)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_affinity)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_affinity)
  IPC_STRUCT_TRAITS_MEMBER(routing_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_routing_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AXContentTreeUpdate)
  IPC_STRUCT_TRAITS_MEMBER(has_tree_data)
  IPC_STRUCT_TRAITS_MEMBER(tree_data)
  IPC_STRUCT_TRAITS_MEMBER(node_id_to_clear)
  IPC_STRUCT_TRAITS_MEMBER(root_id)
  IPC_STRUCT_TRAITS_MEMBER(nodes)
  IPC_STRUCT_TRAITS_MEMBER(event_from)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_EventBundleParams)
  // Zero or more updates to the accessibility tree to apply first.
  IPC_STRUCT_MEMBER(std::vector<content::AXContentTreeUpdate>, updates)

  // Zero or more events to fire after the tree updates have been applied.
  IPC_STRUCT_MEMBER(std::vector<ui::AXEvent>, events)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Tells the render view that a AccessibilityHostMsg_EventBundle
// message was processed and it can send additional updates. The argument
// must be the same as the ack_token passed to
// AccessibilityHostMsg_EventBundle.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_EventBundle_ACK, int /* ack_token */)

// Messages sent from the renderer to the browser.

// Sent to notify the browser about renderer accessibility events.
// The browser responds with a AccessibilityMsg_EventBundle_ACK with the same
// ack_token.
// The |reset_token| parameter is set if this IPC was sent in response
// to a reset request from the browser. When the browser requests a reset,
// it ignores incoming IPCs until it sees one with the correct reset token.
// Any other time, it ignores IPCs with a reset token.
IPC_MESSAGE_ROUTED3(AccessibilityHostMsg_EventBundle,
                    AccessibilityHostMsg_EventBundleParams /* params */,
                    int /* reset_token */,
                    int /* ack_token */)

// Sent in response to PerformAction with parameter kHitTest.
IPC_MESSAGE_ROUTED5(AccessibilityHostMsg_ChildFrameHitTestResult,
                    int /* action request id of initial caller */,
                    gfx::Point /* location tested */,
                    int /* routing id of child frame */,
                    int /* browser plugin instance id of child frame */,
                    ax::mojom::Event /* event to fire */)

#endif  // CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_
