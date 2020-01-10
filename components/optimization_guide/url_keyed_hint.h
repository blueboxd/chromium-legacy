// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_URL_KEYED_HINT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_URL_KEYED_HINT_H_

#include "base/time/time.h"
#include "components/optimization_guide/proto/hints.pb.h"

// Holds a hint keyed by URL and its expiration time, used to maintain URL-keyed
// hints in memory.
class URLKeyedHint {
 public:
  URLKeyedHint(const base::Time expiration_time,
               optimization_guide::proto::Hint&& url_keyed_hint);
  ~URLKeyedHint();

  const base::Time expiration_time() const { return expiration_time_; }
  optimization_guide::proto::Hint* url_keyed_hint() const {
    return url_keyed_hint_.get();
  }

 private:
  const base::Time expiration_time_;
  std::unique_ptr<optimization_guide::proto::Hint> url_keyed_hint_;

  DISALLOW_COPY_AND_ASSIGN(URLKeyedHint);
};

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_URL_KEYED_HINT_H_