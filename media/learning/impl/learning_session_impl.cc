// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_session_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "media/learning/impl/distribution_reporter.h"
#include "media/learning/impl/learning_task_controller_impl.h"

namespace media {
namespace learning {

// Allow multiple clients to own an LTC that points to the same underlying LTC.
// Since we don't own the LTC, we also keep track of in-flight observations and
// explicitly cancel them on destruction, since dropping an LTC implies that.
class WeakLearningTaskController : public LearningTaskController {
 public:
  WeakLearningTaskController(
      base::WeakPtr<LearningSessionImpl> weak_session,
      base::SequenceBound<LearningTaskController>* controller)
      : weak_session_(std::move(weak_session)), controller_(controller) {}

  ~WeakLearningTaskController() override {
    if (!weak_session_)
      return;

    // Cancel any outstanding observations.
    for (auto& id : outstanding_ids_) {
      controller_->Post(FROM_HERE, &LearningTaskController::CancelObservation,
                        id);
    }
  }

  void BeginObservation(base::UnguessableToken id,
                        const FeatureVector& features) override {
    if (!weak_session_)
      return;

    outstanding_ids_.insert(id);
    controller_->Post(FROM_HERE, &LearningTaskController::BeginObservation, id,
                      features);
  }

  void CompleteObservation(base::UnguessableToken id,
                           const ObservationCompletion& completion) override {
    if (!weak_session_)
      return;
    outstanding_ids_.erase(id);
    controller_->Post(FROM_HERE, &LearningTaskController::CompleteObservation,
                      id, completion);
  }

  void CancelObservation(base::UnguessableToken id) override {
    if (!weak_session_)
      return;
    outstanding_ids_.erase(id);
    controller_->Post(FROM_HERE, &LearningTaskController::CancelObservation,
                      id);
  }

  base::WeakPtr<LearningSessionImpl> weak_session_;
  base::SequenceBound<LearningTaskController>* controller_;

  // Set of ids that have been started but not completed / cancelled yet.
  std::set<base::UnguessableToken> outstanding_ids_;
};

LearningSessionImpl::LearningSessionImpl(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      controller_factory_(base::BindRepeating(
          [](scoped_refptr<base::SequencedTaskRunner> task_runner,
             const LearningTask& task,
             SequenceBoundFeatureProvider feature_provider)
              -> base::SequenceBound<LearningTaskController> {
            return base::SequenceBound<LearningTaskControllerImpl>(
                task_runner, task, DistributionReporter::Create(task),
                std::move(feature_provider));
          })) {}

LearningSessionImpl::~LearningSessionImpl() = default;

void LearningSessionImpl::SetTaskControllerFactoryCBForTesting(
    CreateTaskControllerCB cb) {
  controller_factory_ = std::move(cb);
}

std::unique_ptr<LearningTaskController> LearningSessionImpl::GetController(
    const std::string& task_name) {
  auto iter = task_map_.find(task_name);
  if (iter == task_map_.end())
    return nullptr;

  // If there were any way to replace / destroy a controller other than when we
  // destroy |this|, then this wouldn't be such a good idea.
  return std::make_unique<WeakLearningTaskController>(
      weak_factory_.GetWeakPtr(), &iter->second);
}

void LearningSessionImpl::RegisterTask(
    const LearningTask& task,
    SequenceBoundFeatureProvider feature_provider) {
  DCHECK(task_map_.count(task.name) == 0);
  task_map_.emplace(
      task.name,
      controller_factory_.Run(task_runner_, task, std::move(feature_provider)));
}

}  // namespace learning
}  // namespace media
