// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_frame_sink_manager.h"

namespace viz {

TestFrameSinkManagerImpl::TestFrameSinkManagerImpl() = default;

TestFrameSinkManagerImpl::~TestFrameSinkManagerImpl() = default;

void TestFrameSinkManagerImpl::BindReceiver(
    mojo::PendingReceiver<mojom::FrameSinkManager> receiver,
    mojom::FrameSinkManagerClientPtr client) {
  receiver_.Bind(std::move(receiver));
  client_ = std::move(client);
}

}  // namespace viz
