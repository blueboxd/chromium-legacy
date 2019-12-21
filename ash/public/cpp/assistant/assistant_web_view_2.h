// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_WEB_VIEW_2_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_WEB_VIEW_2_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "ui/views/view.h"

class GURL;
enum class WindowOpenDisposition;

namespace ash {

// TODO(b/146520500): Rename to AssistantWebView after freeing up name which is
// currently in use.
// A view which wraps a views::WebView (and associated content::WebContents) to
// work around dependency restrictions in Ash.
class ASH_PUBLIC_EXPORT AssistantWebView2 : public views::View {
 public:
  // Initialization parameters which dictate how an instance of
  // AssistantWebView2 should behave.
  struct InitParams {
    InitParams();
    InitParams(const InitParams& copy);
    ~InitParams();

    // If enabled, AssistantWebView2 will automatically resize to the size
    // desired by its embedded content::WebContents. Note that, if specified,
    // the content::WebContents will be bounded by |min_size| and |max_size|.
    bool enable_auto_resize = false;
    base::Optional<gfx::Size> min_size = base::nullopt;
    base::Optional<gfx::Size> max_size = base::nullopt;

    // If enabled, AssistantWebView2 will suppress navigation attempts of its
    // embedded content::WebContents. When navigation suppression occurs,
    // Observer::DidSuppressNavigation() will be invoked.
    bool suppress_navigation = false;
  };

  // An observer which receives AssistantWebView2 events.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the embedded content::WebContents has stopped loading.
    virtual void DidStopLoading() {}

    // Invoked when the embedded content::WebContents has suppressed navigation.
    virtual void DidSuppressNavigation(const GURL& url,
                                       WindowOpenDisposition disposition,
                                       bool from_user_gesture) {}

    // Invoked when the focused node within the embedded content::WebContents
    // has changed.
    virtual void DidChangeFocusedNode(const gfx::Rect& node_bounds_in_screen) {}
  };

  ~AssistantWebView2() override;

  // Adds/removes the specified |observer|.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Invoke to navigate back in the embedded content::WebContents' navigation
  // stack. If backwards navigation is not possible, return |false|. Otherwise
  // returns |true| to indicate success.
  virtual bool GoBack() = 0;

  // Invoke to navigate the embedded content::WebContents' to |url|.
  virtual void Navigate(const GURL& url) = 0;

 protected:
  AssistantWebView2();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_WEB_VIEW_2_H_
