// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_CONVERSATION_STARTERS_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_CONVERSATION_STARTERS_CLIENT_IMPL_H_

#include "ash/public/cpp/assistant/conversation_starters_client.h"

class Profile;

// The implementation of the conversation starters feature browser client.
class ConversationStartersClientImpl : public ash::ConversationStartersClient {
 public:
  explicit ConversationStartersClientImpl(Profile* profile);
  ConversationStartersClientImpl(ConversationStartersClientImpl& copy) = delete;
  ConversationStartersClientImpl& operator=(
      ConversationStartersClientImpl& assign) = delete;
  ~ConversationStartersClientImpl() override;

  // ash::ConversationStartersClient:
  void FetchConversationStarters(Callback callback) override;

 private:
  Profile* const profile_;
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_CONVERSATION_STARTERS_CLIENT_IMPL_H_
