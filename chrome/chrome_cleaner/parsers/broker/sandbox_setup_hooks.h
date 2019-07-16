// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_BROKER_SANDBOX_SETUP_HOOKS_H_
#define CHROME_CHROME_CLEANER_PARSERS_BROKER_SANDBOX_SETUP_HOOKS_H_

#include <memory>

#include "base/command_line.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

using UniqueParserPtr =
    std::unique_ptr<mojom::ParserPtr, base::OnTaskRunnerDeleter>;

// Hooks to spawn a new sandboxed Parser process and bind a Mojo interface
// pointer to the sandboxed implementation.
class ParserSandboxSetupHooks : public MojoSandboxSetupHooks {
 public:
  ParserSandboxSetupHooks(scoped_refptr<MojoTaskRunner> mojo_task_runner,
                          base::OnceClosure connection_error_handler);
  ~ParserSandboxSetupHooks() override;

  // Transfers ownership of |parser_ptr_| to the caller.
  UniqueParserPtr TakeParserPtr();

  // SandboxSetupHooks
  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override;

 private:
  void BindParserPtr(mojo::ScopedMessagePipeHandle pipe_handle,
                     mojom::ParserPtr* parser_ptr);

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  base::OnceClosure connection_error_handler_;

  UniqueParserPtr parser_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ParserSandboxSetupHooks);
};

// Spawn a sandboxed process with type kParser, and return the bound
// |parser_ptr|.
ResultCode SpawnParserSandbox(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    UniqueParserPtr* parser_ptr);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_BROKER_SANDBOX_SETUP_HOOKS_H_
