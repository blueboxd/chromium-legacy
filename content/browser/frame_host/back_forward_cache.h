// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_BACK_FORWARD_CACHE_H_
#define CONTENT_BROWSER_FRAME_HOST_BACK_FORWARD_CACHE_H_

#include <list>
#include <memory>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHostImpl;

// BackForwardCache:
//
// After the user navigates away from a document, the old one goes into the
// frozen state and is kept in this object. They can potentially be reused
// after an history navigation. Reusing a document means swapping it back with
// the current_frame_host.
class CONTENT_EXPORT BackForwardCache {
 public:
  BackForwardCache();
  ~BackForwardCache();

  // Returns true when a RenderFrameHost can be stored into the
  // BackForwardCache. Depends on the |render_frame_host| and its children's
  // state.
  bool CanStoreDocument(RenderFrameHostImpl* render_frame_host);

  // Moves |rfh| into the BackForwardCache. It can be reused in
  // a future history navigation by using RestoreDocument(). When the
  // BackForwardCache is full, the least recently used document is evicted.
  // Precondition: CanStoreDocument(render_frame_host).
  void StoreDocument(std::unique_ptr<RenderFrameHostImpl> rfh);

  // Iterates over all the RenderViewHost inside |main_rfh| and freeze or
  // resume them.
  static void Freeze(RenderFrameHostImpl* main_rfh);
  static void Resume(RenderFrameHostImpl* main_rfh);

  // Returns a pointer to a cached RenderFrameHost matching
  // |navigation_entry_id| if it exists in the BackForwardCache. Returns nullptr
  // if no matching document is found.
  //
  // Note: The returned pointer should be used temporarily only within the
  // execution of a single task on the event loop. Beyond that, there is no
  // guarantee the pointer will be valid, because the document may be
  // removed/evicted from the cache.
  RenderFrameHostImpl* GetDocument(int navigation_entry_id);

  // During a history navigation, move a document out of the BackForwardCache
  // knowing its |navigation_entry_id|. Returns nullptr when none is found.
  std::unique_ptr<RenderFrameHostImpl> RestoreDocument(int navigation_entry_id);

  // Remove all entries from the BackForwardCache.
  void Flush();

  // Posts a task to destroy all frames in the BackForwardCache that have been
  // marked as evicted.
  void PostTaskToDestroyEvictedFrames();

  // Storing frames in back-forward cache is not supported indefinitely
  // due to potential privacy issues and memory leaks. Instead we are evicting
  // the frame from the cache after the time to live, which can be controlled
  // via experiment.
  static base::TimeDelta GetTimeToLiveInBackForwardCache();

  // Returns the task runner that should be used by the eviction timer.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return task_runner_for_testing_ ? task_runner_for_testing_
                                    : base::ThreadTaskRunnerHandle::Get();
  }

  // Inject task runner for precise timing control in browser tests.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    task_runner_for_testing_ = task_runner;
  }

  // List of reasons the BackForwardCache was disabled for a specific test. If a
  // test needs to be disabled for a reason not covered below, please add to
  // this enum.
  enum DisableForTestingReason {
    // The test has expectations that won't make sense if caching is enabled.
    //
    // One alternative to disabling the test is to make the test's logic
    // conditional, based on whether or not BackForwardCache is enabled.
    //
    // You should also consider whether it would make sense to instead
    // split into two tests, one using a cacheable page, and one using an
    // uncacheable page.
    //
    // Once BackForwardCache is enabled everywhere, any tests still disabled for
    // this reason should change their expectations to permanently match the
    // BackForwardCache enabled behavior.
    TEST_ASSUMES_NO_CACHING,

    // Unload events never fire for documents that are put into the
    // BackForwardCache. This is by design, as there is never an appropriate
    // moment to fire unload if the document is cached.
    // In short, this is because:
    //
    // * We can't fire unload when going into the cache, because it may be
    // destructive, and put the document into an unknown/bad state. Pages can
    // also be cached and restored multiple times, and we don't want to invoke
    // unload more than once.
    //
    // * We can't fire unload when the document is evicted from the cache,
    // because at that point we don't want to run javascript for privacy and
    // security reasons.
    //
    // An alternative to disabling the test, is to have the test load a page
    // that is ineligible for caching (e.g. due to an unsupported feature).
    TEST_USES_UNLOAD_EVENT,
  };

  // Disables the BackForwardCache so that no documents will be stored/served.
  // This allows tests to "force" not using the BackForwardCache, this can be
  // useful when:
  // * Tests rely on a new document being loaded.
  // * Tests want to test this case specifically.
  // Callers should pass an accurate |reason| to make future triaging of
  // disabled tests easier.
  //
  // Note: It's preferable to make tests BackForwardCache compatible
  // when feasible, rather than using this method. Also please consider whether
  // you actually should have 2 tests, one with the document cached
  // (BackForwardCache enabled), and one without.
  void DisableForTesting(DisableForTestingReason reason);

  // Sets the number of documents that can be stored in the cache. This is meant
  // for use from within tests only.
  // If |cache_size_limit_for_testing| is 0 (the default), the normal cache
  // size limit will be used.
  void set_cache_size_limit_for_testing(size_t cache_size_limit_for_testing) {
    cache_size_limit_for_testing_ = cache_size_limit_for_testing;
  }

 private:
  // Destroys all evicted frames in the BackForwardCache.
  void DestroyEvictedFrames();

  // Contains the set of stored RenderFrameHost.
  // Invariant:
  // - Ordered from the most recently used to the last recently used.
  // - Once the list is full, the least recently used document is evicted.
  std::list<std::unique_ptr<RenderFrameHostImpl>> render_frame_hosts_;

  // Only used in tests. Whether the BackforwardCached has been disabled for
  // testing.
  bool is_disabled_for_testing_ = false;

  // Only used in tests. If non-zero, this value will be used as the cache size
  // limit.
  size_t cache_size_limit_for_testing_ = 0;

  // Only used for tests. This task runner is used for precise injection in
  // browser tests and for timing control.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing_;

  base::WeakPtrFactory<BackForwardCache> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_BACK_FORWARD_CACHE_H_
