// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_INTERACTION_HANDLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_INTERACTION_HANDLER_ANDROID_H_

#include <map>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Receives incoming events and runs the corresponding set of callbacks.
//
// - It is NOT safe to register new interactions while listening to events!
// - This class is NOT thread-safe!
// - The lifetimes of instances should be tied to the existence of a particular
// UI.
class InteractionHandlerAndroid : public EventHandler::Observer {
 public:
  // Each interaction callback has exactly one free |ValueProto| parameter.
  using InteractionCallback = base::RepeatingCallback<void(const ValueProto&)>;

  // Constructor. |event_handler| and |jcontext| must outlive this instance.
  InteractionHandlerAndroid(
      EventHandler* event_handler,
      base::android::ScopedJavaLocalRef<jobject> jcontext);
  ~InteractionHandlerAndroid() override;

  void StartListening();
  void StopListening();

  // Creates callbacks for each interaction in |proto| as well as the
  // corresponding view events in |views|. Returns false if |proto| is invalid.
  bool AddInteractionsFromProto(
      const InteractionsProto& proto,
      JNIEnv* env,
      const std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>&
          views,
      base::android::ScopedJavaGlobalRef<jobject> jdelegate,
      UserModel* user_model);

  // Overrides autofill_assistant::EventHandler::Observer:
  void OnEvent(const EventHandler::EventKey& key,
               const ValueProto& value) override;

 private:
  void AddInteraction(const EventHandler::EventKey& key,
                      const InteractionCallback& callback);

  // Maps event keys to the corresponding list of callbacks to execute.
  std::map<EventHandler::EventKey, std::vector<InteractionCallback>>
      interactions_;

  EventHandler* event_handler_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> jcontext_ = nullptr;
  bool is_listening_ = false;
  DISALLOW_COPY_AND_ASSIGN(InteractionHandlerAndroid);
};

}  //  namespace autofill_assistant

#endif  //  CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_INTERACTION_HANDLER_ANDROID_H_
