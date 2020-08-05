// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/remote_decision_tree_predictor.h"

#include <string>

#include "base/containers/flat_set.h"

namespace optimization_guide {

RemoteDecisionTreePredictor::RemoteDecisionTreePredictor(
    const proto::PredictionModel& model) {
  // The Decision Tree model type is currently the only supported model type.
  DCHECK(model.model_info().supported_model_types(0) ==
         optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE);

  version_ = model.model_info().version();
  model_features_.reserve(
      model.model_info().supported_model_features_size() +
      model.model_info().supported_host_model_features_size());
  // Insert all the client model features for the owned |model_|.
  for (const auto& client_model_feature :
       model.model_info().supported_model_features()) {
    model_features_.emplace(
        proto::ClientModelFeature_Name(client_model_feature));
  }
  // Insert all the host model features for the owned |model_|.
  for (const auto& host_model_feature :
       model.model_info().supported_host_model_features()) {
    model_features_.emplace(host_model_feature);
  }
}

RemoteDecisionTreePredictor::~RemoteDecisionTreePredictor() = default;

machine_learning::mojom::DecisionTreePredictorProxy*
RemoteDecisionTreePredictor::Get() const {
  if (!remote_)
    return nullptr;

  return remote_.get();
}

bool RemoteDecisionTreePredictor::IsConnected() const {
  return remote_.is_connected();
}

void RemoteDecisionTreePredictor::FlushForTesting() {
  remote_.FlushForTesting();
}

mojo::PendingReceiver<machine_learning::mojom::DecisionTreePredictor>
RemoteDecisionTreePredictor::BindNewPipeAndPassReceiver() {
  return remote_.BindNewPipeAndPassReceiver();
}

const base::flat_set<std::string>& RemoteDecisionTreePredictor::model_features()
    const {
  return model_features_;
}

int64_t RemoteDecisionTreePredictor::version() const {
  return version_;
}

}  // namespace optimization_guide
