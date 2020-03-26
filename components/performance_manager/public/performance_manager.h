// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

namespace performance_manager {

class Graph;
class GraphOwned;
class PageNode;
class PerformanceManagerMainThreadObserver;

// The performance manager is a rendezvous point for communicating with the
// performance manager graph on its dedicated sequence.
class PerformanceManager {
 public:
  virtual ~PerformanceManager();

  // Returns true if the performance manager is initialized. Valid to call from
  // the main thread only.
  static bool IsAvailable();

  // Posts a callback that will run on the PM sequence. Valid to call from any
  // sequence.
  //
  // Note: If called from the main thread, the |callback| is guaranteed to run
  //       if and only if "IsAvailable()" returns true.
  //
  //       If called from any other sequence, there is no guarantee that the
  //       callback will run. It will depend on if the PerformanceManager was
  //       destroyed before the the task is scheduled.
  static void CallOnGraph(const base::Location& from_here,
                          base::OnceClosure callback);

  // Same as the above, but the callback is provided a pointer to the graph.
  using GraphCallback = base::OnceCallback<void(Graph*)>;
  static void CallOnGraph(const base::Location& from_here,
                          GraphCallback callback);

  // Passes a GraphOwned object into the Graph on the PM sequence. Must only be
  // called if "IsAvailable()" returns true. Valid to call from the main thread
  // only.
  static void PassToGraph(const base::Location& from_here,
                          std::unique_ptr<GraphOwned> graph_owned);

  // Returns a WeakPtr to the PageNode associated with a given WebContents,
  // or a null WeakPtr if there's no PageNode for this WebContents.
  // Valid to call from the main thread only, the returned WeakPtr should only
  // be dereferenced on the PM sequence (e.g. it can be used in a
  // CallOnGraph callback).
  static base::WeakPtr<PageNode> GetPageNodeForWebContents(
      content::WebContents* wc);

  // Adds / removes an observer that is notified of PerformanceManager events
  // that happen on the main thread. Can only be called on the main thread.
  static void AddObserver(PerformanceManagerMainThreadObserver* observer);
  static void RemoveObserver(PerformanceManagerMainThreadObserver* observer);

 protected:
  PerformanceManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(PerformanceManager);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
