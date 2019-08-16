// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_SYSTEM_NODE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_SYSTEM_NODE_IMPL_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/public/graph/system_node.h"

namespace performance_manager {

class SystemNodeImpl : public PublicNodeImpl<SystemNodeImpl, SystemNode>,
                       public TypedNodeBase<SystemNodeImpl,
                                            SystemNode,
                                            SystemNodeObserver> {
 public:
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kSystem; }

  explicit SystemNodeImpl(GraphImpl* graph);
  ~SystemNodeImpl() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemNodeImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_SYSTEM_NODE_IMPL_H_
