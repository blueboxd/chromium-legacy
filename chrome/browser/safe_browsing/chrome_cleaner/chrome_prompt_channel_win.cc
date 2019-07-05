// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_channel_win.h"

#include <windows.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "base/win/win_util.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_actions_win.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace safe_browsing {

using base::win::ScopedHandle;
using content::BrowserThread;
using CleanerProcessDelegate = ChromePromptChannel::CleanerProcessDelegate;

namespace {

class CleanerProcessWrapper : public CleanerProcessDelegate {
 public:
  explicit CleanerProcessWrapper(const base::Process& process)
      : process_(process.Duplicate()) {}

  ~CleanerProcessWrapper() override = default;

  base::ProcessHandle Handle() const override { return process_.Handle(); }

  void TerminateOnError() const override {
    // TODO(crbug.com/969139): Assign a new exit code?
    process_.Terminate(
        chrome_cleaner::RESULT_CODE_CHROME_PROMPT_IPC_DISCONNECTED_TOO_SOON,
        /*wait=*/false);
  }

 private:
  CleanerProcessWrapper(const CleanerProcessWrapper& other) = delete;
  CleanerProcessWrapper& operator=(const CleanerProcessWrapper& other) = delete;

  base::Process process_;
};

enum class ServerPipeDirection {
  kInbound,
  kOutbound,
};

// Opens a pipe of type PIPE_TYPE_MESSAGE and returns a pair (server handle,
// client handle). The client handle is safe to pass to a subprocess. Both
// handles will be invalid on failure.
std::pair<ScopedHandle, ScopedHandle> CreateMessagePipe(
    ServerPipeDirection server_direction) {
  SECURITY_ATTRIBUTES security_attributes = {};
  security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  // Use this process's default access token.
  security_attributes.lpSecurityDescriptor = nullptr;
  // Handles to inherit will be added to the LaunchOptions explicitly.
  security_attributes.bInheritHandle = false;

  base::string16 pipe_name = base::UTF8ToUTF16(
      base::StrCat({"\\\\.\\pipe\\chrome-cleaner-",
                    base::UnguessableToken::Create().ToString()}));

  // Create the server end of the pipe.
  DWORD direction_flag = server_direction == ServerPipeDirection::kInbound
                             ? PIPE_ACCESS_INBOUND
                             : PIPE_ACCESS_OUTBOUND;
  ScopedHandle server_handle(::CreateNamedPipe(
      pipe_name.c_str(), direction_flag | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
          PIPE_REJECT_REMOTE_CLIENTS,
      /*nMaxInstances=*/1, /*nOutBufferSize=*/0, /*nInBufferSize=*/0,
      /*nDefaultTimeOut=*/0, &security_attributes));
  if (!server_handle.IsValid()) {
    PLOG(ERROR) << "Error creating server pipe";
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }

  // The client pipe's read/write permissions are the opposite of the server's.
  DWORD client_mode = server_direction == ServerPipeDirection::kInbound
                          ? GENERIC_WRITE
                          : GENERIC_READ;

  // Create the client end of the pipe.
  ScopedHandle client_handle(::CreateFile(
      pipe_name.c_str(), client_mode, /*dwShareMode=*/0,
      /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS,
      /*hTemplateFile=*/nullptr));
  if (!client_handle.IsValid()) {
    PLOG(ERROR) << "Error creating client pipe";
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }

  // Wait for the client end to connect (this should return
  // ERROR_PIPE_CONNECTED immediately since it's already connected).
  if (::ConnectNamedPipe(server_handle.Get(), /*lpOverlapped=*/nullptr)) {
    LOG(ERROR) << "ConnectNamedPipe got an unexpected connection";
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }
  const auto error = ::GetLastError();
  if (error != ERROR_PIPE_CONNECTED) {
    LOG(ERROR) << "ConnectNamedPipe returned unexpected error: "
               << logging::SystemErrorCodeToString(error);
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }

  return std::make_pair(std::move(server_handle), std::move(client_handle));
}

void AppendHandleToCommandLine(base::CommandLine* command_line,
                               const std::string& switch_string,
                               HANDLE handle) {
  DCHECK(command_line);
  command_line->AppendSwitchASCII(
      switch_string, base::NumberToString(base::win::HandleToUint32(handle)));
}

void ServiceChromePromptRequests(
    base::WeakPtr<ChromePromptChannelProtobuf> channel,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    HANDLE request_read_handle,
    HANDLE response_write_handle,
    std::unique_ptr<CleanerProcessDelegate> cleaner_process,
    base::OnceClosure on_connection_closed) {
  // Always call OnConnectionClosed when finished whether it's with an error or
  // because a CloseConnectionRequest was received.
  base::ScopedClosureRunner call_connection_closed(
      std::move(on_connection_closed));

  // On any error condition, kill the cleaner process (if it's already dead this
  // is a no-op).
  base::ScopedClosureRunner kill_cleaner_on_error(base::BindOnce(
      &CleanerProcessDelegate::TerminateOnError, std::move(cleaner_process)));

  // TODO(crbug.com/969139): Read requests and send responses. Any errors that
  // happen during this stage will call |kill_cleaner_on_error|. It will never
  // be called until this TODO is implemented.

  // Normal exit: do not kill the cleaner. OnConnectionClosed will still be
  // called.
  kill_cleaner_on_error.ReplaceClosure(base::DoNothing());
}

}  // namespace

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

