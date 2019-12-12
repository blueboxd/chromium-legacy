// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_

#include <memory>
#include <string>

#include "chromeos/components/quick_answers/search_result_loader.h"

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace chromeos {
namespace quick_answers {

struct QuickAnswer;
struct QuickAnswersRequest;

// Quick answers client to load and parse quick answer results.
class QuickAnswersClient {
 public:
  // A delegate interface for the QuickAnswersClient.
  class QuickAnswersDelegate {
   public:
    QuickAnswersDelegate(const QuickAnswersDelegate&) = delete;
    QuickAnswersDelegate& operator=(const QuickAnswersDelegate&) = delete;

    // Invoked when the |quick_answer| is received. Note that |quick_answer| may
    // be |nullptr| if no answer found for the selected content.
    virtual void OnQuickAnswerReceived(
        std::unique_ptr<QuickAnswer> quick_answer) {}

    // Invoked when the query is rewritten.
    virtual void OnRequestPreprocessFinish(
        const QuickAnswersRequest& processed_request) {}

   protected:
    QuickAnswersDelegate() = default;
    virtual ~QuickAnswersDelegate() = default;
  };

  QuickAnswersClient(network::mojom::URLLoaderFactory* url_loader_factory,
                     QuickAnswersDelegate* delegate);
  ~QuickAnswersClient();

  QuickAnswersClient(const QuickAnswersClient&) = delete;
  QuickAnswersClient& operator=(const QuickAnswersClient&) = delete;

  // Send a quick answer request.
  void SendRequest(const QuickAnswersRequest& quick_answers_request);

 private:
  void OnQuickAnswerReceived(std::unique_ptr<QuickAnswer> quick_answer);

  network::mojom::URLLoaderFactory* url_loader_factory_ = nullptr;
  QuickAnswersDelegate* delegate_ = nullptr;
  std::unique_ptr<SearchResultLoader> search_results_loader_;
};

}  // namespace quick_answers
}  // namespace chromeos
#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_
