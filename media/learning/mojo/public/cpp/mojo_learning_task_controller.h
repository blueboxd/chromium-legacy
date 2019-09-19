// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_TASK_CONTROLLER_H_
#define MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_TASK_CONTROLLER_H_

#include <utility>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/mojo/public/mojom/learning_task_controller.mojom.h"

namespace media {
namespace learning {

// LearningTaskController implementation to forward to a remote impl via mojo.
class COMPONENT_EXPORT(MEDIA_LEARNING_MOJO) MojoLearningTaskController
    : public LearningTaskController {
 public:
  // |task| will be provided by GetLearningTask().  Hopefully, it matches
  // whatever |controller_ptr| uses.
  MojoLearningTaskController(const LearningTask& task,
                             mojom::LearningTaskControllerPtr controller_ptr);
  ~MojoLearningTaskController() override;

  // LearningTaskController
  void BeginObservation(base::UnguessableToken id,
                        const FeatureVector& features) override;
  void CompleteObservation(base::UnguessableToken id,
                           const ObservationCompletion& completion) override;
  void CancelObservation(base::UnguessableToken id) override;
  const LearningTask& GetLearningTask() override;

 private:
  LearningTask task_;
  mojom::LearningTaskControllerPtr controller_ptr_;

  DISALLOW_COPY_AND_ASSIGN(MojoLearningTaskController);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_TASK_CONTROLLER_H_
