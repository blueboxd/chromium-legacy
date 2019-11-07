// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/dawn_context_provider.h"

#include "base/memory/ptr_util.h"

namespace viz {

std::unique_ptr<DawnContextProvider> DawnContextProvider::Create() {
  auto context_provider = base::WrapUnique(new DawnContextProvider());
  if (!context_provider->IsValid())
    return nullptr;
  return context_provider;
}

DawnContextProvider::DawnContextProvider() = default;

DawnContextProvider::~DawnContextProvider() = default;

}  // namespace viz
