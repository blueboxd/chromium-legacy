// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_load_script_injector/browser/on_load_script_injector_host.h"

#include <utility>

namespace on_load_script_injector {

OriginScopedScript::OriginScopedScript() = default;

OriginScopedScript::OriginScopedScript(std::vector<url::Origin> origins,
                                       base::ReadOnlySharedMemoryRegion script)
    : origins_(std::move(origins)), script_(std::move(script)) {}

OriginScopedScript& OriginScopedScript::operator=(OriginScopedScript&& other) {
  origins_ = std::move(other.origins_);
  script_ = std::move(other.script_);
  return *this;
}

OriginScopedScript::~OriginScopedScript() = default;

}  // namespace on_load_script_injector
