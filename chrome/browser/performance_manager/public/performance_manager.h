// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_

#include "base/callback.h"
#include "base/location.h"

namespace performance_manager {

class Graph;
class GraphOwned;

// The performance manager is a rendezvous point for communicating with the
// performance manager graph on its dedicated sequence.
class PerformanceManager {
 public:
  // Returns true if the performance manager is initialized. Valid to call from
  // the main thread only.
  static bool IsAvailable();

  // Posts a callback that will run on the PM sequence, and be provided a
  // pointer to the Graph. Valid to call from the main thread only, and only
  // if "IsAvailable" returns true.
  using GraphCallback = base::OnceCallback<void(Graph*)>;
  static void CallOnGraph(const base::Location& from_here,
                          GraphCallback graph_callback);

  // Passes a GraphOwned object into the Graph on the PM sequence. Should only
  // be called from the main thread and only if "IsAvailable" returns true.
  static void PassToGraph(const base::Location& from_here,
                          std::unique_ptr<GraphOwned> graph_owned);

 protected:
  PerformanceManager();
  virtual ~PerformanceManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(PerformanceManager);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
