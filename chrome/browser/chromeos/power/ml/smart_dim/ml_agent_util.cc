// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/smart_dim/ml_agent_util.h"

#include "base/optional.h"

namespace chromeos {
namespace power {
namespace ml {

namespace {

// Extracts node names and id, then inserts into the name_2_node_map.
bool PopulateMapFromNamesAndNodes(
    const base::Value& names,
    const base::Value& nodes,
    base::flat_map<std::string, int>* name_2_node_map) {
  if (names.GetList().size() != nodes.GetList().size()) {
    DVLOG(1) << "names and nodes don't match";
    return false;
  }

  for (size_t i = 0; i < names.GetList().size(); i++) {
    if (!names.GetList()[i].is_string() || !nodes.GetList()[i].is_int()) {
      DVLOG(1) << i << " names and nodes have unexpected type";
      return false;
    }
    name_2_node_map->emplace(names.GetList()[i].GetString(),
                             nodes.GetList()[i].GetInt());
  }
  return true;
}

}  // namespace

bool ParseMetaInfoFromJsonObject(const base::Value& root,
                                 std::string* metrics_model_name,
                                 double* dim_threshold,
                                 size_t* expected_feature_size,
                                 base::flat_map<std::string, int>* inputs,
                                 base::flat_map<std::string, int>* outputs) {
  DCHECK(metrics_model_name && dim_threshold && expected_feature_size &&
         inputs && outputs);

  const std::string* metrics_model_name_value =
      root.FindStringKey("metrics_model_name");
  const base::Optional<double> dim_threshold_value =
      root.FindDoubleKey("threshold");
  const base::Optional<int> expected_feature_size_value =
      root.FindIntKey("expected_feature_size");

  if (!metrics_model_name_value || *metrics_model_name_value == "" ||
      dim_threshold_value == base::nullopt ||
      expected_feature_size_value == base::nullopt) {
    DVLOG(1) << "metadata_json missing expected field(s).";
    return false;
  }

  *metrics_model_name = *metrics_model_name_value;
  *dim_threshold = dim_threshold_value.value();
  *expected_feature_size =
      static_cast<size_t>(expected_feature_size_value.value());

  const base::Value* input_names = root.FindListKey("input_names");
  const base::Value* input_nodes = root.FindListKey("input_nodes");
  const base::Value* output_names = root.FindListKey("output_names");
  const base::Value* output_nodes = root.FindListKey("output_nodes");

  if (!input_names || !input_nodes || !output_names || !output_nodes ||
      !PopulateMapFromNamesAndNodes(*input_names, *input_nodes, inputs) ||
      !PopulateMapFromNamesAndNodes(*output_names, *output_nodes, outputs)) {
    DVLOG(1) << "Failed to load inputs and outputs maps from metadata_json";
    *metrics_model_name = "";
    *dim_threshold = 0.0;
    *expected_feature_size = 0;
    inputs->clear();
    outputs->clear();
    return false;
  }

  return true;
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
