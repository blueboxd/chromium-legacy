// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEX_HELPER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEX_HELPER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace extensions {
namespace declarative_net_request {

// A class to help in indexing multiple rulesets.
class IndexHelper : public base::RefCountedThreadSafe<IndexHelper> {
 public:
  // Starts indexing rulesets. Must be called on a sequence which supports file
  // IO. The |callback| will be dispatched to the same sequence on which Start()
  // is called.
  using Results = std::vector<IndexAndPersistJSONRulesetResult>;
  using IndexCallback = base::OnceCallback<void(Results)>;
  static void Start(std::vector<RulesetSource> sources, IndexCallback callback);

 private:
  friend class base::RefCountedThreadSafe<IndexHelper>;

  IndexHelper(std::vector<RulesetSource> sources, IndexCallback callback);
  ~IndexHelper();

  // Starts indexing the rulesets.
  void Start();

  // Callback invoked when indexing of all rulesets is completed.
  void OnAllRulesetsIndexed();

  // Callback invoked when indexing of a single ruleset is completed.
  void OnRulesetIndexed(base::OnceClosure ruleset_done_closure,
                        IndexAndPersistJSONRulesetResult result);

  std::vector<RulesetSource> sources_;
  IndexCallback callback_;
  Results results_;

  // We use a single shared Data Decoder service instance to process all of the
  // rulesets for this IndexHelper.
  data_decoder::DataDecoder decoder_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEX_HELPER_H_
