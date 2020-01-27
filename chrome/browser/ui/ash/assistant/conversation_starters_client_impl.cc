// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/conversation_starters_client_impl.h"

#include <vector>

#include "ash/public/cpp/assistant/conversation_starter.h"
#include "chrome/browser/profiles/profile.h"

ConversationStartersClientImpl::ConversationStartersClientImpl(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

ConversationStartersClientImpl::~ConversationStartersClientImpl() = default;

// TODO(dmblack): Fetch conversation starters from the server.
void ConversationStartersClientImpl::FetchConversationStarters(
    Callback callback) {
  std::move(callback).Run(std::vector<ash::ConversationStarter>());
}
