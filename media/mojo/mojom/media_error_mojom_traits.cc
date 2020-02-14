// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/media_error_mojom_traits.h"

#include "media/base/media_error_codes.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::MediaErrorDataView, media::MediaError>::Read(
    media::mojom::MediaErrorDataView data,
    media::MediaError* output) {
  DCHECK(!output->data_);

  media::ErrorCode code;
  std::string message;
  if (!data.ReadCode(&code))
    return false;

  if (code == media::ErrorCode::kOk)
    return true;

  if (!data.ReadMessage(&message))
    return false;

  output->data_ = std::make_unique<media::MediaError::MediaErrorInternal>(
      code, std::move(message));

  if (!data.ReadFrames(&output->data_->frames))
    return false;

  if (!data.ReadCauses(&output->data_->causes))
    return false;

  if (!data.ReadData(&output->data_->data))
    return false;

  return true;
}

}  // namespace mojo
