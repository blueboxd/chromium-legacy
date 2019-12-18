// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_DISPATCH_CALLBACK_STORAGE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_DISPATCH_CALLBACK_STORAGE_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"

// Callback for OverlayResponses dispatched for user interaction events
// occurring in an ongoing overlay.
typedef base::RepeatingCallback<void(OverlayResponse* response)>
    OverlayDispatchCallback;

// Storage object used to hold OverlayDispatchCallbacks and execute them for
// dispatched responses.
class OverlayDispatchCallbackStorage {
 public:
  OverlayDispatchCallbackStorage();
  ~OverlayDispatchCallbackStorage();

  // Adds |callback| to the storage to be executed whenever DispatchResponse()
  // is called with an OverlayResponse created with InfoType.
  template <class InfoType>
  void AddDispatchCallback(OverlayDispatchCallback callback) {
    DCHECK(!callback.is_null());
    GetCallbackList<InfoType>()->AddCallback(std::move(callback));
  }

  // Executes the added callbacks for |response|.
  void DispatchResponse(OverlayResponse* response);

 protected:
  // Helper object that stores OverlayDispatchCallbacks for OverlayResponses
  // created with a specific info type.
  class CallbackList {
   public:
    CallbackList();
    virtual ~CallbackList();

    // Adds the callback to the list.
    void AddCallback(OverlayDispatchCallback callback);
    // Executes the callbacks with |response| if it's supported.
    void ExecuteCallbacks(OverlayResponse* response);

   protected:
    // Returns whether the callbacks in the list have been registered for
    // |response|'s info type.
    virtual bool ShouldExecuteForResponse(OverlayResponse* response) = 0;

    // The callbacks stored in this list.
    std::vector<OverlayDispatchCallback> callbacks_;
  };

  // Template used to create CallbackList instantiations for a specific info
  // type.
  template <class InfoType>
  class CallbackListImpl : public CallbackList {
   public:
    CallbackListImpl() = default;
    ~CallbackListImpl() override = default;

   private:
    // CallbackList:
    bool ShouldExecuteForResponse(OverlayResponse* response) override {
      return !!response->GetInfo<InfoType>();
    }
  };

  // Returns the CallbackList for OverlayRequests created with InfoType,
  // creating one if necessary.
  template <class InfoType>
  CallbackList* GetCallbackList() {
    auto& callback_list = callback_lists_[InfoType::UserDataKey()];
    if (!callback_list)
      callback_list = std::make_unique<CallbackListImpl<InfoType>>();
    return callback_list.get();
  }

  // Map storing the CallbackList under the user data key for each supported
  // OverlayRequest info type.
  std::map<const void*, std::unique_ptr<CallbackList>> callback_lists_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_DISPATCH_CALLBACK_STORAGE_H_
