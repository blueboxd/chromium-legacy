// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TEXT_INPUT_CLIENT_MESSAGES_H_
#define CONTENT_COMMON_TEXT_INPUT_CLIENT_MESSAGES_H_

#include <stddef.h>

#include "build/build_config.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

#if defined(OS_MACOSX)
#include "content/common/mac/attributed_string_type_converters.h"
#include "ui/base/mojom/attributed_string.mojom.h"
#endif

#define IPC_MESSAGE_START TextInputClientMsgStart
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#if defined(OS_MACOSX)
IPC_STRUCT_TRAITS_BEGIN(ui::mojom::FontAttribute)
  IPC_STRUCT_TRAITS_MEMBER(font_name)
  IPC_STRUCT_TRAITS_MEMBER(font_point_size)
  IPC_STRUCT_TRAITS_MEMBER(effective_range)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::mojom::AttributedString)
  IPC_STRUCT_TRAITS_MEMBER(string)
  IPC_STRUCT_TRAITS_MEMBER(attributes)
IPC_STRUCT_TRAITS_END()
#endif

// Browser -> Renderer Messages ////////////////////////////////////////////////
// These messages are sent from the browser to the renderer. Each one has a
// corresponding reply message.
////////////////////////////////////////////////////////////////////////////////

// Tells the renderer to send back the text fragment in a given range.
IPC_MESSAGE_ROUTED1(TextInputClientMsg_StringForRange,
                    gfx::Range)

// Tells the renderer to send back the word under the given point and its
// baseline point.
IPC_MESSAGE_ROUTED1(TextInputClientMsg_StringAtPoint, gfx::Point)

////////////////////////////////////////////////////////////////////////////////

// Renderer -> Browser Replies /////////////////////////////////////////////////
// These messages are sent in reply to the above messages.
////////////////////////////////////////////////////////////////////////////////

#if defined(OS_MACOSX)
// Reply message for TextInputClientMsg_StringForRange.
IPC_MESSAGE_ROUTED2(TextInputClientReplyMsg_GotStringForRange,
                    ui::mojom::AttributedString,
                    gfx::Point)

// Reply message for TextInputClientMsg_StringAtPoint
IPC_MESSAGE_ROUTED2(TextInputClientReplyMsg_GotStringAtPoint,
                    ui::mojom::AttributedString,
                    gfx::Point)
#endif  // defined(OS_MACOSX)

#endif  // CONTENT_COMMON_TEXT_INPUT_CLIENT_MESSAGES_H_
