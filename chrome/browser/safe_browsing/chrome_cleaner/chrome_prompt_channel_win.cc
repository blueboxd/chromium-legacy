// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_channel_win.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_actions_win.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace safe_browsing {

using content::BrowserThread;

namespace internal {

// Implementation of the ChromePrompt Mojo interface. Must be constructed and
// destructed on the IO thread. Calls ChromePromptActions to do the
// work for each message received.
class ChromePromptImpl : public chrome_cleaner::mojom::ChromePrompt {
 public:
  ChromePromptImpl(chrome_cleaner::mojom::ChromePromptRequest request,
                   base::OnceClosure on_connection_closed,
                   std::unique_ptr<ChromePromptActions> actions)
      : binding_(this, std::move(request)), actions_(std::move(actions)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    binding_.set_connection_error_handler(std::move(on_connection_closed));
  }

  ~ChromePromptImpl() override { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

  void PromptUser(
      const std::vector<base::FilePath>& files_to_delete,
      const base::Optional<std::vector<base::string16>>& registry_keys,
      const base::Optional<std::vector<base::string16>>& extension_ids,
      PromptUserCallback callback) override {
    // Wrap |callback| in a ChromePromptActions::PromptUserReplyCallback that
    // converts |prompt_acceptance| to a Mojo enum and invokes |callback| on
    // the IO thread.
    auto callback_wrapper =
        [](PromptUserCallback mojo_callback,
           ChromePromptActions::PromptAcceptance acceptance) {
          auto mojo_acceptance =
              static_cast<chrome_cleaner::mojom::PromptAcceptance>(acceptance);
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})
              ->PostTask(FROM_HERE, base::BindOnce(std::move(mojo_callback),
                                                   mojo_acceptance));
        };
    actions_->PromptUser(files_to_delete, registry_keys, extension_ids,
                         base::BindOnce(callback_wrapper, std::move(callback)));
  }

  void DisableExtensions(
      const std::vector<base::string16>& extension_ids,
      chrome_cleaner::mojom::ChromePrompt::DisableExtensionsCallback callback)
      override {
    std::move(callback).Run(actions_->DisableExtensions(extension_ids));
  }

 private:
  ChromePromptImpl(const ChromePromptImpl& other) = delete;
  ChromePromptImpl& operator=(ChromePromptImpl& other) = delete;

  mojo::Binding<chrome_cleaner::mojom::ChromePrompt> binding_;
  std::unique_ptr<ChromePromptActions> actions_;
};

}  // namespace internal

ChromePromptChannel::ChromePromptChannel(
    scoped_refptr<ChromeCleanerRunner> owner,
    std::unique_ptr<ChromePromptActions> actions)
    : owner_(std::move(owner)), actions_(std::move(actions)) {}

ChromePromptChannel::~ChromePromptChannel() = default;

// ChromePromptChannelMojo

ChromePromptChannelMojo::ChromePromptChannelMojo(
    scoped_refptr<ChromeCleanerRunner> owner,
    std::unique_ptr<ChromePromptActions> actions)
    : ChromePromptChannel(std::move(owner), std::move(actions)) {}

ChromePromptChannelMojo::~ChromePromptChannelMojo() = default;

void ChromePromptChannelMojo::PrepareForCleaner(
    base::CommandLine* command_line,
    base::HandlesToInheritVector* handles_to_inherit) {
  std::string pipe_name = base::NumberToString(base::RandUint64());
  request_pipe_ = invitation_.AttachMessagePipe(pipe_name);
  command_line->AppendSwitchASCII(chrome_cleaner::kChromeMojoPipeTokenSwitch,
                                  pipe_name);
  mojo_channel_.PrepareToPassRemoteEndpoint(handles_to_inherit, command_line);
}

void ChromePromptChannelMojo::CleanupAfterCleanerLaunchFailed() {
  // Mojo requires RemoteProcessLaunchAttempted to be called after the launch
  // whether it succeeded or failed.
  mojo_channel_.RemoteProcessLaunchAttempted();
}

void ChromePromptChannelMojo::ConnectToCleaner(
    const base::Process& cleaner_process) {
  mojo_channel_.RemoteProcessLaunchAttempted();
  // ChromePromptImpl tasks will need to run on the IO thread. There is
  // no need to synchronize its creation, since the client end will wait for
  // this initialization to be done before sending requests.
  base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&ChromePromptChannelMojo::CreateChromePromptImpl,
                         base::Unretained(this),
                         chrome_cleaner::mojom::ChromePromptRequest(
                             std::move(request_pipe_))));
  mojo::OutgoingInvitation::Send(std::move(invitation_),
                                 cleaner_process.Handle(),
                                 mojo_channel_.TakeLocalEndpoint());
}

void ChromePromptChannelMojo::CreateChromePromptImpl(
    chrome_cleaner::mojom::ChromePromptRequest chrome_prompt_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!chrome_prompt_impl_);

  // Cannot use std::make_unique() since it does not support creating
  // std::unique_ptrs with custom deleters.
  chrome_prompt_impl_.reset(new internal::ChromePromptImpl(
      std::move(chrome_prompt_request),
      base::BindOnce(&ChromeCleanerRunner::OnConnectionClosed,
                     base::RetainedRef(owner_)),
      // Pass ownership of actions_ to the ChromePromptImpl.
      std::move(actions_)));
}

}  // namespace safe_browsing
