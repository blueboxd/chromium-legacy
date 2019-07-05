// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_ACTIONS_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_ACTIONS_WIN_H_

#include <ostream>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string16.h"

namespace extensions {
class ExtensionService;
}  // namespace extensions

namespace safe_browsing {

class ChromeCleanerScannerResults;

// Actions triggered from the ChromePrompt IPC interface.
//
// A ChromePromptActions object should be owned by the ChromePromptChannel that
// implements the IPC channel. It can be called to trigger an action for each
// message received.
class ChromePromptActions {
 public:
  // TODO(crbug.com/969139): This mirrors the PromptAcceptance enums from
  // chrome_prompt.mojom and chrome_prompt.proto. Once the
  // ChromeCleanupProtobufIPC experiment is over remove this and use the proto
  // version directly.
  enum class PromptAcceptance {
    UNSPECIFIED,
    // The user explicitly accepted the cleanup operation and cleaner logs
    // upload is allowed.
    ACCEPTED_WITH_LOGS,
    // The user explicitly accepted the cleanup operation and cleaner logs
    // upload is not allowed.
    ACCEPTED_WITHOUT_LOGS,
    // The user explicitly denied the Chrome prompt.
    DENIED,
  };

  // A callback to be called after showing the prompt, with the user's choice.
  using PromptUserReplyCallback = base::OnceCallback<void(PromptAcceptance)>;

  // A callback to show the prompt. The ChromeCleanerScannerResults contains
  // the items that were detected by the scanner, for display in the prompt.
  // The PromptUserCallback must call the PromptUserReplyCallback with the
  // user's choice.
  using PromptUserCallback =
      base::OnceCallback<void(ChromeCleanerScannerResults&&,
                              PromptUserReplyCallback)>;

  // Initializes the actions to use |extension_service| to disable extensions
  // when the DisableExtensions method is called, and the |on_prompt_user|
  // callback to display the prompt when the PromptUser method is called.
  ChromePromptActions(extensions::ExtensionService* extension_service,
                      PromptUserCallback on_prompt_user);
  ~ChromePromptActions();

  // Calls the PromptUserCallback to show the prompt.  |reply_callback| will be
  // called with the user's choice.
  void PromptUser(
      const std::vector<base::FilePath>& files_to_delete,
      const base::Optional<std::vector<base::string16>>& registry_keys,
      const base::Optional<std::vector<base::string16>>& extension_ids,
      PromptUserReplyCallback reply_callback);

  // Deletes the given |extension_ids|. Returns false on an error, including if
  // not all |extension_ids| were displayed to the user in the last PromptUser
  // call.
  //
  // TODO(crbug.com/969139): Now that we're updating the IPC interface, rename
  // this DeleteExtensions to match the implementation.
  bool DisableExtensions(const std::vector<base::string16>& extension_ids);

 private:
  ChromePromptActions(const ChromePromptActions& other) = delete;
  ChromePromptActions& operator=(const ChromePromptActions& other) = delete;

  // Extension service used to implement DisableExtensions. This can be null if
  // no ExtensionService is available, otherwise it is a long-running service
  // that will outlive ChromePromptActions.
  extensions::ExtensionService* extension_service_;

  // Callback that will be invoked when PromptUser is called to display the
  // prompt.
  PromptUserCallback on_prompt_user_;

  std::vector<base::string16> extension_ids_;
};

//  Format for debug output in tests.
std::ostream& operator<<(
    std::ostream& out,
    ChromePromptActions::PromptAcceptance prompt_acceptance);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_ACTIONS_WIN_H_
