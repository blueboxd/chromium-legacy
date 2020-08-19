// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/icu_util.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/browser/code_cache/generated_code_cache_context.h"  // [nogncheck]
#include "content/browser/renderer_host/code_cache_host_impl.h"  // [nogncheck]
#include "content/browser/storage_partition_impl_map.h"          // [nogncheck]
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/test/fuzzer/code_cache_host_mojolpm_fuzzer.pb.h"
#include "mojo/core/embedder/embedder.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-mojolpm.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/origin.h"

using url::Origin;

namespace content {

const char* cmdline[] = {"code_cache_host_mojolpm_fuzzer", nullptr};

class ContentFuzzerEnvironment {
 public:
  ContentFuzzerEnvironment()
      : fuzzer_thread_("fuzzer_thread"),
        task_environment_(
            (base::CommandLine::Init(1, cmdline),
             TestTimeouts::Initialize(),
             base::test::TaskEnvironment::MainThreadType::DEFAULT),
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
            base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    logging::SetMinLogLevel(logging::LOG_FATAL);
    mojo::core::Init();
    base::i18n::InitializeICU();
    fuzzer_thread_.StartAndWaitForTesting();
  }

  scoped_refptr<base::SequencedTaskRunner> fuzzer_task_runner() {
    return fuzzer_thread_.task_runner();
  }

 private:
  base::AtExitManager at_exit_manager_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::Thread fuzzer_thread_;
  BrowserTaskEnvironment task_environment_;
  TestContentClientInitializer content_client_initializer_;
};

ContentFuzzerEnvironment g_environment;

ContentFuzzerEnvironment& SingletonEnvironment() {
  return g_environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return SingletonEnvironment().fuzzer_task_runner();
}

class CodeCacheHostFuzzerContext : public mojolpm::Context {
  const Origin kOriginA;
  const Origin kOriginB;
  const Origin kOriginOpaque;
  const Origin kOriginEmpty;

 public:
  CodeCacheHostFuzzerContext()
      : kOriginA(url::Origin::Create(GURL("http://aaa.com/"))),
        kOriginB(url::Origin::Create(GURL("http://bbb.com/"))),
        kOriginOpaque(url::Origin::Create(GURL("opaque"))),
        kOriginEmpty(url::Origin::Create(GURL("file://this_becomes_empty"))),
        browser_context_() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::PostTaskAndReply(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&CodeCacheHostFuzzerContext::InitializeOnUIThread,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void InitializeOnUIThread() {
    cache_storage_context_ = base::MakeRefCounted<CacheStorageContextImpl>();
    cache_storage_context_->Init(browser_context_.GetPath(),
                                 browser_context_.GetSpecialStoragePolicy(),
                                 nullptr);

    generated_code_cache_context_ =
        base::MakeRefCounted<GeneratedCodeCacheContext>();
    generated_code_cache_context_->Initialize(browser_context_.GetPath(),
                                              65536);
  }

  void AddCodeCacheHostImpl(
      uint32_t id,
      int renderer_id,
      const Origin& origin,
      mojo::PendingReceiver<::blink::mojom::CodeCacheHost>&& receiver) {
    code_cache_hosts_[renderer_id] = std::make_unique<CodeCacheHostImpl>(
        renderer_id, cache_storage_context_, generated_code_cache_context_,
        std::move(receiver));
  }

  void AddCodeCacheHost(
      uint32_t id,
      int renderer_id,
      content::fuzzing::code_cache_host::proto::NewCodeCacheHostAction::OriginId
          origin_id) {
    mojo::Remote<::blink::mojom::CodeCacheHost> remote;
    auto receiver = remote.BindNewPipeAndPassReceiver();

    const Origin* origin = &kOriginA;
    if (origin_id == 1) {
      origin = &kOriginB;
    } else if (origin_id == 2) {
      origin = &kOriginOpaque;
    } else if (origin_id == 3) {
      origin = &kOriginEmpty;
    }

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::PostTaskAndReply(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&CodeCacheHostFuzzerContext::AddCodeCacheHostImpl,
                       base::Unretained(this), id, renderer_id, *origin,
                       std::move(receiver)),
        run_loop.QuitClosure());
    run_loop.Run();

    AddInstance(id, std::move(remote));
  }

 private:
  TestBrowserContext browser_context_;

