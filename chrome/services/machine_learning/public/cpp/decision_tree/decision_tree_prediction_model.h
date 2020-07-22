// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_DECISION_TREE_DECISION_TREE_PREDICTION_MODEL_H_
#define CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_DECISION_TREE_DECISION_TREE_PREDICTION_MODEL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/services/machine_learning/public/cpp/decision_tree/prediction_model.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace machine_learning {
namespace decision_tree {

// A concrete PredictionModel capable of evaluating the decision tree model type
// supported by the optimization guide.
class DecisionTreePredictionModel : public PredictionModel {
 public:
  explicit DecisionTreePredictionModel(
      std::unique_ptr<optimization_guide::proto::PredictionModel>
          prediction_model);

  ~DecisionTreePredictionModel() override;

  DecisionTreePredictionModel(const DecisionTreePredictionModel&) = delete;
  DecisionTreePredictionModel& operator=(const DecisionTreePredictionModel&) =
      delete;

  // PredictionModel implementation:
  mojom::DecisionTreePredictionResult Predict(
      const base::flat_map<std::string, float>& model_features,
      double* prediction_score) override;

 private:
  // Evaluates the provided model, either an ensemble or decision tree model,
  // with the |model_features| and stores the output in |result|. Returns false
  // if evaluation fails.
  bool EvaluateModel(const optimization_guide::proto::Model& model,
                     const base::flat_map<std::string, float>& model_features,
                     double* result);

  // Evaluates the decision tree model with the |model_features| and
  // stores the output in |result|. Returns false if evaluation fails.
  bool EvaluateDecisionTree(
      const optimization_guide::proto::DecisionTree& tree,
      const base::flat_map<std::string, float>& model_features,
      double* result);

  // Evaluates an ensemble model with the |model_features| and
  // stores the output in |result|. Returns false if evaluation fails.
  bool EvaluateEnsembleModel(
      const optimization_guide::proto::Ensemble& ensemble,
      const base::flat_map<std::string, float>& model_features,
      double* result);

  // Performs a depth first traversal the |tree| based on |model_features|
  // and stores the value of the leaf in |result|. Returns false if the
  // traversal or node evaluation fails.
  bool TraverseTree(const optimization_guide::proto::DecisionTree& tree,
                    const optimization_guide::proto::TreeNode& node,
                    const base::flat_map<std::string, float>& model_features,
                    double* result);

  // PredictionModel implementation:
  bool ValidatePredictionModel() const override;

  // Validates a model or submodel of an ensemble. Returns
  // false if the model is invalid.
  bool ValidateModel(const optimization_guide::proto::Model& model) const;

  // Validates an ensemble model. Returns false if the ensemble
  // if invalid.
  bool ValidateEnsembleModel(
      const optimization_guide::proto::Ensemble& ensemble) const;

  // Validates a decision tree model. Returns false if the
  // decision tree model is invalid.
  bool ValidateDecisionTree(
      const optimization_guide::proto::DecisionTree& tree) const;

  // Validates a leaf. Returns false if the leaf is invalid.
  bool ValidateLeaf(const optimization_guide::proto::Leaf& leaf) const;

  // Validates an inequality test. Returns false if the
  // inequality test is invalid.
  bool ValidateInequalityTest(
      const optimization_guide::proto::InequalityTest& inequality_test) const;

  // Validates each node of a decision tree by traversing every
  // node of the |tree|. Returns false if any part of the tree is invalid.
  bool ValidateTreeNode(const optimization_guide::proto::DecisionTree& tree,
                        const optimization_guide::proto::TreeNode& node,
                        const int& node_index) const;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace decision_tree
}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_DECISION_TREE_DECISION_TREE_PREDICTION_MODEL_H_
