// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verifier/nss_ocsp_session_url_loader.h"

#include <array>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_nss.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert_net/nss_ocsp.h"
#include "net/http/http_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace cert_verifier {

namespace {

// Matches the caIssuers hostname from the generated certificate.
const char kAiaHost[] = "aia-test.invalid";
// Returning a single DER-encoded cert, so the mime-type must be
// application/pkix-cert per RFC 5280.
const char kAiaHeaders[] =
    "HTTP/1.1 200 OK\0"
    "Content-type: application/pkix-cert\0"
    "\0";

const char kMimeType[] = "application/pkix-cert";

const char kDummyCertContents[] = "dummy_data";

const base::TimeDelta kTimeout = base::TimeDelta::FromHours(1);

network::mojom::URLResponseHeadPtr GetResponseHead() {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(kAiaHeaders));
  head->mime_type = kMimeType;
  return head;
}

void GetIntermediateCertContents(std::string* file_contents) {
  ASSERT_TRUE(base::ReadFileToString(
      net::GetTestCertsDirectory().AppendASCII("aia-intermediate.der"),
      file_contents));
  ASSERT_FALSE(file_contents->empty());
}

}  // namespace

class OCSPRequestSessionDelegateURLLoaderTest : public ::testing::Test {
 public:
  OCSPRequestSessionDelegateURLLoaderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        loader_factory_(std::make_unique<network::TestURLLoaderFactory>()),
        delegate_factory_(
            std::make_unique<OCSPRequestSessionDelegateFactoryURLLoader>(
                base::SequencedTaskRunnerHandle::Get(),
                loader_factory_->GetSafeWeakWrapper())) {}

  void SetUp() override {
    params_.url = intercept_url_;
    params_.http_request_method = "GET";
    params_.timeout = kTimeout;

    old_interceptor_ = base::BindLambdaForTesting(
        [this](const network::ResourceRequest& request) {
          EXPECT_EQ(request.url, intercept_url_);
          EXPECT_TRUE(request.trusted_params.has_value());
          EXPECT_TRUE(request.trusted_params->disable_secure_dns);
          num_loaders_created_++;
        });
    loader_factory_->SetInterceptor(old_interceptor_);
  }

 protected:
  // Primes |loader_factory_| to respond with |head| and |file_contents| to
  // |intercept_url_|.
  void AddResponse(network::mojom::URLResponseHeadPtr head,
                   std::string file_contents,
                   network::TestURLLoaderFactory::Redirects redirects =
                       network::TestURLLoaderFactory::Redirects()) {
    loader_factory_->AddResponse(
        intercept_url_, std::move(head), std::move(file_contents),
        network::URLLoaderCompletionStatus(), std::move(redirects));
  }

  // Sets an interceptor for |loader_factory_| that will be run once and then be
  // replaced with the old interceptor. The old interceptor will still run after
  // this one.
  void AddTemporaryInterceptor(
      base::RepeatingCallback<void(const network::ResourceRequest& request)>
          temp_interceptor) {
    loader_factory_->SetInterceptor(base::BindLambdaForTesting(
        [this, temp_interceptor = std::move(temp_interceptor)](
            const network::ResourceRequest& request) {
          temp_interceptor.Run(request);
          old_interceptor_.Run(request);
          loader_factory_->SetInterceptor(old_interceptor_);
        }));
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  int num_loaders_created() const { return num_loaders_created_; }

  // Ensures that at least |n|+1 worker threads have been created, then
  // returns the nth one.
  scoped_refptr<base::SequencedTaskRunner> worker_thread(size_t n) {
    while (worker_threads_.size() <= n) {
      worker_threads_.push_back(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
    }
    return worker_threads_[n];
  }

  net::OCSPRequestSessionParams* params() { return &params_; }

  OCSPRequestSessionDelegateFactoryURLLoader* delegate_factory() {
    return delegate_factory_.get();
  }

  void ResetDelegateFactory() { delegate_factory_.reset(); }
  void ResetLoaderFactory() { loader_factory_.reset(); }

 private:
  const GURL intercept_url_{std::string("http://") + kAiaHost};

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory_;
  int num_loaders_created_ = 0;

  // Sequence that delegate->StartAndWait() is called on, that blocks.
  std::vector<scoped_refptr<base::SequencedTaskRunner>> worker_threads_;

  net::OCSPRequestSessionParams params_;

  std::unique_ptr<OCSPRequestSessionDelegateFactoryURLLoader> delegate_factory_;

  base::RepeatingCallback<void(const network::ResourceRequest& request)>
      old_interceptor_;
};

// Tests that OCSPRequestSessionURLLoader will fail when asked to load HTTPS
// URLs.
TEST_F(OCSPRequestSessionDelegateURLLoaderTest, TestNoHttps) {
  base::RunLoop run_loop;
  worker_thread(0)->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto delegate = delegate_factory()->CreateOCSPRequestSessionDelegate();

        std::unique_ptr<net::OCSPRequestSessionResult> result;
        {
          // Request a load from an HTTPS URL.
          params()->url = GURL(std::string("https://") + kAiaHost);

          base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync;
          result = delegate->StartAndWait(params());
        }

        ASSERT_FALSE(result);

        run_loop.Quit();
      }));
  run_loop.Run();
}