  scoped_refptr<CacheStorageContextImpl> cache_storage_context_;
  scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context_;

  std::map<int, std::unique_ptr<CodeCacheHostImpl>> code_cache_hosts_;
};
}  // namespace content

class CodeCacheHostTestcase : public mojolpm::TestcaseBase {
  content::CodeCacheHostFuzzerContext& cch_context_;
  const content::fuzzing::code_cache_host::proto::Testcase& testcase_;
  int next_idx_ = 0;
  int action_count_ = 0;
  const int MAX_ACTION_COUNT = 512;

 public:
  CodeCacheHostTestcase(
      content::CodeCacheHostFuzzerContext& cch_context,
      const content::fuzzing::code_cache_host::proto::Testcase& testcase);
  ~CodeCacheHostTestcase() override;
  bool IsFinished() override;
  void NextAction() override;
};

CodeCacheHostTestcase::CodeCacheHostTestcase(
    content::CodeCacheHostFuzzerContext& cch_context,
    const content::fuzzing::code_cache_host::proto::Testcase& testcase)
    : cch_context_(cch_context), testcase_(testcase) {}

CodeCacheHostTestcase::~CodeCacheHostTestcase() {}

bool CodeCacheHostTestcase::IsFinished() {
  return next_idx_ >= testcase_.sequence_indexes_size();
}

void CodeCacheHostTestcase::NextAction() {
  if (next_idx_ < testcase_.sequence_indexes_size()) {
    auto sequence_idx = testcase_.sequence_indexes(next_idx_++);
    const auto& sequence =
        testcase_.sequences(sequence_idx % testcase_.sequences_size());
    for (auto action_idx : sequence.action_indexes()) {
      if (!testcase_.actions_size() || ++action_count_ > MAX_ACTION_COUNT) {
        return;
      }
      const auto& action =
          testcase_.actions(action_idx % testcase_.actions_size());
      switch (action.action_case()) {
        case content::fuzzing::code_cache_host::proto::Action::
            kNewCodeCacheHost: {
          cch_context_.AddCodeCacheHost(
              action.new_code_cache_host().id(),
              action.new_code_cache_host().render_process_id(),
              action.new_code_cache_host().origin_id());
        } break;

        case content::fuzzing::code_cache_host::proto::Action::kRunThread: {
          if (action.run_thread().id()) {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                           run_loop.QuitClosure());
            run_loop.Run();
          } else {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                           run_loop.QuitClosure());
            run_loop.Run();
          }
        } break;

        case content::fuzzing::code_cache_host::proto::Action::
            kCodeCacheHostRemoteAction: {
          mojolpm::HandleRemoteAction(action.code_cache_host_remote_action());
        } break;

        case content::fuzzing::code_cache_host::proto::Action::ACTION_NOT_SET:
          break;
      }
    }
  }
}

void NextAction(content::CodeCacheHostFuzzerContext* context,
                base::RepeatingClosure quit_closure) {
  if (!context->IsFinished()) {
    context->NextAction();
    content::GetFuzzerTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(NextAction, base::Unretained(context),
                                  std::move(quit_closure)));
  } else {
    content::GetFuzzerTaskRunner()->PostTask(FROM_HERE,
                                             std::move(quit_closure));
  }
}

void RunTestcase(
    content::CodeCacheHostFuzzerContext* context,
    const content::fuzzing::code_cache_host::proto::Testcase* testcase) {
  mojo::Message message;
  auto dispatch_context =
      std::make_unique<mojo::internal::MessageDispatchContext>(&message);

  CodeCacheHostTestcase cch_testcase(*context, *testcase);
  context->StartTestcase(&cch_testcase, content::GetFuzzerTaskRunner());

  base::RunLoop fuzzer_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  content::GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(NextAction, base::Unretained(context),
                                fuzzer_run_loop.QuitClosure()));
  fuzzer_run_loop.Run();

  context->EndTestcase();
}

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::code_cache_host::proto::Testcase& testcase) {
  if (!testcase.actions_size() || !testcase.sequences_size() ||
      !testcase.sequence_indexes_size()) {
    return;
  }

  content::CodeCacheHostFuzzerContext context;
  mojolpm::SetContext(&context);

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  content::GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(RunTestcase, base::Unretained(&context),
                     base::Unretained(&testcase)),
      ui_run_loop.QuitClosure());

  ui_run_loop.Run();
}
