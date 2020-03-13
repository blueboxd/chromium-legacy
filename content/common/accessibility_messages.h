// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_
#define CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_

// IPC messages for accessibility.

#include "content/common/content_export.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_param_traits.h"
#include "ui/gfx/transform.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START AccessibilityMsgStart

// Messages sent from the renderer to the browser.

// Sent in response to PerformAction with parameter kHitTest.
IPC_MESSAGE_ROUTED5(AccessibilityHostMsg_ChildFrameHitTestResult,
                    int /* action request id of initial caller */,
                    gfx::Point /* location tested */,
                    int /* routing id of child frame */,
                    int /* browser plugin instance id of child frame */,
                    ax::mojom::Event /* event to fire */)

#endif  // CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_
