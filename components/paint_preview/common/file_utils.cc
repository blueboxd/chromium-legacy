// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/file_utils.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"

namespace paint_preview {

std::unique_ptr<PaintPreviewProto> ReadProtoFromFile(
    const base::FilePath& file_path) {
  // TODO(crbug.com/1046925): Use ZeroCopyInputStream instead.
  std::string out;
  std::unique_ptr<PaintPreviewProto> proto =
      std::make_unique<PaintPreviewProto>();
  if (!base::ReadFileToString(file_path, &out) ||
      !(proto->ParseFromString(out)))
    return nullptr;

  return proto;
}

}  // namespace paint_preview
