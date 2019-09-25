// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_DECISION_TREE_PREDICTION_MODEL_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_DECISION_TREE_PREDICTION_MODEL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// A concrete PredictionModel capable of evaluating the decision tree model type
// supported by the optimization guide.
class DecisionTreePredictionModel : public PredictionModel {
 public:
  DecisionTreePredictionModel(
      std::unique_ptr<optimization_guide::proto::PredictionModel>
          prediction_model,
      const base::flat_set<std::string>& host_model_features);

  ~DecisionTreePredictionModel() override;

  // PredictionModel implementation:
  optimization_guide::OptimizationTargetDecision Predict(
      const base::flat_map<std::string, float>& model_features) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DecisionTreePredictionModel);
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_DECISION_TREE_PREDICTION_MODEL_H_
