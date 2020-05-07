// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
#define CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_

#include "components/viz/common/quads/selection.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/public/common/page_visibility_state.h"
#include "ipc/ipc_message_macros.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/platform/web_text_autosizer_page_info.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::RequestContextType,
                          blink::mojom::RequestContextType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ResourceType,
                          blink::mojom::ResourceType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(
    network::mojom::ContentSecurityPolicySource,
    network::mojom::ContentSecurityPolicySource::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::ContentSecurityPolicyType,
                          network::mojom::ContentSecurityPolicyType::kMaxValue)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::mojom::CursorType,
                              ui::mojom::CursorType::kNull,
                              ui::mojom::CursorType::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebImeTextSpan::Type,
                          blink::WebImeTextSpan::Type::kMisspellingSuggestion)
IPC_ENUM_TRAITS_MAX_VALUE(ui::mojom::ImeTextSpanThickness,
                          ui::mojom::ImeTextSpanThickness::kThick)
IPC_ENUM_TRAITS_MAX_VALUE(content::PageVisibilityState,
                          content::PageVisibilityState::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(viz::Selection<gfx::SelectionBound>)
  IPC_STRUCT_TRAITS_MEMBER(start)
  IPC_STRUCT_TRAITS_MEMBER(end)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebImeTextSpan)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(start_offset)
  IPC_STRUCT_TRAITS_MEMBER(end_offset)
  IPC_STRUCT_TRAITS_MEMBER(underline_color)
  IPC_STRUCT_TRAITS_MEMBER(thickness)
  IPC_STRUCT_TRAITS_MEMBER(background_color)
  IPC_STRUCT_TRAITS_MEMBER(suggestion_highlight_color)
  IPC_STRUCT_TRAITS_MEMBER(remove_on_finish_composing)
  IPC_STRUCT_TRAITS_MEMBER(suggestions)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebTextAutosizerPageInfo)
  IPC_STRUCT_TRAITS_MEMBER(main_frame_width)
  IPC_STRUCT_TRAITS_MEMBER(main_frame_layout_width)
  IPC_STRUCT_TRAITS_MEMBER(device_scale_adjustment)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_COMMON_CONTENT_PARAM_TRAITS_MACROS_H_
