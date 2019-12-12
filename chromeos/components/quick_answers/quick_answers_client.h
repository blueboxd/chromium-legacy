// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
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
class QuickAnswersClient : public ash::AssistantStateObserver {
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

    // Invoked when feature eligibility changed.
    virtual void OnEligibilityChanged(bool eligible) {}

   protected:
    QuickAnswersDelegate() = default;
    virtual ~QuickAnswersDelegate() = default;
  };

  QuickAnswersClient(network::mojom::URLLoaderFactory* url_loader_factory,
                     ash::AssistantState* assistant_state,
                     QuickAnswersDelegate* delegate);
  ~QuickAnswersClient() override;

  QuickAnswersClient(const QuickAnswersClient&) = delete;
  QuickAnswersClient& operator=(const QuickAnswersClient&) = delete;

  // AssistantStateObserver:
  void OnAssistantFeatureAllowedChanged(
      ash::mojom::AssistantAllowedState state) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantContextEnabled(bool enabled) override;
  void OnLocaleChanged(const std::string& locale) override;

  // Send a quick answer request.
  void SendRequest(const QuickAnswersRequest& quick_answers_request);

 private:
  void OnQuickAnswerReceived(std::unique_ptr<QuickAnswer> quick_answer);
  void NotifyEligibilityChanged();

  network::mojom::URLLoaderFactory* url_loader_factory_ = nullptr;
  ash::AssistantState* assistant_state_ = nullptr;
  QuickAnswersDelegate* delegate_ = nullptr;
  std::unique_ptr<SearchResultLoader> search_results_loader_;
  bool assistant_enabled_ = false;
  bool assistant_context_enabled_ = false;
  bool locale_supported_ = false;
  ash::mojom::AssistantAllowedState assistant_allowed_state_ =
      ash::mojom::AssistantAllowedState::ALLOWED;
  bool is_eligible_ = false;
};

}  // namespace quick_answers
}  // namespace chromeos
#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_CLIENT_H_