// Tests that the timeout works correctly.
TEST_F(OCSPRequestSessionDelegateURLLoaderTest, TestTimeout) {
  scoped_refptr<net::OCSPRequestSessionDelegate> delegate;
  OCSPRequestSessionDelegateURLLoader* delegate_ptr;

  {
    base::RunLoop run_loop;
    worker_thread(0)->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          delegate = delegate_factory()->CreateOCSPRequestSessionDelegate();
          delegate_ptr =
              static_cast<OCSPRequestSessionDelegateURLLoader*>(delegate.get());

          delegate_ptr->Start(params());

          // Tell the main thread to continue once it has serviced the
          // StartLoad() that was posted to it.
          run_loop.QuitWhenIdle();
        }));
    run_loop.Run();
  }

  // Drain all the tasks from all the queues to make sure that the
  // SimpleURLLoader has started its load and timeout.
  task_environment()->FastForwardUntilNoTasksRemain();

  {
    base::RunLoop run_loop;
    worker_thread(0)->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          std::unique_ptr<net::OCSPRequestSessionResult> result;
          {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync;
            result = delegate_ptr->WaitForResult();
          }

          ASSERT_FALSE(result);

          run_loop.Quit();
        }));

    // The load_sequence has started the SimpleURLLoader, so we can advance time
    // by the timeout to cause the timeout to fire.
    task_environment()->AdvanceClock(kTimeout);

    run_loop.Run();
  }

  // Expect that a URLLoader was created by the SimpleURLLoader.
  EXPECT_LT(0, num_loaders_created());
}

// Tests that a redirect to HTTPS causes a failure.
TEST_F(OCSPRequestSessionDelegateURLLoaderTest, TestNoHttpsRedirect) {
  // Add a redirect to an https url.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL(std::string("https://") + kAiaHost);
  auto redirect_head = network::mojom::URLResponseHead::New();
  redirect_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.push_back({redirect_info, std::move(redirect_head)});

  // Prime the loader to redirect to the https url.
  AddResponse(GetResponseHead(), kDummyCertContents, std::move(redirects));

  base::RunLoop run_loop;
  worker_thread(0)->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto delegate = delegate_factory()->CreateOCSPRequestSessionDelegate();

        std::unique_ptr<net::OCSPRequestSessionResult> result;
        {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync;
          result = delegate->StartAndWait(params());
        }

        ASSERT_FALSE(result);

        run_loop.Quit();
      }));
  run_loop.Run();

  // This test should have failed when the redirect occurred, so a URLLoader
  // must have been created already in order to even see the redirect.
  EXPECT_LT(0, num_loaders_created());
}

TEST_F(OCSPRequestSessionDelegateURLLoaderTest, TestSuccessfulLoad) {
  // Prime the loader to respond with our dummy cert contents.
  AddResponse(GetResponseHead(), kDummyCertContents);

  std::unique_ptr<net::OCSPRequestSessionResult> result;

  base::RunLoop run_loop;
  worker_thread(0)->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto delegate = delegate_factory()->CreateOCSPRequestSessionDelegate();

        {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync;
          result = delegate->StartAndWait(params());

          run_loop.Quit();
        }
      }));
  run_loop.Run();

  // We should have seen at least one loader created.
  EXPECT_LT(0, num_loaders_created());

  ASSERT_TRUE(result);

  // Test that we received the correct response.
  EXPECT_EQ(result->response_code, 200);
  EXPECT_EQ(result->response_content_type, "application/pkix-cert");
  EXPECT_EQ(result->data, kDummyCertContents);
}

