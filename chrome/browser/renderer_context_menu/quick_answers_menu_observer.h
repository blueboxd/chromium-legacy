// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_QUICK_ANSWERS_MENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_QUICK_ANSWERS_MENU_OBSERVER_H_

#include <memory>
#include <string>

#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"

class RenderViewContextMenuProxy;

// A class that implements the quick answers menu.
class QuickAnswersMenuObserver
    : public RenderViewContextMenuObserver,
      public chromeos::quick_answers::QuickAnswersClient::QuickAnswersDelegate {
 public:
  QuickAnswersMenuObserver(const QuickAnswersMenuObserver&) = delete;
  QuickAnswersMenuObserver& operator=(const QuickAnswersMenuObserver&) = delete;

  explicit QuickAnswersMenuObserver(RenderViewContextMenuProxy* proxy);
  ~QuickAnswersMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

  // QuickAnswersDelegate implementation.
  void OnQuickAnswerReceived(
      std::unique_ptr<chromeos::quick_answers::QuickAnswer> answer) override;
  void OnEligibilityChanged(bool eligible) override;

  void SetQuickAnswerClientForTesting(
      std::unique_ptr<chromeos::quick_answers::QuickAnswersClient>
          quick_answers_client);

 private:
  // The interface to add a context-menu item and update it.
  RenderViewContextMenuProxy* proxy_;

  std::unique_ptr<chromeos::quick_answers::QuickAnswersClient>
      quick_answers_client_;

  // Whether the feature is enabled and all eligibility criteria are met (
  // locale, consents, etc).
  bool is_eligible_ = false;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_QUICK_ANSWERS_MENU_OBSERVER_H_
