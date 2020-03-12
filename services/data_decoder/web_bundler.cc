// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/web_bundler.h"

namespace data_decoder {

void WebBundler::Generate(
    std::vector<mojo::PendingRemote<mojom::ResourceSnapshotForWebBundle>>
        snapshots,
    base::File file,
    GenerateCallback callback) {
  // The Web Bundle generation logic is not implemented yet.
  // TODO(crbug.com/1040752): Implement this.
  std::move(callback).Run(0, mojom::WebBundlerError::kNotImplemented);
}

}  // namespace data_decoder
