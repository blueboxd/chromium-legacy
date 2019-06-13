// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_TOP_HOST_PROVIDER_IMPL_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_TOP_HOST_PROVIDER_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/previews/content/previews_top_host_provider.h"

class PrefService;

namespace content {
class BrowserContext;
class NavigationHandle;
}
namespace previews {

// An implementation of the PreviewTopHostProvider for getting the top sites
// based on site engagement scores.
class PreviewsTopHostProviderImpl : public PreviewsTopHostProvider {
 public:
  explicit PreviewsTopHostProviderImpl(content::BrowserContext* BrowserContext);
  ~PreviewsTopHostProviderImpl() override;

  // Update the HintsFetcherTopHostBlacklist by attempting to remove the host
  // for the current navigation from the blacklist. A host is removed if it is
  // currently on the blacklist and the blacklist state is updated if the
  // blacklist is empty after removing a host.
  static void MaybeUpdateTopHostBlacklist(
      content::NavigationHandle* navigation_handle);

  std::vector<std::string> GetTopHosts(size_t max_sites) override;

 private:
  // Initializes the HintsFetcherTopHostBlacklist with all the hosts in the site
  // engagement service and transitions the blacklist state from kNotInitialized
  // to kInitialized.
  void InitializeHintsFetcherTopHostBlacklist();

  // Return the current state of the HintsFetcherTopHostBlacklist held in the
  // |kHintsFetcherTopHostBlacklistState| pref.
  optimization_guide::prefs::HintsFetcherTopHostBlacklistState
  GetCurrentBlacklistState() const;

  // Transition the current HintsFetcherTopHostBlacklist state to |state| and
  // validate the transition. The updated state is persisted in the
  // |kHintsFetcherTopHostBlacklistState| pref.
  void UpdateCurrentBlacklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState state);

  // |browser_context_| is used for interaction with the SiteEngagementService
  // and the embedder should guarantee that it is non-null during the lifetime
  // of |this|.
  content::BrowserContext* browser_context_;

  // |pref_service_| provides information about the current profile's
  // settings. It is not owned and guaranteed to outlive |this|.
  PrefService* pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsTopHostProviderImpl);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_TOP_HOST_PROVIDER_IMPL_H_
