// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_WINDOW_SNAPSHOT_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_WINDOW_SNAPSHOT_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "chromeos/crosapi/cpp/window_snapshot.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(CROSAPI_MOJOM_TRAITS)
    StructTraits<crosapi::mojom::WindowSnapshotDataView,
                 crosapi::WindowSnapshot> {
  static uint32_t width(const crosapi::WindowSnapshot& snapshot) {
    return snapshot.width;
  }
  static uint32_t height(const crosapi::WindowSnapshot& snapshot) {
    return snapshot.height;
  }
  static const std::vector<uint8_t>& bitmap(
      const crosapi::WindowSnapshot& snapshot) {
    return snapshot.bitmap;
  }
  static bool Read(crosapi::mojom::WindowSnapshotDataView,
                   crosapi::WindowSnapshot* out);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_WINDOW_SNAPSHOT_MOJOM_TRAITS_H_
