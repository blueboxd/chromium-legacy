// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_MEDIA_ERROR_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_MEDIA_ERROR_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "base/values.h"
#include "base/optional.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/base/media_error.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::MediaErrorDataView, media::MediaError> {
  static media::ErrorCode code(const media::MediaError& input) {
    return input.GetErrorCode();
  }

  static const std::string& message(const media::MediaError& input) {
    return input.GetErrorMessage();
  }

  static base::span<base::Value> frames(const media::MediaError& input) {
    return input.data_->frames;
  }

  static base::span<media::MediaError> causes(const media::MediaError& input) {
    return input.data_->causes;
  }

  static base::Optional<base::Value> data(const media::MediaError& input) {
    if (!input.IsOk()) {
      DCHECK(input.data_);
      return input.data_->data.Clone();
    }
    return base::nullopt;
  }

  static bool Read(media::mojom::MediaErrorDataView data,
                   media::MediaError* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_MEDIA_ERROR_MOJOM_TRAITS_H_
