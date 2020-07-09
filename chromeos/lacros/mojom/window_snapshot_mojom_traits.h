// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_MOJOM_WINDOW_SNAPSHOT_MOJOM_TRAITS_H_
#define CHROMEOS_LACROS_MOJOM_WINDOW_SNAPSHOT_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "chromeos/lacros/cpp/window_snapshot.h"
#include "chromeos/lacros/mojom/screen_manager.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(LACROS_MOJOM_TRAITS)
    StructTraits<lacros::mojom::WindowSnapshotDataView,
                 lacros::WindowSnapshot> {
  static uint32_t width(const lacros::WindowSnapshot& snapshot) {
    return snapshot.width;
  }
  static uint32_t height(const lacros::WindowSnapshot& snapshot) {
    return snapshot.height;
  }
  static const std::vector<uint8_t>& bitmap(
      const lacros::WindowSnapshot& snapshot) {
    return snapshot.bitmap;
  }
  static bool Read(lacros::mojom::WindowSnapshotDataView,
                   lacros::WindowSnapshot* out);
};

}  // namespace mojo

#endif  // CHROMEOS_LACROS_MOJOM_WINDOW_SNAPSHOT_MOJOM_TRAITS_H_
