// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/data_decoder_service.h"

#include "content/public/browser/service_process_host.h"

namespace content {

mojo::Remote<data_decoder::mojom::DataDecoderService> LaunchDataDecoder() {
  return ServiceProcessHost::Launch<data_decoder::mojom::DataDecoderService>(
      ServiceProcessHost::Options()
          .WithSandboxType(service_manager::SANDBOX_TYPE_UTILITY)
          .WithDisplayName("Data Decoder Service")
          .Pass());
}

}  // namespace content
