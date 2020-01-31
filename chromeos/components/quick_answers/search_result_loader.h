// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_LOADER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_LOADER_H_

#include <memory>
#include <string>

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
  // A delegate interface for the SearchResultLoader.
  class SearchResultLoaderDelegate {
   public:
    SearchResultLoaderDelegate(const SearchResultLoaderDelegate&) = delete;
    SearchResultLoaderDelegate& operator=(const SearchResultLoaderDelegate&) =
        delete;

    // Invoked when there is a network error.
    virtual void OnNetworkError() {}

    // Invoked when the |quick_answer| is received. Note that |quick_answer| may
    // be |nullptr| if no answer found for the selected content.
    virtual void OnQuickAnswerReceived(
        std::unique_ptr<QuickAnswer> quick_answer) {}

   protected:
    SearchResultLoaderDelegate() = default;
    virtual ~SearchResultLoaderDelegate() = default;
  };

  SearchResultLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                     SearchResultLoaderDelegate* delegate);
  ~SearchResultLoader();

  SearchResultLoader(const SearchResultLoader&) = delete;
  SearchResultLoader& operator=(const SearchResultLoader&) = delete;

  // Starts downloading of |quick_answers| associated with |selected_text|,
  // calling |SearchResultLoaderDelegate| methods when finished.
  // Note that delegate methods should be called only once per
  // SearchResultLoader instance.
  void Fetch(const std::string& selected_text);

 private:
  void OnSimpleURLLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnResultParserComplete(std::unique_ptr<QuickAnswer> quick_answer);

  std::unique_ptr<SearchResponseParser> search_response_parser_;
  network::mojom::URLLoaderFactory* network_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  SearchResultLoaderDelegate* const delegate_;

  // Time when the query is issued.
  base::TimeTicks fetch_start_time_;
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_LOADER_H_
