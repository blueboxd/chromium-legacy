// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_OBSERVER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ash {

struct ProactiveSuggestions;

// An observer which receives notification of events pertaining to the
// ProactiveSuggestionsClient in the browser.
class ASH_PUBLIC_EXPORT ProactiveSuggestionsClientObserver
    : public base::CheckedObserver {
 public:
  // Invoked when the ProactiveSuggestionsClient is being destroyed so as to
  // give observers an opportunity to remove themselves.
  virtual void OnProactiveSuggestionsClientDestroying() {}

  // Invoked when the |proactive_suggestions| associated with the browser have
  // changed. Note that |proactive_suggestions| may be |nullptr| if none exist.
  virtual void OnProactiveSuggestionsChanged(
      const ProactiveSuggestions* proactive_suggestions) {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_OBSERVER_H_
