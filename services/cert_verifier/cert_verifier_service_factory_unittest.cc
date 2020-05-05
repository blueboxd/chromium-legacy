// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service_factory.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace cert_verifier {
namespace {

const char kMockURL[] = "http://mock.invalid/";

class TestReconnector : public mojom::URLLoaderFactoryConnector {
 public:
  explicit TestReconnector(
      network::TestURLLoaderFactory* test_url_loader_factory)
      : test_url_loader_factory_(test_url_loader_factory) {}
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          url_loader_factory_receiver) override {
    num_connects_++;
    receiver_set_.Add(test_url_loader_factory_,
                      std::move(url_loader_factory_receiver));
  }

  void DisconnectAll() { receiver_set_.Clear(); }

  size_t num_connects() const { return num_connects_; }

 private:
  network::TestURLLoaderFactory* test_url_loader_factory_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receiver_set_;

  size_t num_connects_ = 0;
};

std::unique_ptr<net::CertNetFetcher::Request>
PerformCertNetFetcherRequestOnTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> runner,
    scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher) {
  std::unique_ptr<net::CertNetFetcher::Request> request;
  base::RunLoop run_loop;
  runner->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                     request = cert_net_fetcher->FetchCaIssuers(
                         GURL(kMockURL), net::CertNetFetcher::DEFAULT,
                         net::CertNetFetcher::DEFAULT);
                     run_loop.QuitWhenIdle();
                   }));
  run_loop.Run();
  return request;
}

class CertVerifierServiceFactoryTest : public PlatformTest {
 public:
  CertVerifierServiceFactoryTest()
      : test_reconnector_(&test_url_loader_factory_),
        test_reconnector_receiver_(&test_reconnector_) {
    test_reconnector_.CreateURLLoaderFactory(
        test_url_loader_factory_remote_.InitWithNewPipeAndPassReceiver());
    EXPECT_EQ(test_reconnector_.num_connects(), 1ul);
  }

  TestReconnector* test_reconnector() { return &test_reconnector_; }
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  TakeURLLoaderFactoryPendingRemote() {
    return std::move(test_url_loader_factory_remote_);
  }
  mojo::Receiver<mojom::URLLoaderFactoryConnector>&
  test_reconnector_receiver() {
    return test_reconnector_receiver_;
  }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  // Instantiate a TestURLLoaderFactory which we can use to respond and unpause
  // network requests.
  network::TestURLLoaderFactory test_url_loader_factory_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      test_url_loader_factory_remote_;

  // Instantiate a dummy URLLoaderFactoryConnector that can disconnect at will
  // and will automatically reconnect to the |test_url_loader_factory| above.
  TestReconnector test_reconnector_;
  mojo::Receiver<mojom::URLLoaderFactoryConnector> test_reconnector_receiver_;
};

struct DummyCVServiceRequest : public mojom::CertVerifierRequest {
  explicit DummyCVServiceRequest(base::RepeatingClosure on_finish)
      : on_finish_(std::move(on_finish)) {}
  void Complete(const net::CertVerifyResult& result_param,
                int32_t net_error_param) override {
    is_completed = true;
    result = result_param;
    net_error = net_error_param;
    std::move(on_finish_).Run();
  }

  base::RepeatingClosure on_finish_;
  bool is_completed = false;
  net::CertVerifyResult result;
  int net_error;
};
}  // namespace

