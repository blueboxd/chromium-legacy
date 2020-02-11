// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_MANAGER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/context.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "url/gurl.h"

namespace domain_reliability {

class DOMAIN_RELIABILITY_EXPORT DomainReliabilityContextManager {
 public:
  DomainReliabilityContextManager(
      DomainReliabilityContext::Factory* context_factory);
  ~DomainReliabilityContextManager();

  // If |url| maps to a context added to this manager, calls |OnBeacon| on
  // that context with |beacon|. Otherwise, does nothing.
  void RouteBeacon(std::unique_ptr<DomainReliabilityBeacon> beacon);

  // Calls |ClearBeacons| on all contexts matched by |origin_filter| added
  // to this manager, but leaves the contexts themselves intact. A null
  // |origin_filter| is interpreted as an always-true filter, indicating
  // complete deletion.
  void ClearBeacons(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);

  // TODO(juliatuttle): Once unit tests test ContextManager directly, they can
  // use a custom Context::Factory to get the created Context, and this can be
  // void.
  DomainReliabilityContext* AddContextForConfig(
      std::unique_ptr<const DomainReliabilityConfig> config);

  // Removes all contexts matched by |origin_filter| from this manager
  // (discarding all queued beacons in the process). A null |origin_filter|
  // is interpreted as an always-true filter, indicating complete deletion.
  void RemoveContexts(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);

  std::unique_ptr<base::Value> GetWebUIData() const;

  size_t contexts_size_for_testing() const { return contexts_.size(); }

 private:
  typedef std::map<std::string, DomainReliabilityContext*> ContextMap;

  DomainReliabilityContext* GetContextForHost(const std::string& host);

  DomainReliabilityContext::Factory* context_factory_;
  // Owns DomainReliabilityContexts.
  ContextMap contexts_;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityContextManager);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_MANAGER_H_
