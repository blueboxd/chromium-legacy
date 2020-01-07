// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TAB_GROUPS_TAB_GROUP_ID_H_
#define COMPONENTS_TAB_GROUPS_TAB_GROUP_ID_H_

#include "base/component_export.h"
#include "base/token.h"

namespace tab_groups {

class COMPONENT_EXPORT(TAB_GROUPS) TabGroupId {
 public:
  static TabGroupId GenerateNew();

  // This should only called with |token| returned from a previous |token()|
  // call on a valid TabGroupId.
  static TabGroupId FromRawToken(base::Token token);

  TabGroupId(const TabGroupId& other);

  TabGroupId& operator=(const TabGroupId& other);

  bool operator==(const TabGroupId& other) const;
  bool operator!=(const TabGroupId& other) const;
  bool operator<(const TabGroupId& other) const;

  const base::Token& token() const { return token_; }

  std::string ToString() const;

 private:
  explicit TabGroupId(base::Token token);

  base::Token token_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_TAB_GROUPS_TAB_GROUP_ID_H_
