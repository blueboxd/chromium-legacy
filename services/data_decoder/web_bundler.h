// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_WEB_BUNDLER_H_
#define SERVICES_DATA_DECODER_WEB_BUNDLER_H_

#include <vector>

#include "base/files/file.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/data_decoder/public/mojom/resource_snapshot_for_web_bundle.mojom.h"
#include "services/data_decoder/public/mojom/web_bundler.mojom.h"

namespace data_decoder {

class WebBundler : public mojom::WebBundler {
 public:
  WebBundler() = default;
  ~WebBundler() override = default;

  WebBundler(const WebBundler&) = delete;
  WebBundler& operator=(const WebBundler&) = delete;

 private:
  // mojom::WebBundler implementation.
  void Generate(
      std::vector<mojo::PendingRemote<mojom::ResourceSnapshotForWebBundle>>
          snapshots,
      base::File file,
      GenerateCallback callback) override;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_WEB_BUNDLER_H_
