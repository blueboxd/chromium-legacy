// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_EXPERIMENT_HELPER_H_
#define MEDIA_LEARNING_COMMON_EXPERIMENT_HELPER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/feature_dictionary.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/common/learning_task_controller.h"

namespace media {
namespace learning {

// Helper for adding a learning experiment to existing code.
class COMPONENT_EXPORT(LEARNING_COMMON) ExperimentHelper {
 public:
  // If |controller| is null, then everything else no-ops.
  ExperimentHelper(std::unique_ptr<LearningTaskController> controller);

  // Cancels any existing observation.
  ~ExperimentHelper();

  // Start a new observation.  Any existing observation is cancelled.  Does
  // nothing if there's no controller.
  void BeginObservation(const FeatureDictionary& dictionary);

  // Complete any pending observation.  Does nothing if none is in progress.
  void CompleteObservationIfNeeded(const TargetValue& target);

  // Cancel any pending observation.
  void CancelObservationIfNeeded();

 private:
  // May be null.
  std::unique_ptr<LearningTaskController> controller_;

  // May be null if no observation is in flight.  Must be null if |controller_|
  // is null.
  base::UnguessableToken observation_id_;

  DISALLOW_COPY_AND_ASSIGN(ExperimentHelper);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_EXPERIMENT_HELPER_H_
