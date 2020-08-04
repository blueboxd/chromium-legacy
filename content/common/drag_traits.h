// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DRAG_TRAITS_H_
#define CONTENT_COMMON_DRAG_TRAITS_H_

#include "content/common/drag_event_source_info.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/gfx/geometry/point.h"

#define IPC_MESSAGE_START DragMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::DragEventSourceInfo)
  IPC_STRUCT_TRAITS_MEMBER(event_location)
  IPC_STRUCT_TRAITS_MEMBER(event_source)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(ui::mojom::DragEventSource,
                          ui::mojom::DragEventSource::kMaxValue)

#endif  // CONTENT_COMMON_DRAG_TRAITS_H_
