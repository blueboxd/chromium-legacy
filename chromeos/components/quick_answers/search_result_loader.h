// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_LOADER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_LOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "chromeos/components/quick_answers/search_result_parsers/search_response_parser.h"

namespace network {
class SimpleURLLoader;

namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace chromeos {
namespace quick_answers {

struct QuickAnswer;

class SearchResultLoader {
 public:
  // Callback used when downloading of |quick_answer| is complete.
  // Note that |proactive_suggestions| may be |nullptr|.
  using CompleteCallback =
      base::OnceCallback<void(std::unique_ptr<QuickAnswer> quick_answer)>;

  SearchResultLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                     CompleteCallback complete_callback);
  ~SearchResultLoader();

  SearchResultLoader(const SearchResultLoader&) = delete;
  SearchResultLoader& operator=(const SearchResultLoader&) = delete;

  // Starts downloading of |proactive_suggestions| associated with |url_|,
  // running |complete_callback| when finished. Note that this method should be
  // called only once per ProactiveSuggestionsLoader instance.
  void Fetch(const std::string& selected_text);

 private:
  void OnSimpleURLLoaderComplete(std::unique_ptr<std::string> response_body);

  std::unique_ptr<SearchResponseParser> search_response_parser_;
  network::mojom::URLLoaderFactory* network_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  CompleteCallback complete_callback_;
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_LOADER_H_
