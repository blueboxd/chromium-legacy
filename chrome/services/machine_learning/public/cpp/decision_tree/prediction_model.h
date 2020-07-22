// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_DECISION_TREE_PREDICTION_MODEL_H_
#define CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_DECISION_TREE_PREDICTION_MODEL_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace machine_learning {
namespace decision_tree {

// A PredictionModel supported by the optimization guide that makes an
// OptimizationTargetDecision by evaluating a prediction model.
class PredictionModel {
 public:
  virtual ~PredictionModel();

  PredictionModel(const PredictionModel&) = delete;
  PredictionModel& operator=(const PredictionModel&) = delete;

  // Creates an Prediction model of the correct ModelType specified in
  // |prediction_model|. The validation overhead of this factory can be high and
  // should should be called in the background.
  static std::unique_ptr<PredictionModel> Create(
      std::unique_ptr<optimization_guide::proto::PredictionModel>
          prediction_model);

  // Returns the DecisionTreePredictionResult by evaluating the |model_|
  // using the provided |model_features|. |prediction_score| will be populated
  // with the score output by the model.
  virtual mojom::DecisionTreePredictionResult Predict(
      const base::flat_map<std::string, float>& model_features,
      double* prediction_score) = 0;

  // Provide the version of the |model_| by |this|.
  int64_t GetVersion() const;

  // Provide the model features required for evaluation of the |model_| by
  // |this|.
  base::flat_set<std::string> GetModelFeatures() const;

 protected:
  explicit PredictionModel(
      std::unique_ptr<optimization_guide::proto::PredictionModel>
          prediction_model);

  // The in-memory model used for prediction.
  std::unique_ptr<optimization_guide::proto::Model> model_;

 private:
  // Determines if the |model_| is complete and can be successfully evaluated by
  // |this|.
  virtual bool ValidatePredictionModel() const = 0;

  // The information that describes the |model_|
  std::unique_ptr<optimization_guide::proto::ModelInfo> model_info_;

  // The set of features required by the |model_| to be evaluated.
  base::flat_set<std::string> model_features_;

  // The version of the |model_|.
  int64_t version_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace decision_tree
}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_DECISION_TREE_PREDICTION_MODEL_H_
