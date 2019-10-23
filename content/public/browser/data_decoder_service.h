// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DATA_DECODER_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_DATA_DECODER_SERVICE_H_

#include "content/common/content_export.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"

namespace content {

// Launches a new isolated instance of the Data Decoder service. This instance
// will live as long as the returned Remote is kept alive and bound. Instances
// can be used to batch multiple decoding operations, but callers should be wary
// of sending data from multiple untrusted sources to the same instance.
CONTENT_EXPORT mojo::Remote<data_decoder::mojom::DataDecoderService>
LaunchDataDecoder();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DATA_DECODER_SERVICE_H_
