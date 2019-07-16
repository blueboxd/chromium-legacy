// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_TOP_HOST_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_TOP_HOST_PROVIDER_H_

#include <string>
#include <vector>

namespace optimization_guide {

// A class to handle querying for the top hosts for a user.
class TopHostProvider {
 public:
  // Returns a vector of at most |max_sites| top hosts, the order of hosts is
  // not guaranteed.
  virtual std::vector<std::string> GetTopHosts(size_t max_sites) = 0;

 protected:
  TopHostProvider() {}
  virtual ~TopHostProvider() {}
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_TOP_HOST_PROVIDER_H_
