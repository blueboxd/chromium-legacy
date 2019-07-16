// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/hint_cache.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class FilePath;
}  // namespace base
namespace network {
class SharedURLLoaderFactory;
}  // namespace network
namespace optimization_guide {
struct HintsComponentInfo;
class HintsFetcher;
class OptimizationGuideService;
class TopHostProvider;
namespace proto {
class Hint;
}  // namespace proto
}  // namespace optimization_guide

namespace previews {

class PreviewsHints;
class PreviewsUserData;

// A Previews optimization guide that makes decisions guided by hints received
// from the OptimizationGuideService.
class PreviewsOptimizationGuide
    : public optimization_guide::OptimizationGuideServiceObserver {
 public:
  // The embedder guarantees |optimization_guide_service| outlives |this|.
  // The embedder guarantees that |previews_top_host_provider_| outlives |this|.
  PreviewsOptimizationGuide(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      const base::FilePath& profile_path,
      PrefService* pref_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      optimization_guide::TopHostProvider* top_host_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~PreviewsOptimizationGuide() override;

  // Returns whether |type| is whitelisted for |url|. If so |out_ect_threshold|
  // provides the maximum effective connection type to trigger the preview for.
  // |previews_data| can be modified (for further details provided by hints).
  // Virtual so it can be mocked in tests.
  virtual bool IsWhitelisted(
      PreviewsUserData* previews_data,
      const GURL& url,
      PreviewsType type,
      net::EffectiveConnectionType* out_ect_threshold) const;

  // Returns whether |type| is blacklisted for |url|.
  // Virtual so it can be mocked in tests.
  virtual bool IsBlacklisted(const GURL& url, PreviewsType type) const;

  // Returns whether |request| may have associated optimization hints
  // (specifically, PageHints). If so, but the hints are not available
  // synchronously, this method will request that they be loaded (from disk or
  // network). The callback is run after the hint is loaded and can be used as
  // a signal during tests.
  bool MaybeLoadOptimizationHints(const GURL& url, base::OnceClosure callback);

  // Whether |url| has loaded resource loading hints and, if it does, populates
  // |out_resource_patterns_to_block| with the resource patterns to block.
  bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) const;

  // Logs UMA for whether the OptimizationGuide HintCache has a matching Hint
  // guidance for |url|. This is useful for measuring the effectiveness of the
  // page hints provided by Cacao.
  void LogHintCacheMatch(const GURL& url,
                         bool is_committed,
                         net::EffectiveConnectionType ect) const;

  // optimization_guide::OptimizationGuideServiceObserver implementation:
  // Called by OptimizationGuideService when a new component is available for
  // processing.
  void OnHintsComponentAvailable(
      const optimization_guide::HintsComponentInfo& info) override;

  PreviewsHints* GetHintsForTesting() { return hints_.get(); }

  // |next_update_closure| is called the next time OnHintsComponentAvailable is
  // called and the corresponding hints have been updated.
  void ListenForNextUpdateForTesting(base::OnceClosure next_update_closure);

  // Updates the hints to the latest hints sent by the Component Updater.
  // |update_closure| is called once the hints are updated. Public for testing.
  void UpdateHints(base::OnceClosure update_closure,
                   std::unique_ptr<PreviewsHints> hints);

  // Clear all fetched hints known to |this|, including those persisted on disk.
  void ClearFetchedHints();

  bool has_hints() const { return !!hints_; }

  // Set |time_clock_| for testing.
  void SetTimeClockForTesting(const base::Clock* time_clock);

  // Set |hints_fetcher_| for testing.
  void SetHintsFetcherForTesting(
      std::unique_ptr<optimization_guide::HintsFetcher> hints_fetcher);

  optimization_guide::HintsFetcher* GetHintsFetcherForTesting();

  // Called when the hints store is initialized to determine when hints
  // should be fetched and schedules the |hints_fetch_timer_| to fire based on:
  // 1. The update time for the fetched hints in the store and
  // 2. The last time a fetch attempt was made, |last_fetch_attempt_|.
  void ScheduleHintsFetch();

 protected:
  // Callback executed after remote hints have been fetched and returned from
  // the remote Optimization Guide Service. At this point, the hints response
  // is ready to be processed and stored for use. Virtual to be mocked in
  // testing.
  virtual void OnHintsFetched(
      base::Optional<
          std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
          get_hints_response);

  // Callback executed after the Hints have been successfully stored in the
  // store. Virtual to be mocked in tests.
  virtual void OnFetchedHintsStored();

 private:
  // Callback run after the hint cache is fully initialized. At this point, the
  // PreviewsOptimizationGuide is ready to process components from the
  // OptimizationGuideService and registers as an observer with it.
  void OnHintCacheInitialized();

  // Called when the hints have been fully updated with the latest hints from
  // the Component Updater. This is used as a signal during tests.
  // |update_closure| is called immediately if not null.
  void OnHintsUpdated(base::OnceClosure update_closure);

  // Callback when a hint is loaded.
  void OnLoadedHint(base::OnceClosure callback,
                    const GURL& document_url,
                    const optimization_guide::proto::Hint* loaded_hint) const;

  // Method to request new hints for user's sites based on
  // engagement scores using |hints_fetcher_|.
  void FetchHints();

  // Return the time when a hints fetch request was last attempted.
  base::Time GetLastHintsFetchAttemptTime() const;

  // Set the time when a hints fetch was last attempted to |last_attempt_time|.
  void SetLastHintsFetchAttemptTime(base::Time last_attempt_time);

  // The OptimizationGuideService that this guide is listening to. Not owned.
  optimization_guide::OptimizationGuideService* optimization_guide_service_;

  // Runner for UI thread tasks.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Background thread where hints processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The hint cache used by PreviewsHints. It is owned by
  // PreviewsOptimizationGuide so that the existing hint cache can be reused on
  // component updates. Otherwise, a new cache and store would need to be
  // created during each component update.
  std::unique_ptr<optimization_guide::HintCache> hint_cache_;

  // The current hints used for this optimization guide.
  std::unique_ptr<PreviewsHints> hints_;

  // Used in testing to subscribe to an update event in this class.
  base::OnceClosure next_update_closure_;

  // HintsFetcher handles making the request for updated hints from the remote
  // Optimization Guide Service.
  std::unique_ptr<optimization_guide::HintsFetcher> hints_fetcher_;

  // Timer to schedule when to fetch hints from the remote Optimization Guide
  // Service.
  base::OneShotTimer hints_fetch_timer_;

  // TopHostProvider that this guide can query. Not owned.
  optimization_guide::TopHostProvider* top_host_provider_ = nullptr;

  // Clock used for scheduling the |hints_fetch_timer_|.
  const base::Clock* time_clock_;

  // A reference to the PrefService for this profile. Not owned.
  PrefService* pref_service_ = nullptr;

  // Used for fetching Hints by the Hints Fetcher.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<PreviewsOptimizationGuide> ui_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PreviewsOptimizationGuide);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
