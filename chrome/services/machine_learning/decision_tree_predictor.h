// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_DECISION_TREE_PREDICTOR_H_
#define CHROME_SERVICES_MACHINE_LEARNING_DECISION_TREE_PREDICTOR_H_

#include <memory>
#include <string>

#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"

namespace machine_learning {

class DecisionTreeModel;

// Holds a decision tree model instance and provides the interface for model
// prediction. This class takes care of the deserialization and validation
// of the decision tree model.
class DecisionTreePredictor : public mojom::DecisionTreePredictor {
 public:
  // Creates a predictor instance from a DecisionTreeModel. |model| can be null
  // which indicates failed deserialization or validation, resulting in an
  // invalid model.
  explicit DecisionTreePredictor(std::unique_ptr<DecisionTreeModel> model);

  // Creates a predictor instance from a model spec. This will take ownership of
  // |spec| and consume it.
  static std::unique_ptr<DecisionTreePredictor> FromModelSpec(
      mojom::DecisionTreeModelSpecPtr spec);

  ~DecisionTreePredictor() override;

  DecisionTreePredictor(const DecisionTreePredictor&) = delete;
  DecisionTreePredictor& operator=(const DecisionTreePredictor&) = delete;

  // Whether the predictor instance holds a valid model. The ML Service will
  // check this and will not bind the predictor instance to any Mojo receiver if
  // it is not valid.
  bool IsValid() const;

 private:
  // |mojom::DecisionTreePredictor::Predict| implementation.
  void Predict(const base::flat_map<std::string, float>& model_features,
               PredictCallback callback) override;

  // Decision Tree model instance.
  std::unique_ptr<DecisionTreeModel> model_;
};

}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_DECISION_TREE_PREDICTOR_H_
