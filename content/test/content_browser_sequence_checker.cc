// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_sequence_checker.h"

#include "base/bind.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/test/web_contents_observer_sequence_checker.h"

namespace content {

namespace {
bool g_sequence_checks_already_enabled = false;
}

ContentBrowserSequenceChecker::ContentBrowserSequenceChecker() {
  CHECK(!g_sequence_checks_already_enabled)
      << "Tried to enable ContentBrowserSequenceChecker, but it's already been "
      << "enabled.";
  g_sequence_checks_already_enabled = true;

  creation_hook_ =
      base::BindRepeating(&ContentBrowserSequenceChecker::OnWebContentsCreated,
                          base::Unretained(this));
  WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(creation_hook_);
}

ContentBrowserSequenceChecker::~ContentBrowserSequenceChecker() {
  WebContentsImpl::FriendWrapper::RemoveCreatedCallbackForTesting(
      creation_hook_);
  g_sequence_checks_already_enabled = false;
}

void ContentBrowserSequenceChecker::OnWebContentsCreated(
    WebContents* web_contents) {
  WebContentsObserverSequenceChecker::Enable(web_contents);
}

}  // namespace content
