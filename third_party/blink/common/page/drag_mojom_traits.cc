// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/drag_mojom_traits.h"

#include "base/notreached.h"

namespace {

constexpr int allow_all =
    blink::kWebDragOperationCopy | blink::kWebDragOperationLink |
    blink::kWebDragOperationGeneric | blink::kWebDragOperationPrivate |
    blink::kWebDragOperationMove | blink::kWebDragOperationDelete;

}  // namespace

namespace mojo {

// static
blink::mojom::DragOperation
EnumTraits<blink::mojom::DragOperation, blink::WebDragOperation>::ToMojom(
    blink::WebDragOperation op) {
  switch (op) {
    case blink::kWebDragOperationNone:
      return blink::mojom::DragOperation::kNone;
    case blink::kWebDragOperationCopy:
      return blink::mojom::DragOperation::kCopy;
    case blink::kWebDragOperationLink:
      return blink::mojom::DragOperation::kLink;
    case blink::kWebDragOperationGeneric:
      return blink::mojom::DragOperation::kGeneric;
    case blink::kWebDragOperationPrivate:
      return blink::mojom::DragOperation::kPrivate;
    case blink::kWebDragOperationMove:
      return blink::mojom::DragOperation::kMove;
    case blink::kWebDragOperationDelete:
      return blink::mojom::DragOperation::kDelete;
    default:
      // blink::kWebDragOperationEvery is not handled on purpose, as
      // DragOperation should only represent a single operation.
      NOTREACHED();
      return blink::mojom::DragOperation::kNone;
  }
}

// static
bool EnumTraits<blink::mojom::DragOperation, blink::WebDragOperation>::
    FromMojom(blink::mojom::DragOperation op, blink::WebDragOperation* out) {
  switch (op) {
    case blink::mojom::DragOperation::kNone:
      *out = blink::kWebDragOperationNone;
      return true;
    case blink::mojom::DragOperation::kCopy:
      *out = blink::kWebDragOperationCopy;
      return true;
    case blink::mojom::DragOperation::kLink:
      *out = blink::kWebDragOperationLink;
      return true;
    case blink::mojom::DragOperation::kGeneric:
      *out = blink::kWebDragOperationGeneric;
      return true;
    case blink::mojom::DragOperation::kPrivate:
      *out = blink::kWebDragOperationPrivate;
      return true;
    case blink::mojom::DragOperation::kMove:
      *out = blink::kWebDragOperationMove;
      return true;
    case blink::mojom::DragOperation::kDelete:
      *out = blink::kWebDragOperationDelete;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<blink::mojom::AllowedDragOperationsDataView,
                  blink::WebDragOperationsMask>::
    Read(blink::mojom::AllowedDragOperationsDataView data,
         blink::WebDragOperationsMask* out) {
  int op_mask = blink::kWebDragOperationNone;
  if (data.allow_copy())
    op_mask |= blink::kWebDragOperationCopy;
  if (data.allow_link())
    op_mask |= blink::kWebDragOperationLink;
  if (data.allow_generic())
    op_mask |= blink::kWebDragOperationGeneric;
  if (data.allow_private())
    op_mask |= blink::kWebDragOperationPrivate;
  if (data.allow_move())
    op_mask |= blink::kWebDragOperationMove;
  if (data.allow_delete())
    op_mask |= blink::kWebDragOperationDelete;
  if (op_mask == allow_all)
    op_mask = blink::kWebDragOperationEvery;
  *out = static_cast<blink::WebDragOperationsMask>(op_mask);
  return true;
}

}  // namespace mojo
