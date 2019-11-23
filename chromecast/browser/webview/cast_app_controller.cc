// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/cast_app_controller.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {

CastAppController::CastAppController(Client* client,
                                     content::WebContents* contents)
    : WebContentController(client), contents_(contents) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  auto ax_id = contents->GetMainFrame()->GetAXTreeID().ToString();
  response->mutable_create_response()
      ->mutable_accessibility_info()
      ->set_ax_tree_id(ax_id);

  client->EnqueueSend(std::move(response));
}

CastAppController::~CastAppController() {}

void CastAppController::Destroy() {
  client_ = nullptr;
  delete this;
}

content::WebContents* CastAppController::GetWebContents() {
  return contents_;
}

}  // namespace chromecast
