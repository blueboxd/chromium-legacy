// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_CONVERSATION_STARTERS_PARSER_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_CONVERSATION_STARTERS_PARSER_H_

#include <string>
#include <vector>

namespace ash {
class ConversationStarter;
}  // namespace ash

class ConversationStartersParser {
 public:
  // Parses a collection of conversation starters from the given safe JSON
  // response. In the event of a parse error, an empty collection is returned.
  static std::vector<ash::ConversationStarter> Parse(
      const std::string& safe_json_response);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_CONVERSATION_STARTERS_PARSER_H_