// ChromePromptChannel

ChromePromptChannel::ChromePromptChannel(
    base::OnceClosure on_connection_closed,
    std::unique_ptr<ChromePromptActions> actions,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : on_connection_closed_(std::move(on_connection_closed)),
      actions_(std::move(actions)),
      task_runner_(std::move(task_runner)) {}

ChromePromptChannel::~ChromePromptChannel() = default;

// static
std::unique_ptr<CleanerProcessDelegate>
ChromePromptChannel::CreateDelegateForProcess(const base::Process& process) {
  return std::make_unique<CleanerProcessWrapper>(process);
}

// ChromePromptChannelMojo

ChromePromptChannelMojo::ChromePromptChannelMojo(
    base::OnceClosure on_connection_closed,
    std::unique_ptr<ChromePromptActions> actions,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ChromePromptChannel(std::move(on_connection_closed),
                          std::move(actions),
                          std::move(task_runner)),
      weak_factory_(this) {}

ChromePromptChannelMojo::~ChromePromptChannelMojo() = default;

bool ChromePromptChannelMojo::PrepareForCleaner(
    base::CommandLine* command_line,
    base::HandlesToInheritVector* handles_to_inherit) {
  std::string pipe_name = base::NumberToString(base::RandUint64());
  request_pipe_ = invitation_.AttachMessagePipe(pipe_name);
  command_line->AppendSwitchASCII(chrome_cleaner::kChromeMojoPipeTokenSwitch,
                                  pipe_name);
  mojo_channel_.PrepareToPassRemoteEndpoint(handles_to_inherit, command_line);
  return true;
}

void ChromePromptChannelMojo::CleanupAfterCleanerLaunchFailed() {
  // Mojo requires RemoteProcessLaunchAttempted to be called after the launch
  // whether it succeeded or failed.
  mojo_channel_.RemoteProcessLaunchAttempted();
}

void ChromePromptChannelMojo::ConnectToCleaner(
    std::unique_ptr<CleanerProcessDelegate> cleaner_process) {
  mojo_channel_.RemoteProcessLaunchAttempted();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromePromptChannelMojo::CreateChromePromptImpl,
                     weak_factory_.GetWeakPtr(),
                     chrome_cleaner::mojom::ChromePromptRequest(
                         std::move(request_pipe_))));
  mojo::OutgoingInvitation::Send(std::move(invitation_),
                                 cleaner_process->Handle(),
                                 mojo_channel_.TakeLocalEndpoint());
}

