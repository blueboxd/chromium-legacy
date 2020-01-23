// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/quick_answers_menu_observer.h"

#include <utility>

#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/context_menu_params.h"

namespace {

using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswersClient;
using chromeos::quick_answers::QuickAnswersRequest;

// TODO(llin): Update the placeholder after finalizing on the design.
constexpr char kLoadingPlaceholder[] = "Loading...";
constexpr char kNoResult[] = "See result in Assistant";

}  // namespace

QuickAnswersMenuObserver::QuickAnswersMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {
  auto* assistant_state = ash::AssistantState::Get();
  if (assistant_state && proxy_ && proxy_->GetBrowserContext()) {
    auto* browser_context = proxy_->GetBrowserContext();
    if (browser_context->IsOffTheRecord())
      return;

    quick_answers_client_ = std::make_unique<QuickAnswersClient>(
        content::BrowserContext::GetDefaultStoragePartition(browser_context)
            ->GetURLLoaderFactoryForBrowserProcess()
            .get(),
        assistant_state, this);
  }
}

QuickAnswersMenuObserver::~QuickAnswersMenuObserver() = default;

void QuickAnswersMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  if (!is_eligible_ || !proxy_ || !quick_answers_client_)
    return;

  if (params.input_field_type ==
      blink::ContextMenuDataInputFieldType::kPassword)
    return;

  auto selected_text = base::UTF16ToUTF8(params.selection_text);
  if (selected_text.empty())
    return;

    // Add Quick Answer Menu item.
    // TODO(llin): Update the menu item after finalizing on the design.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  proxy_->AddMenuItemWithIcon(IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
                              params.selection_text, kAssistantIcon);
#else
  proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
                      params.selection_text);
#endif
  proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
                      base::UTF8ToUTF16(kLoadingPlaceholder));
  proxy_->AddSeparator();

  // Fetch Quick Answer.
  QuickAnswersRequest request;
  request.selected_text = selected_text;
  query_ = request.selected_text;
  quick_answers_client_->SendRequest(request);
}

bool QuickAnswersMenuObserver::IsCommandIdSupported(int command_id) {
  return (command_id == IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY ||
          command_id == IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER);
}

bool QuickAnswersMenuObserver::IsCommandIdChecked(int command_id) {
  return false;
}

bool QuickAnswersMenuObserver::IsCommandIdEnabled(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY;
}

void QuickAnswersMenuObserver::ExecuteCommand(int command_id) {
  if (command_id == IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY)
    SendAssistantQuery(query_);
}

void QuickAnswersMenuObserver::OnQuickAnswerReceived(
    std::unique_ptr<QuickAnswer> quick_answer) {
  if (quick_answer) {
    proxy_->UpdateMenuItem(
        IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
        /*enabled=*/false,
        /*hidden=*/false,
        /*title=*/
        base::UTF8ToUTF16(quick_answer->primary_answer.empty()
                              ? kNoResult
                              : quick_answer->primary_answer));

    if (!quick_answer->secondary_answer.empty()) {
      proxy_->UpdateMenuItem(
          IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
          /*enabled=*/true,
          /*hidden=*/false,
          /*title=*/base::UTF8ToUTF16(quick_answer->secondary_answer));
    }
  } else {
    proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
                           /*enabled=*/false,
                           /*hidden=*/false,
                           /*title=*/base::UTF8ToUTF16(kNoResult));
  }
}

void QuickAnswersMenuObserver::OnEligibilityChanged(bool eligible) {
  is_eligible_ = eligible;
}

void QuickAnswersMenuObserver::SetQuickAnswerClientForTesting(
    std::unique_ptr<chromeos::quick_answers::QuickAnswersClient>
        quick_answers_client) {
  quick_answers_client_ = std::move(quick_answers_client);
}

void QuickAnswersMenuObserver::SendAssistantQuery(const std::string& query) {
  mojo::Remote<chromeos::assistant::mojom::AssistantController>
      assistant_controller;
  ash::AssistantInterfaceBinder::GetInstance()->BindController(
      assistant_controller.BindNewPipeAndPassReceiver());
  assistant_controller->StartTextInteraction(
      query, /*allow_tts=*/false,
      chromeos::assistant::mojom::AssistantQuerySource::kQuickAnswers);
}
