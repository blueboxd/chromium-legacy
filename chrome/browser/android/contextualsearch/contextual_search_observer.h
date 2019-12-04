// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace contextual_search {

class ContextualSearchJsApiHandler;

// Observer for the |web_contents| passed to the constructor, which will be used
// later on to call contextual_search::CreateContextualSearchJsApiService() at
// the time of binding the mojo::Receiver<mojom::ContextualSearchJsApiService>
// to the implementation of the ContextualSearchJsApiService interface.
class ContextualSearchObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContextualSearchObserver> {
 public:
  explicit ContextualSearchObserver(content::WebContents* web_contents);
  ~ContextualSearchObserver() override;

  static void SetHandlerForWebContents(content::WebContents* web_contents,
                                       ContextualSearchJsApiHandler* handler);

  ContextualSearchJsApiHandler* api_handler() const { return api_handler_; }

 private:
  friend class content::WebContentsUserData<ContextualSearchObserver>;

  void set_api_handler(ContextualSearchJsApiHandler* handler) {
    api_handler_ = handler;
  }

  ContextualSearchJsApiHandler* api_handler_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchObserver);
};

}  // namespace contextual_search

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_OBSERVER_H_
