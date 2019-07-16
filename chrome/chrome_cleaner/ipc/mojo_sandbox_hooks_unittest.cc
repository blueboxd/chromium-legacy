// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/chrome_cleaner/mojom/test_mojo_sandbox_hooks.mojom.h"
#include "chrome/chrome_cleaner/ipc/mojo_sandbox_hooks.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/os/early_exit.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

constexpr char kTestString[] = "Hello World";

using UniqueTestMojoSandboxHooksPtr =
    std::unique_ptr<mojom::TestMojoSandboxHooksPtr, base::OnTaskRunnerDeleter>;

class MojoSandboxHooksTest : public base::MultiProcessTest {
 public:
  MojoSandboxHooksTest() : mojo_task_runner_(MojoTaskRunner::Create()) {}

 protected:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

// |TestMojoSandboxHooksImpl| runs and handles mojo requests in the sandbox
// child process.
class TestMojoSandboxHooksImpl : mojom::TestMojoSandboxHooks {
 public:
  explicit TestMojoSandboxHooksImpl(mojom::TestMojoSandboxHooksRequest request)
      : binding_(this, std::move(request)) {
    binding_.set_connection_error_handler(base::BindOnce(&EarlyExit, 1));
  }

  void EchoString(const std::string& input,
                  EchoStringCallback callback) override {
    std::move(callback).Run(input);
  }

 private:
  mojo::Binding<mojom::TestMojoSandboxHooks> binding_;
};

class TestSandboxSetupHooks : public MojoSandboxSetupHooks {
 public:
  explicit TestSandboxSetupHooks(scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : mojo_task_runner_(mojo_task_runner),
        // Manually use |new| here because |make_unique| doesn't work with
        // custom deleter.
        test_mojo_ptr_(new mojom::TestMojoSandboxHooksPtr,
                       base::OnTaskRunnerDeleter(mojo_task_runner_)) {}

  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override {
    mojo::ScopedMessagePipeHandle pipe_handle =
        SetupSandboxMessagePipe(policy, command_line);

    // Unretained pointer of |test_mojo_ptr_| is safe because its deleter is
    // run on the same task runner. So it won't be deleted before this task.
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(TestSandboxSetupHooks::BindTestMojoSandboxHooksPtr,
                       base::Unretained(test_mojo_ptr_.get()),
                       std::move(pipe_handle)));

    return RESULT_CODE_SUCCESS;
  }

  UniqueTestMojoSandboxHooksPtr TakeTestMojoSandboxHooksPtr() {
    return std::move(test_mojo_ptr_);
  }

 private:
  static void BindTestMojoSandboxHooksPtr(
      mojom::TestMojoSandboxHooksPtr* test_mojo_ptr,
      mojo::ScopedMessagePipeHandle pipe_handle) {
    test_mojo_ptr->Bind(
        mojom::TestMojoSandboxHooksPtrInfo(std::move(pipe_handle), 0));
    test_mojo_ptr->set_connection_error_handler(base::BindOnce(
        [] { FAIL() << "Mojo sandbox setup connection error"; }));
  }

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  UniqueTestMojoSandboxHooksPtr test_mojo_ptr_;
};

class TestSandboxTargetHooks : public MojoSandboxTargetHooks {
 public:
  ResultCode TargetDroppedPrivileges(
      const base::CommandLine& command_line) override {
    scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
    mojom::TestMojoSandboxHooksRequest request(
        ExtractSandboxMessagePipe(command_line));

    std::unique_ptr<TestMojoSandboxHooksImpl, base::OnTaskRunnerDeleter>
        impl_ptr(nullptr, base::OnTaskRunnerDeleter(mojo_task_runner));

    // This loop will run forever. Once the communication channel with the
    // broker process is broken, mojo error handler will abort this process.
    base::RunLoop loop;
    mojo_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(CreateTestMojoSandboxHooksImpl,
                       base::Unretained(&impl_ptr), std::move(request)));
    loop.Run();

    return RESULT_CODE_SUCCESS;
  }

 private:
  static void CreateTestMojoSandboxHooksImpl(
      std::unique_ptr<TestMojoSandboxHooksImpl, base::OnTaskRunnerDeleter>*
          impl_ptr,
      mojom::TestMojoSandboxHooksRequest request) {
    (*impl_ptr).reset(new TestMojoSandboxHooksImpl(std::move(request)));
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

void RunEchoString(mojom::TestMojoSandboxHooksPtr* test_mojo_ptr,
                   const std::string& input,
                   mojom::TestMojoSandboxHooks::EchoStringCallback callback) {
  DCHECK(test_mojo_ptr);

  (*test_mojo_ptr)->EchoString(input, std::move(callback));
}

void OnEchoStringDone(std::string* result_string,
                      base::OnceClosure done_callback,
                      const std::string& output) {
  *result_string = output;
  std::move(done_callback).Run();
}

}  // namespace

MULTIPROCESS_TEST_MAIN(MojoSandboxHooksTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  TestSandboxTargetHooks target_hooks;
  RunSandboxTarget(*base::CommandLine::ForCurrentProcess(),
                   sandbox_target_services, &target_hooks);

  return 0;
}

TEST_F(MojoSandboxHooksTest, SpawnSandboxTarget) {
  TestSandboxSetupHooks setup_hooks(mojo_task_runner_.get());

  ASSERT_EQ(RESULT_CODE_SUCCESS,
            StartSandboxTarget(MakeCmdLine("MojoSandboxHooksTargetMain"),
                               &setup_hooks, SandboxType::kTest));

  UniqueTestMojoSandboxHooksPtr test_mojo_ptr =
      setup_hooks.TakeTestMojoSandboxHooksPtr();

  std::string test_result_string;
  base::RunLoop loop;
  // Unretained pointers are safe because the test will wait until the task
  // ends.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(RunEchoString, base::Unretained(test_mojo_ptr.get()),
                     kTestString,
                     base::BindOnce(OnEchoStringDone,
                                    base::Unretained(&test_result_string),
                                    loop.QuitClosure())));
  loop.Run();
  EXPECT_EQ(test_result_string, kTestString);
}

}  // namespace chrome_cleaner
