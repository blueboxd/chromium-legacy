// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONVERSATION_STARTER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONVERSATION_STARTER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace ash {

// Models an immutable conversation starter.
class ASH_PUBLIC_EXPORT ConversationStarter {
 public:
  // Enumeration of possible permissions which a conversation starter may
  // require in order to be presented to the user.
  enum class Permission : uint32_t { kRelatedInfo = 1u };

  ConversationStarter(const std::string& label,
                      const base::Optional<GURL>& action_url,
                      const base::Optional<GURL>& icon_url,
                      uint32_t required_permissions);
  ConversationStarter(const ConversationStarter& copy) = delete;
  ConversationStarter& operator=(ConversationStarter& assign) = delete;
  ~ConversationStarter();

  const std::string& label() const { return label_; }
  const base::Optional<GURL>& action_url() const { return action_url_; }
  const base::Optional<GURL>& icon_url() const { return icon_url_; }
  const uint32_t& required_permissions() const { return required_permissions_; }

 private:
  const std::string label_;
  const base::Optional<GURL> action_url_;
  const base::Optional<GURL> icon_url_;
  const uint32_t required_permissions_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONVERSATION_STARTER_H_