void ChromePromptChannelMojo::CreateChromePromptImpl(
    chrome_cleaner::mojom::ChromePromptRequest chrome_prompt_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!chrome_prompt_impl_);

  chrome_prompt_impl_ = std::make_unique<internal::ChromePromptImpl>(
      std::move(chrome_prompt_request),
      // Pass ownership of on_connection_closed_ and actions_ to the
      // ChromePromptImpl.
      std::move(on_connection_closed_), std::move(actions_));
}

// ChromePromptChannelProtobuf

ChromePromptChannelProtobuf::ChromePromptChannelProtobuf(
    base::OnceClosure on_connection_closed,
    std::unique_ptr<ChromePromptActions> actions,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ChromePromptChannel(std::move(on_connection_closed),
                          std::move(actions),
                          std::move(task_runner)),
      weak_factory_(this) {
  // The sequence checker validates that all handler methods and the destructor
  // are called from the same sequence, which is not the same sequence as the
  // constructor.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ChromePromptChannelProtobuf::~ChromePromptChannelProtobuf() {
  // To avoid race conditions accessing WeakPtr's this must be deleted on the
  // same sequence as the request handler methods are called.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool ChromePromptChannelProtobuf::PrepareForCleaner(
    base::CommandLine* command_line,
    base::HandlesToInheritVector* handles_to_inherit) {
  // Requests flow from client to server.
  std::tie(request_read_handle_, request_write_handle_) =
      CreateMessagePipe(ServerPipeDirection::kInbound);

  // Responses flow from server to client.
  std::tie(response_write_handle_, response_read_handle_) =
      CreateMessagePipe(ServerPipeDirection::kOutbound);

  if (!request_read_handle_.IsValid() || !request_write_handle_.IsValid() ||
      !response_read_handle_.IsValid() || !response_write_handle_.IsValid()) {
    return false;
  }

  // The Chrome Cleanup tool will write to the request pipe and read from the
  // response pipe.
  DCHECK(command_line);
  DCHECK(handles_to_inherit);
  AppendHandleToCommandLine(command_line,
                            chrome_cleaner::kChromeWriteHandleSwitch,
                            request_write_handle_.Get());
  handles_to_inherit->push_back(request_write_handle_.Get());
  AppendHandleToCommandLine(command_line,
                            chrome_cleaner::kChromeReadHandleSwitch,
                            response_read_handle_.Get());
  handles_to_inherit->push_back(response_read_handle_.Get());
  return true;
}

void ChromePromptChannelProtobuf::CleanupAfterCleanerLaunchFailed() {
  request_read_handle_.Close();
  request_write_handle_.Close();
  response_read_handle_.Close();
  response_write_handle_.Close();
}

void ChromePromptChannelProtobuf::ConnectToCleaner(
    std::unique_ptr<CleanerProcessDelegate> cleaner_process) {
  // The handles that were passed to the cleaner are no longer needed in this
  // process.
  request_write_handle_.Close();
  response_read_handle_.Close();

  // ServiceChromePromptRequests will loop until a CloseConnection message is
  // received over IPC or an error occurs, and will call request handler
  // methods on |task_runner_|.
  //
  // This object continues to own the pipe handles. ServiceChromePromptRequests
  // gets raw handles, which can go invalid at any time either because the
  // other end of the pipe closes or CloseHandles is called. When that happens
  // the next call to ::ReadFile will return an error and
  // ServiceChromePromptRequests will return.
  base::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ServiceChromePromptRequests, weak_factory_.GetWeakPtr(),
                     task_runner_, request_read_handle_.Get(),
                     response_write_handle_.Get(), std::move(cleaner_process),
                     std::move(on_connection_closed_)));
}

}  // namespace safe_browsing
