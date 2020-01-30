// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FEATURE_LIST_CREATOR_H_
#define WEBLAYER_BROWSER_FEATURE_LIST_CREATOR_H_

#include <memory>

#include "components/prefs/pref_service.h"

namespace weblayer {

// TODO(weblayer-dev): // Set up field trials based on the stored variations
// seed data.
class FeatureListCreator {
 public:
  FeatureListCreator();
  ~FeatureListCreator();

  void CreateLocalState();

  // Passes ownership of the |local_state_| to the caller.
  std::unique_ptr<PrefService> TakePrefService() {
    DCHECK(local_state_);
    return std::move(local_state_);
  }

 private:
  std::unique_ptr<PrefService> local_state_;

  DISALLOW_COPY_AND_ASSIGN(FeatureListCreator);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FEATURE_LIST_CREATOR_H_
