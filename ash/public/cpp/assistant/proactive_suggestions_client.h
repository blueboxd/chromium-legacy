// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_H_
#define ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/assistant/proactive_suggestions_client_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

// A browser client which observes changes to the singleton BrowserList on
// behalf of Assistant to provide it with information necessary to retrieve
// proactive content suggestions.
class ASH_PUBLIC_EXPORT ProactiveSuggestionsClient {
 public:
  // Returns the singleton instance.
  static ProactiveSuggestionsClient* GetInstance();

  // Adds/removes the specified observer.
  void AddObserver(ProactiveSuggestionsClientObserver* observer);
  void RemoveObserver(ProactiveSuggestionsClientObserver* observer);

 protected:
  ProactiveSuggestionsClient();
  virtual ~ProactiveSuggestionsClient();

  base::ObserverList<ProactiveSuggestionsClientObserver> observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestionsClient);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_H_
