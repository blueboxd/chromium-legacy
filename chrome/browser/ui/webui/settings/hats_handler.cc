// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/hats_handler.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace settings {

HatsHandler::HatsHandler() = default;

HatsHandler::~HatsHandler() = default;

void HatsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "tryShowHatsSurvey",
      base::BindRepeating(&HatsHandler::HandleTryShowHatsSurvey,
                          base::Unretained(this)));
}

void HatsHandler::HandleTryShowHatsSurvey(const base::ListValue* args) {
  const std::string& trigger = args->GetList()[0].GetString();
  const int timeout_ms = args->GetList()[1].GetInt();
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()), /* create_if_necessary = */ true);
  if (hats_service) {
    hats_service->LaunchDelayedSurveyForWebContents(
        trigger, web_ui()->GetWebContents(), timeout_ms);
  }
}

}  // namespace settings
