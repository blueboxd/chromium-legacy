// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/mojo/public/cpp/mojo_learning_task_controller.h"

#include <utility>

#include "mojo/public/cpp/bindings/binding.h"

namespace media {
namespace learning {

MojoLearningTaskController::MojoLearningTaskController(
    const LearningTask& task,
    mojom::LearningTaskControllerPtr controller_ptr)
    : task_(task), controller_ptr_(std::move(controller_ptr)) {}

MojoLearningTaskController::~MojoLearningTaskController() = default;

void MojoLearningTaskController::BeginObservation(
    base::UnguessableToken id,
    const FeatureVector& features) {
  // We don't need to keep track of in-flight observations, since the service
  // side handles it for us.
  controller_ptr_->BeginObservation(id, features);
}

void MojoLearningTaskController::CompleteObservation(
    base::UnguessableToken id,
    const ObservationCompletion& completion) {
  controller_ptr_->CompleteObservation(id, completion);
}

void MojoLearningTaskController::CancelObservation(base::UnguessableToken id) {
  controller_ptr_->CancelObservation(id);
}

const LearningTask& MojoLearningTaskController::GetLearningTask() {
  return task_;
}

}  // namespace learning
}  // namespace media