// Test that the CertNetFetcherURLLoader handed to the CertVerifierService
// successfully tries to reconnect after URLLoaderFactory disconnection.
TEST_F(CertVerifierServiceFactoryTest,
       CertNetFetcherReconnectAfterURLLoaderFactoryReset) {
  scoped_refptr<base::SequencedTaskRunner> caller_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  // Pass mojo::Remote's for |test_url_loader_factory| and
  // |test_reconnector| to the CertNetFetcherURLLoader
  // created by the CertVerifierServiceFactoryImpl.
  scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher =
      CertVerifierServiceFactoryImpl::CreateCertNetFetcher(
          TakeURLLoaderFactoryPendingRemote(),
          test_reconnector_receiver().BindNewPipeAndPassRemote());

  // Now run the test.
  auto request1 = PerformCertNetFetcherRequestOnTaskRunner(caller_task_runner,
                                                           cert_net_fetcher);
  EXPECT_EQ(test_url_loader_factory()->NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory()->IsPending(kMockURL));

  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      kMockURL, "", net::HTTP_NOT_FOUND));
  EXPECT_EQ(test_url_loader_factory()->NumPending(), 0);

  // Disconnect the test_url_loader_factory to test reconnection.
  test_reconnector()->DisconnectAll();

  // Now run the test.
  auto request2 = PerformCertNetFetcherRequestOnTaskRunner(caller_task_runner,
                                                           cert_net_fetcher);

  EXPECT_EQ(test_url_loader_factory()->NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory()->IsPending(kMockURL));

  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      kMockURL, "", net::HTTP_NOT_FOUND));
  EXPECT_EQ(test_reconnector()->num_connects(), 2ul);

  // Disconnect the test_url_loader_factory to test reconnection one more time.
  test_reconnector()->DisconnectAll();

  // Now run the test.
  auto request3 = PerformCertNetFetcherRequestOnTaskRunner(caller_task_runner,
                                                           cert_net_fetcher);

  EXPECT_EQ(test_url_loader_factory()->NumPending(), 1);
  EXPECT_TRUE(test_url_loader_factory()->IsPending(kMockURL));

  EXPECT_EQ(test_reconnector()->num_connects(), 3ul);

  // The test simulated responses for at least the first two requests, so they
  // should complete without hanging. The response content is not important, but
  // the test sent error responses, so check that net::OK wasn't returned.
  // The third request may have attached to a previous job, but cancelling it
  // should not cause problems.
  {
    net::Error error1, error2;
    std::vector<uint8_t> bytes1, bytes2;
    base::RunLoop run_loop;
    caller_task_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync;
          request1->WaitForResult(&error1, &bytes1);
          request2->WaitForResult(&error2, &bytes2);
          request3.reset();
          run_loop.Quit();
        }));
    run_loop.Run();
    EXPECT_NE(error1, net::OK);
    EXPECT_NE(error2, net::OK);
  }

  cert_net_fetcher->Shutdown();
}

TEST_F(CertVerifierServiceFactoryTest, GetNewCertVerifier) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> test_cert(
      net::ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(nullptr, test_cert.get());

  mojo::Remote<mojom::CertVerifierServiceFactory> cv_service_factory_remote;
  CertVerifierServiceFactoryImpl cv_service_factory_impl(
      cv_service_factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::CertVerifierService> cv_service_remote;
  network::mojom::CertVerifierCreationParamsPtr cv_creation_params =
      network::mojom::CertVerifierCreationParams::New();

  cv_service_factory_remote->GetNewCertVerifier(
      cv_service_remote.BindNewPipeAndPassReceiver(),
      TakeURLLoaderFactoryPendingRemote(),
      test_reconnector_receiver().BindNewPipeAndPassRemote(),
      std::move(cv_creation_params));

  base::RunLoop request_completed_run_loop;
  DummyCVServiceRequest dummy_cv_service_req(
      request_completed_run_loop.QuitClosure());
  mojo::Receiver<mojom::CertVerifierRequest> dummy_cv_service_req_receiver(
      &dummy_cv_service_req);

  cv_service_remote->Verify(
      net::CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                       /*ocsp_response=*/std::string(),
                                       /*sct_list=*/std::string()),
      dummy_cv_service_req_receiver.BindNewPipeAndPassRemote());

  request_completed_run_loop.Run();
  ASSERT_EQ(dummy_cv_service_req.net_error, net::ERR_CERT_AUTHORITY_INVALID);
  ASSERT_TRUE(dummy_cv_service_req.result.cert_status &
              net::CERT_STATUS_AUTHORITY_INVALID);
}

}  // namespace cert_verifier
