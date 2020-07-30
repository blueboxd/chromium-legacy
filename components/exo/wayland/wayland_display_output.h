// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_

#include <stdint.h>

#include "base/containers/flat_map.h"
#include "base/macros.h"

struct wl_client;
struct wl_global;
struct wl_resource;

namespace exo {
namespace wayland {

// Class that represent a wayland output. Tied to a specific display ID
// and associated with a global.
class WaylandDisplayOutput {
 public:
  explicit WaylandDisplayOutput(int64_t display_id);
  ~WaylandDisplayOutput();

  int64_t id() const;
  void set_global(wl_global* global);

  // Register/Unregister output resources, which will be used to
  // notify surface when enter/leave the output.
  void UnregisterOutput(wl_resource* output_resource);
  void RegisterOutput(wl_resource* output_resource);

  wl_resource* GetOutputResourceForClient(wl_client* client);

 private:
  const int64_t id_;
  wl_global* global_ = nullptr;
  base::flat_map<wl_client*, wl_resource*> output_ids_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplayOutput);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