TEST_F(OCSPRequestSessionDelegateURLLoaderTest,
       TestSimultaneousDelegateFactory) {
  // Prime the loader to respond with our dummy cert contents.
  AddResponse(GetResponseHead(), kDummyCertContents);

  constexpr int num_simultaneous = 5;
  std::vector<std::unique_ptr<net::OCSPRequestSessionResult>> results(
      num_simultaneous);

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(num_simultaneous, run_loop.QuitClosure());

  base::Lock delegate_factory_lock;

  // Tell all the worker threads to create a delegate and call StartAndWait().
  for (int i = 0; i < num_simultaneous; i++) {
    worker_thread(i)->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&, i]() {
          scoped_refptr<net::OCSPRequestSessionDelegate> delegate;
          {
            base::AutoLock autolock(delegate_factory_lock);
            delegate = delegate_factory()->CreateOCSPRequestSessionDelegate();
          }

          {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync;
            // TODO(crbug.com/1038867): remove once this is resolved.
            base::ScopedBlockingCall scoped_blocking_call(
                FROM_HERE, base::BlockingType::WILL_BLOCK);

            results[i] = delegate->StartAndWait(params());
          }

          barrier_closure.Run();
        }));
  }

  // Wait for all the delegates for return from StartAndWait().
  run_loop.Run();

  // We should have seen at least |num_simultaneous| loaders created.
  EXPECT_LE(num_simultaneous, num_loaders_created());

  for (int i = 0; i < num_simultaneous; i++) {
    ASSERT_TRUE(results[i]);

    // Test that we received the correct response.
    EXPECT_EQ(results[i]->response_code, 200);
    EXPECT_EQ(results[i]->response_content_type, "application/pkix-cert");
    EXPECT_EQ(results[i]->data, kDummyCertContents);
  }
}

// Test that we can delete the delegate factory and its associated
// TestURLLoaderFactory while the delegates are running.
TEST_F(OCSPRequestSessionDelegateURLLoaderTest, TestDelegateFactoryDeletion) {
  // Prime the loader to respond with our dummy cert contents.
  AddResponse(GetResponseHead(), kDummyCertContents);

  constexpr int num_simultaneous = 10;
  std::vector<std::unique_ptr<net::OCSPRequestSessionResult>> results(
      num_simultaneous);

  // Quit() when at least one URLLoader has been created.
  base::RunLoop run_loop1;
  AddTemporaryInterceptor(base::BindRepeating(
      [](base::RepeatingClosure quit_closure,
         const network::ResourceRequest& request) { quit_closure.Run(); },
      run_loop1.QuitClosure()));

  // Quit() when each worker has either returned from delegate->WaitForResult()
  // or will not start,
  base::RunLoop run_loop2;
  base::RepeatingClosure barrier_closure2 =
      base::BarrierClosure(num_simultaneous, run_loop2.QuitClosure());

  base::Lock delegate_factory_lock;

  // Tell all the worker threads to create a delegate and call StartAndWait().
  for (int i = 0; i < num_simultaneous; i++) {
    worker_thread(i)->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&, i]() {
          scoped_refptr<net::OCSPRequestSessionDelegate> delegate;
          {
            base::AutoLock autolock(delegate_factory_lock);
            if (delegate_factory())
              delegate = delegate_factory()->CreateOCSPRequestSessionDelegate();
          }

          if (delegate) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync;
            // TODO(crbug.com/1038867): remove once this is resolved.
            base::ScopedBlockingCall scoped_blocking_call(
                FROM_HERE, base::BlockingType::WILL_BLOCK);

            results[i] = delegate->StartAndWait(params());
          }

          barrier_closure2.Run();
        }));
  }

  // Run until at least one URLLoader has been created.
  run_loop1.Run();

  {
    base::AutoLock autolock(delegate_factory_lock);
    // Should be okay to release the delegate factory at any time during
    // execution.
    ResetDelegateFactory();
  }

  // Once the delegate factory has been deleted, it should be okay to delete the
  // TestURLLoaderFactory being used.
  ResetLoaderFactory();

  run_loop2.Run();

  // Check that any results we received are actually correct.
  for (int i = 0; i < num_simultaneous; i++) {
    if (!results[i])
      continue;

    // Test that we received the correct response.
    EXPECT_EQ(results[i]->response_code, 200);
    EXPECT_EQ(results[i]->response_content_type, "application/pkix-cert");
    EXPECT_EQ(results[i]->data, kDummyCertContents);
  }
}

