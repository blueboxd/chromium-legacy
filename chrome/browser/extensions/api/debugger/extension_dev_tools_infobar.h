// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_EXTENSION_DEV_TOOLS_INFOBAR_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_EXTENSION_DEV_TOOLS_INFOBAR_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/callback_list.h"

namespace extensions {

// An infobar used to globally warn users that an extension is debugging the
// browser (which has security consequences).
class ExtensionDevToolsInfoBar {
 public:
  using CallbackList = base::OnceClosureList;

  // Ensures a global infobar corresponding to the supplied extension is
  // showing and registers |destroyed_callback| with it to be called back on
  // destruction.
  static std::unique_ptr<CallbackList::Subscription> Create(
      const std::string& extension_id,
      const std::string& extension_name,
      base::OnceClosure destroyed_callback);

  ExtensionDevToolsInfoBar(const ExtensionDevToolsInfoBar&) = delete;
  ExtensionDevToolsInfoBar& operator=(const ExtensionDevToolsInfoBar&) = delete;

 private:
  ExtensionDevToolsInfoBar(std::string extension_id,
                           const std::string& extension_name);
  ~ExtensionDevToolsInfoBar();

  // Adds |destroyed_callback| to the list of callbacks to run on destruction.
  std::unique_ptr<CallbackList::Subscription> RegisterDestroyedCallback(
      base::OnceClosure destroyed_callback);

  void InfoBarDestroyed();

  const std::string extension_id_;
  CallbackList callback_list_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_EXTENSION_DEV_TOOLS_INFOBAR_H_
