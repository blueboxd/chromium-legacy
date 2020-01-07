// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_NSS_OCSP_SESSION_URL_LOADER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_NSS_OCSP_SESSION_URL_LOADER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert_net/nss_ocsp.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace cert_verifier {

class OCSPRequestSessionDelegateFactoryURLLoader;

// Implementation of net::OCSPRequestSessionDelegate that uses a
// SharedURLLoaderFactory to perform loads via the network service.
class COMPONENT_EXPORT(CERT_VERIFIER_CPP) OCSPRequestSessionDelegateURLLoader
    : public net::OCSPRequestSessionDelegate {
 public:
  // Creates a new Delegate that will use |loader_factory| to
  // load URLs as needed. Loading requests will be dispatched and processed
  // on |load_task_runner|, as StartAndWait() will block the thread/task
  // runner it is called on.
  // |delegate_factory| should be bound to |load_task_runner| and will be used
  // to access |delegate_factory| and perform loads.
  OCSPRequestSessionDelegateURLLoader(
      scoped_refptr<base::SequencedTaskRunner> load_task_runner,
      base::WeakPtr<OCSPRequestSessionDelegateFactoryURLLoader>
          delegate_factory);

  // OCSPRequestSessionDelegate overrides:
  std::unique_ptr<net::OCSPRequestSessionResult> StartAndWait(
      const net::OCSPRequestSessionParams* params) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OCSPRequestSessionDelegateURLLoaderTest,
                           TestTimeout);

  ~OCSPRequestSessionDelegateURLLoader() override;

  // Posts a call to StartLoad() to |load_task_runner|, but does not
  // wait for that load to be completed.
  void Start(const net::OCSPRequestSessionParams* params);

  // Blocks the current thread until the load previously started by
  // Start() completes, returning the result. If Start() has not been
  // called, this will block indefinitely.
  std::unique_ptr<net::OCSPRequestSessionResult> WaitForResult();

  // Invoked on |load_task_runner_|. Will bind |factory_pending_remote_| to the
  // current sequence and instantiate a SimpleURLLoader to load from the
  // network.
  void StartLoad(const net::OCSPRequestSessionParams* params);

  void OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const network::mojom::URLResponseHead& response_head,
                          std::vector<std::string>* removed_headers);

  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);

  void OnUrlLoaderCompleted(std::unique_ptr<std::string> response_body);

  // May delete |this|.
  void CancelLoad();

  // May delete |this|.
  void FinishLoad();

  const scoped_refptr<base::SequencedTaskRunner> load_task_runner_;

  // Accessed only on |load_task_runner_|.
  const base::WeakPtr<OCSPRequestSessionDelegateFactoryURLLoader>
      delegate_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  std::unique_ptr<net::OCSPRequestSessionResult> result_;
  base::WaitableEvent wait_event_;
};

// An implementation of net::OCSPRequestSessionDelegateFactory that takes a
// SharedURLLoaderFactory, and the sequence it's bound to, and will vend
// net::OCSPRequestSessionDelegate's that use the provided
// SharedURLLoaderFactory to load from the network.
class COMPONENT_EXPORT(CERT_VERIFIER_CPP)
    OCSPRequestSessionDelegateFactoryURLLoader
    : public net::OCSPRequestSessionDelegateFactory {
 public:
  // |loader_factory_sequence| should be the sequence that |loader_factory| is
  // bound to. Tasks will be posted to the |loader_factory_sequence| that make
  // use of |loader_factory|.
  // When the delegate factory is destroyed, vended delegates may finish their
  // loads or may return early without a result.
  OCSPRequestSessionDelegateFactoryURLLoader(
      scoped_refptr<base::SequencedTaskRunner> loader_factory_sequence,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);

  // Returns a scoped_refptr<net::OCSPRequestSessionDelegateURLLoader>.
  scoped_refptr<net::OCSPRequestSessionDelegate>
  CreateOCSPRequestSessionDelegate() override;

  ~OCSPRequestSessionDelegateFactoryURLLoader() override;

 private:
  // The delegate is a friend so that it can call GetSharedURLLoaderFactory().
  friend OCSPRequestSessionDelegateURLLoader;

  // Must be invoked on |loader_factory_sequence|. Gets the
  // SharedURLLoaderFactory passed in the constructor.
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      const {
    return loader_factory_;
  }

  // Sequence that |loader_factory_| is bound to. Use to run the
  // SimpleURLLoaders.
  const scoped_refptr<base::SequencedTaskRunner> loader_factory_sequence_;
  // SharedURLLoaderFactory to use for network loads.
  const scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  // Holds a weak ptr to |this| and bound to |loader_factory_sequence_|.
  base::WeakPtr<OCSPRequestSessionDelegateFactoryURLLoader> weak_ptr_;

  base::WeakPtrFactory<OCSPRequestSessionDelegateFactoryURLLoader>
      weak_factory_;
};

}  // namespace cert_verifier

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_NSS_OCSP_SESSION_URL_LOADER_H_
