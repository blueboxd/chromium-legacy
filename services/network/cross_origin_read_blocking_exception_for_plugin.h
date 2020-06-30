// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CROSS_ORIGIN_READ_BLOCKING_EXCEPTION_FOR_PLUGIN_H_
#define SERVICES_NETWORK_CROSS_ORIGIN_READ_BLOCKING_EXCEPTION_FOR_PLUGIN_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE)
    CrossOriginReadBlockingExceptionForPlugin {
 public:
  // Notifies CORB that |process_id| is proxying requests on behalf of a
  // universal-access plugin and therefore CORB should stop blocking requests
  // marked as ResourceType::kPluginResource.
  //
  // TODO(lukasza, laforge): https://crbug.com/702995: Remove the static
  // ...ForPlugin methods once Flash support is removed from Chromium (probably
  // around 2020 - see https://www.chromium.org/flash-roadmap).
  static void AddExceptionForPlugin(int process_id);

  // Returns true if CORB should ignore a request initiated by a universal
  // access plugin - i.e. if |process_id| has been previously passed to
  // AddExceptionForPlugin.
  static bool ShouldAllowForPlugin(int process_id);

  // Reverts AddExceptionForPlugin.
  static void RemoveExceptionForPlugin(int process_id);

  DISALLOW_COPY_AND_ASSIGN(CrossOriginReadBlockingExceptionForPlugin);
};

}  // namespace network

#endif  // SERVICES_NETWORK_CROSS_ORIGIN_READ_BLOCKING_EXCEPTION_FOR_PLUGIN_H_