class NssHttpURLLoaderTest : public net::TestWithTaskEnvironment {
 public:
  NssHttpURLLoaderTest()
      : verify_proc_(new net::CertVerifyProcNSS),
        verifier_(new net::MultiThreadedCertVerifier(verify_proc_.get())) {}
  ~NssHttpURLLoaderTest() override = default;

  void SetUp() override {
    loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [this](const network::ResourceRequest& request) {
          EXPECT_EQ(request.url, intercept_url_);
          EXPECT_TRUE(request.trusted_params.has_value());
          EXPECT_TRUE(request.trusted_params->disable_secure_dns);
          num_loaders_created_++;
        }));

    net::SetOCSPRequestSessionDelegateFactory(
        std::make_unique<OCSPRequestSessionDelegateFactoryURLLoader>(
            base::SequencedTaskRunnerHandle::Get(),
            loader_factory_.GetSafeWeakWrapper()));

    test_cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "aia-cert.pem");
    ASSERT_TRUE(test_cert_.get());

    test_root_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "aia-root.pem");
    ASSERT_TRUE(test_root_.get());

    scoped_root_.Reset({test_root_.get()});
  }

  void TearDown() override {
    net::SetOCSPRequestSessionDelegateFactory(nullptr);
  }

  net::CertVerifier* verifier() const { return verifier_.get(); }

  int num_loaders_created() const { return num_loaders_created_; }

  scoped_refptr<net::X509Certificate> test_cert() const { return test_cert_; }

 protected:
  // Primes |loader_factory_| to respond with |head| and |file_contents| to
  // |intercept_url_|.
  void AddResponse(network::mojom::URLResponseHeadPtr head,
                   std::string file_contents) {
    loader_factory_.AddResponse(intercept_url_, std::move(head),
                                std::move(file_contents),
                                network::URLLoaderCompletionStatus());
  }

 private:
  const GURL intercept_url_{std::string("http://") + kAiaHost};
  scoped_refptr<net::X509Certificate> test_root_;
  net::ScopedTestRoot scoped_root_;
  scoped_refptr<net::X509Certificate> test_cert_;

  network::TestURLLoaderFactory loader_factory_;
  int num_loaders_created_ = 0;

  scoped_refptr<net::CertVerifyProc> verify_proc_;
  std::unique_ptr<net::CertVerifier> verifier_;
};

// Tests that when using NSS to verify certificates that a request to fetch
// missing intermediate certificates is made successfully.
TEST_F(NssHttpURLLoaderTest, TestAia) {
  // Prime |loader_factory_| to return the intermediate cert.
  std::string cert_contents;
  GetIntermediateCertContents(&cert_contents);
  AddResponse(GetResponseHead(), std::move(cert_contents));

  net::CertVerifyResult verify_result;
  net::TestCompletionCallback test_callback;
  std::unique_ptr<net::CertVerifier::Request> request;

  int flags = 0;
  int error = verifier()->Verify(
      net::CertVerifier::RequestParams(test_cert(), "aia-host.invalid", flags,
                                       /*ocsp_response=*/std::string(),
                                       /*sct_list=*/std::string()),
      &verify_result, test_callback.callback(), &request,
      net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));

  error = test_callback.WaitForResult();

  EXPECT_THAT(error, IsOk());

  // Ensure that NSS made an AIA request for the missing intermediate.
  EXPECT_LT(0, num_loaders_created());
}

}  // namespace cert_verifier
