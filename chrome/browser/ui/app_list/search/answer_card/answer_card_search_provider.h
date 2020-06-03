// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_SEARCH_PROVIDER_H_

#include <memory>
#include <string>

#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class Profile;
class TemplateURLService;

namespace app_list {

// Search provider for the answer card.
class AnswerCardSearchProvider : public SearchProvider {
 public:
  AnswerCardSearchProvider(Profile* profile,
                           AppListModelUpdater* model_updater,
                           AppListControllerDelegate* list_controller);

  ~AnswerCardSearchProvider() override;

  // SearchProvider overrides:
  void Start(const base::string16& query) override;
  ash::AppListSearchResultType ResultType() override;

 private:
  void UpdateQuery(const base::string16& query);

  // Unowned pointer to the associated profile.
  Profile* const profile_;

  // Unowned pointer to app list model updater.
  AppListModelUpdater* const model_updater_;

  // Unowned pointer to app list controller.
  AppListControllerDelegate* const list_controller_;

  // If valid, URL of the answer server. Otherwise, search answers are disabled.
  const GURL answer_server_url_;

  // URL of the current answer server query.
  GURL current_potential_answer_card_url_;

  // Unowned pointer to template URL service.
  TemplateURLService* const template_url_service_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_SEARCH_PROVIDER_H_
