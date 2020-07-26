// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_CAPTURE_RESULT_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_CAPTURE_RESULT_H_

#include "base/containers/flat_map.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-forward.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

// A subset of PaintPreviewCaptureParams that will be filled in by
// PaintPreviewClient. This type mainly exists to aggregate related parameters.
struct RecordingParams {
  explicit RecordingParams(const base::UnguessableToken& document_guid);

  // The document GUID for this capture.
  const base::UnguessableToken document_guid;

  // The rect to which to clip the capture to.
  gfx::Rect clip_rect;

  // Whether the capture is for the main frame or an OOP subframe.
  bool is_main_frame;

  // The maximum capture size allowed per SkPicture captured. A size of 0 is
  // unlimited.
  // TODO(crbug/1071446): Ideally, this would cap the total size rather than
  // being a per SkPicture limit. However, that is non-trivial due to the
  // async ordering of captures from different frames making it hard to keep
  // track of available headroom at the time of each capture triggering.
  size_t max_per_capture_size;
};

// The result of a capture of a WebContents, which may contain recordings of
// multiple subframes.
struct CaptureResult {
 public:
  explicit CaptureResult(mojom::Persistence persistence);
  ~CaptureResult();

  CaptureResult(CaptureResult&&);
  CaptureResult& operator=(CaptureResult&&);

  // Will match the |persistence| in the original capture request.
  mojom::Persistence persistence;

  PaintPreviewProto proto = {};

  // Maps frame embedding tokens to buffers containing the serialized
  // recordings. See |PaintPreviewCaptureResponse::skp| for information on how
  // to intepret these buffers. Empty if |Persistence::FileSystem|.
  base::flat_map<base::UnguessableToken, mojo_base::BigBuffer> serialized_skps =
      {};

  // Indicates that at least one subframe finished successfully.
  bool capture_success = false;
};

}  // namespace paint_preview

#endif
