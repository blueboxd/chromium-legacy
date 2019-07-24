// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_UTIL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace base {
class Value;
}

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace app_list {

// Returns a new, configured instance of the predictor defined in |config|.
std::unique_ptr<RecurrencePredictor> MakePredictor(
    const RecurrencePredictorConfigProto& config,
    const std::string& model_identifier);

// A utility class for converting a JSON configuration for a RecurrenceRanker
// into a RecurrenceRankerConfigProto that can be used to construct the ranker.
// JSON parsing is performed safely on another thread.
//
// The JSON configuration format is similar, but not identical, to the
// RecurrenceRankerConfigProto schema. The unit tests are the best
// specification of the format.
class JsonConfigConverter {
 public:
  using OnConfigLoadedCallback =
      base::OnceCallback<void(base::Optional<RecurrenceRankerConfigProto>)>;

  // The constructor requires a connector to interact with the parsing service.
  // For most use cases, this can be content::GetSystemConnector().
  explicit JsonConfigConverter(service_manager::Connector* connector);
  ~JsonConfigConverter();

  // Performs a conversion. The provided |callback| will be called with the
  // resulting proto if the conversion succeeded, or base::nullopt if the
  // parsing or conversion failed. |model_identifier| is used for metrics
  // reporting in the same way as the RecurrenceRanker's |model_identifier|
  // parameter.
  void Convert(const std::string& json_string,
               const std::string& model_identifier,
               OnConfigLoadedCallback callback);

 private:
  // Create or reuse a connection to the data decoder service for safe JSON
  // parsing.
  data_decoder::mojom::JsonParser& GetJsonParser();

  // Callback for parser.
  void OnJsonParsed(OnConfigLoadedCallback callback,
                    const std::string& model_identifier,
                    const base::Optional<base::Value> json_data,
                    const base::Optional<std::string>& error);

  std::string model_identifier_;

  service_manager::Connector* connector_;
  data_decoder::mojom::JsonParserPtr json_parser_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_UTIL_H_
