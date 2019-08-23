// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_H_
#define ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"

namespace ash {

class ProactiveSuggestions;

// A browser client which observes changes to the singleton BrowserList on
// behalf of Assistant to provide it with information necessary to retrieve
// proactive content suggestions.
class ASH_PUBLIC_EXPORT ProactiveSuggestionsClient {
 public:
  // A delegate interface for the ProactiveSuggestionsClient.
  class Delegate {
   public:
    // Invoked when the proactive suggestions client is being destroyed so as to
    // give the delegate an opportunity to remove itself.
    virtual void OnProactiveSuggestionsClientDestroying() {}

    // Invoked when the |proactive_suggestions| associated with the browser have
    // changed. Note that |proactive_suggestions| may be |nullptr| if none
    // exist.
    virtual void OnProactiveSuggestionsChanged(
        std::unique_ptr<ProactiveSuggestions> proactive_suggestions) {}

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Returns the singleton instance.
  static ProactiveSuggestionsClient* Get();

  // Sets the |delegate_| for the ProactiveSuggestionsClient. May be |nullptr|.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 protected:
  ProactiveSuggestionsClient();
  virtual ~ProactiveSuggestionsClient();

  Delegate* delegate_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestionsClient);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_H_
