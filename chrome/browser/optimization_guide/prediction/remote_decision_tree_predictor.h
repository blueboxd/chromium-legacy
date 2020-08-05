// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_REMOTE_DECISION_TREE_PREDICTOR_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_REMOTE_DECISION_TREE_PREDICTOR_H_

#include <string>

#include "base/containers/flat_set.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace optimization_guide {

// Holds a Mojo remote handle connected to a DecisionTreePredictor instance in
// the ML Service, together with model information necessary for building
// feature maps.
class RemoteDecisionTreePredictor {
 public:
  // Initializes an unbound remote handle and saves necessary information from
  // |model|. |model| must be a valid model.
  explicit RemoteDecisionTreePredictor(const proto::PredictionModel& model);
  ~RemoteDecisionTreePredictor();

  RemoteDecisionTreePredictor(const RemoteDecisionTreePredictor&) = delete;
  RemoteDecisionTreePredictor& operator=(const RemoteDecisionTreePredictor&) =
      delete;

  // Exposes access to callable interface methods directed at the |remote_|'s
  // receiver. Returns nullptr if |remote_| is unbound.
  machine_learning::mojom::DecisionTreePredictorProxy* Get() const;

  // Whether |remote_| is connected.
  bool IsConnected() const;

  // Flushes |remote_| for testing purpose.
  void FlushForTesting();

  // Calls the |BindNewPipeAndPassReceiver| method of the |remote_|. Must only
  // be called on a bound |remote_|.
  mojo::PendingReceiver<machine_learning::mojom::DecisionTreePredictor>
  BindNewPipeAndPassReceiver();

  // A set of model features bound to the predictor handle.
  const base::flat_set<std::string>& model_features() const;

  // Version of the model bound to the predictor handle.
  int64_t version() const;

 private:
  mojo::Remote<machine_learning::mojom::DecisionTreePredictor> remote_;
  base::flat_set<std::string> model_features_;
  int64_t version_;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_REMOTE_DECISION_TREE_PREDICTOR_H_
