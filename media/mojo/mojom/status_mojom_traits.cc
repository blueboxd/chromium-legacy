// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/status_mojom_traits.h"

#include "media/base/status_codes.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::StatusDataView, media::Status>::Read(
    media::mojom::StatusDataView data,
    media::Status* output) {
  DCHECK(!output->data_);

  media::StatusCode code;
  std::string message;
  if (!data.ReadCode(&code))
    return false;

  if (media::StatusCode::kOk == code)
    return true;

  if (!data.ReadMessage(&message))
    return false;

  output->data_ =
      std::make_unique<media::Status::StatusInternal>(code, std::move(message));

  if (!data.ReadFrames(&output->data_->frames))
    return false;

  if (!data.ReadCauses(&output->data_->causes))
    return false;

  if (!data.ReadData(&output->data_->data))
    return false;

  return true;
}

}  // namespace mojo
