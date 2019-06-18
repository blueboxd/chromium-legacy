// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_CHANNEL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_CHANNEL_WIN_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_runner_win.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace base {
class CommandLine;
class Process;
}  // namespace base

namespace safe_browsing {

namespace internal {
class ChromePromptImpl;
}  // namespace internal

class ChromePromptActions;

class ChromePromptChannel {
 public:
  ChromePromptChannel(scoped_refptr<ChromeCleanerRunner> owner,
                      std::unique_ptr<ChromePromptActions> actions);
  virtual ~ChromePromptChannel();

  // Prepares an IPC channel to be used by the cleaner process that is about to
  // be launched. Adds all handles used by the channel to |handles_to_inherit|
  // so that the cleaner process can access them, and adds switches to
  // |command_line| that the cleaner process can use to connect to the channel.
  virtual void PrepareForCleaner(
      base::CommandLine* command_line,
      base::HandlesToInheritVector* handles_to_inherit) = 0;

  // Does any cleanup required if the cleaner process fails to launch after
  // PrepareForCleaner was called.
  virtual void CleanupAfterCleanerLaunchFailed() = 0;

  // Kicks off communication between the IPC channel prepared by
  // PrepareForCleaner and the process in |cleaner_process|.
  virtual void ConnectToCleaner(const base::Process& cleaner_process) = 0;

 protected:
  scoped_refptr<ChromeCleanerRunner> owner_;
  std::unique_ptr<ChromePromptActions> actions_;

 private:
  ChromePromptChannel(const ChromePromptChannel& other) = delete;
  ChromePromptChannel& operator=(const ChromePromptChannel& other) = delete;
};

class ChromePromptChannelMojo : public ChromePromptChannel {
 public:
  ChromePromptChannelMojo(scoped_refptr<ChromeCleanerRunner> owner,
                          std::unique_ptr<ChromePromptActions> actions);
  ~ChromePromptChannelMojo() override;

  void PrepareForCleaner(
      base::CommandLine* command_line,
      base::HandlesToInheritVector* handles_to_inherit) override;

  void CleanupAfterCleanerLaunchFailed() override;

  void ConnectToCleaner(const base::Process& cleaner_process) override;

 private:
  ChromePromptChannelMojo(const ChromePromptChannelMojo& other) = delete;
  ChromePromptChannelMojo& operator=(const ChromePromptChannelMojo& other) =
      delete;

  void CreateChromePromptImpl(
      chrome_cleaner::mojom::ChromePromptRequest chrome_prompt_request);

  mojo::OutgoingInvitation invitation_;
  mojo::PlatformChannel mojo_channel_;
  mojo::ScopedMessagePipeHandle request_pipe_;

  std::unique_ptr<internal::ChromePromptImpl,
                  content::BrowserThread::DeleteOnIOThread>
      chrome_prompt_impl_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_PROMPT_CHANNEL_WIN_H_
