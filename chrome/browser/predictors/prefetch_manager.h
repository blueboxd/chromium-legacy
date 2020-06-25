// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREFETCH_MANAGER_H_
#define CHROME_BROWSER_PREDICTORS_PREFETCH_MANAGER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/id_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace blink {
class ThrottlingURLLoader;
}

namespace network {
namespace mojom {
class URLLoaderClient;
}
class SharedURLLoaderFactory;
}

namespace predictors {

struct PrefetchRequest;
class PrefetchManager;

struct PrefetchStats {
  explicit PrefetchStats(const GURL& url);
  ~PrefetchStats();

  PrefetchStats(const PrefetchStats&) = delete;
  PrefetchStats& operator=(const PrefetchStats&) = delete;

  GURL url;
  base::TimeTicks start_time;
  // TODO(falken): Add stats about what was requested to measure
  // the accuracy.
};

// Stores the status of all prefetches associated with a given |url|.
struct PrefetchInfo {
  PrefetchInfo(const GURL& url, PrefetchManager& manager);
  ~PrefetchInfo();

  PrefetchInfo(const PrefetchInfo&) = delete;
  PrefetchInfo& operator=(const PrefetchInfo&) = delete;

  // Called by PrefetchJob only.
  void OnJobCreated();
  void OnJobDestroyed();

  bool is_done() const { return job_count == 0; }

  GURL url;
  size_t job_count = 0;
  bool was_canceled = false;
  std::unique_ptr<PrefetchStats> stats;
  // Owns |this|.
  PrefetchManager* const manager;

  base::WeakPtrFactory<PrefetchInfo> weak_factory{this};
};

// Stores all data need for running a prefetch to a |url|.
struct PrefetchJob {
  PrefetchJob(PrefetchRequest prefetch_request, PrefetchInfo& info);
  ~PrefetchJob();

  PrefetchJob(const PrefetchJob&) = delete;
  PrefetchJob& operator=(const PrefetchJob&) = delete;

  GURL url;
  net::NetworkIsolationKey network_isolation_key;
  // PrefetchJob can outlive PrefetchInfo.
  base::WeakPtr<PrefetchInfo> info;
};

// PrefetchManager prefetches input lists of URLs.
//  - The input list of URLs is associated with a main frame url that can be
//  used for cancelling.
//  - Limits the total number of prefetches in flight.
//  - All methods of the class must be called on the UI thread.
//
//  This class is very similar to PreconnectManager, which does
//  preresolve/preconnect instead of prefetching. It is only
//  usable when LoadingPredictorPrefetch is enabled.
class PrefetchManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when all prefetch jobs for the |stats->url| are finished.
    // Called on the UI thread.
    virtual void PrefetchFinished(std::unique_ptr<PrefetchStats> stats) = 0;
  };

  static const size_t kMaxInflightJobs = 3;

  PrefetchManager(base::WeakPtr<Delegate> delegate, Profile* profile);
  ~PrefetchManager();

  PrefetchManager(const PrefetchManager&) = delete;
  PrefetchManager& operator=(const PrefetchManager&) = delete;

  // Starts prefetch jobs keyed by |url|.
  void Start(const GURL& url, std::vector<PrefetchRequest> requests);

  // Stops further prefetch jobs keyed by |url|. Queued jobs will never start;
  // started jobs will continue to completion.
  void Stop(const GURL& url);

  base::WeakPtr<PrefetchManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Called by PrefetchInfo.
  void AllPrefetchJobsForUrlFinished(PrefetchInfo& info);

 private:
  friend class PrefetchManagerTest;

  void PrefetchUrl(std::unique_ptr<PrefetchJob> job,
                   scoped_refptr<network::SharedURLLoaderFactory> factory);
  void OnPrefetchFinished(
      std::unique_ptr<PrefetchJob> job,
      std::unique_ptr<blink::ThrottlingURLLoader> loader,
      std::unique_ptr<network::mojom::URLLoaderClient> client);
  void TryToLaunchPrefetchJobs();

  base::WeakPtr<Delegate> delegate_;
  Profile* const profile_;

  // All the jobs that haven't yet started. A job is removed once it starts.
  // Inflight jobs destruct once finished.
  std::list<std::unique_ptr<PrefetchJob>> queued_jobs_;

  std::map<GURL, std::unique_ptr<PrefetchInfo>> prefetch_info_;

  // The total number of prefetches that have started and not yet finished,
  // across all main frame URLs.
  size_t inflight_jobs_count_ = 0;

  base::WeakPtrFactory<PrefetchManager> weak_factory_{this};
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREFETCH_MANAGER_H_
