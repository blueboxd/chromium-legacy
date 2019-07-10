// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_hints/browser/network_hints_message_filter.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/network_hints/common/network_hints_common.h"
#include "components/network_hints/common/network_hints_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace network_hints {

namespace {

const int kDefaultPort = 80;

// This class contains a std::unique_ptr of itself, it is passed in through
// Start() method, and will be freed by the OnComplete() method when resolving
// has completed or mojo connection error has happened.
class DnsLookupRequest : public network::ResolveHostClientBase {
 public:
  DnsLookupRequest(int render_process_id, const std::string& hostname)
      : binding_(this),
        render_process_id_(render_process_id),
        hostname_(hostname) {}

  // Return underlying network resolver status.
  // net::OK ==> Host was found synchronously.
  // net:ERR_IO_PENDING ==> Network will callback later with result.
  // anything else ==> Host was not found synchronously.
  void Start(std::unique_ptr<DnsLookupRequest> request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    request_ = std::move(request);

    content::RenderProcessHost* render_process_host =
        content::RenderProcessHost::FromID(render_process_id_);
    if (!render_process_host) {
      OnComplete(net::ERR_FAILED, base::nullopt);
      return;
    }

    DCHECK(!binding_);
    network::mojom::ResolveHostClientPtr client_ptr;
    binding_.Bind(mojo::MakeRequest(&client_ptr));
    binding_.set_connection_error_handler(
        base::BindOnce(&DnsLookupRequest::OnComplete, base::Unretained(this),
                       net::ERR_FAILED, base::nullopt));
    net::HostPortPair host_port_pair(hostname_, kDefaultPort);
    network::mojom::ResolveHostParametersPtr resolve_host_parameters =
        network::mojom::ResolveHostParameters::New();
    // Lets the host resolver know it can be de-prioritized.
    resolve_host_parameters->initial_priority = net::RequestPriority::IDLE;
    // Make a note that this is a speculative resolve request. This allows
    // separating it from real navigations in the observer's callback.
    resolve_host_parameters->is_speculative = true;
    render_process_host->GetStoragePartition()
        ->GetNetworkContext()
        ->ResolveHost(host_port_pair, std::move(resolve_host_parameters),
                      std::move(client_ptr));
  }

 private:
  // network::mojom::ResolveHostClient:
  void OnComplete(
      int result,
      const base::Optional<net::AddressList>& resolved_addresses) override {
    VLOG(2) << __FUNCTION__ << ": " << hostname_ << ", result=" << result;
    request_.reset();
  }

  mojo::Binding<network::mojom::ResolveHostClient> binding_;
  int render_process_id_;
  const std::string hostname_;
  std::unique_ptr<DnsLookupRequest> request_;

  DISALLOW_COPY_AND_ASSIGN(DnsLookupRequest);
};

}  // namespace

NetworkHintsMessageFilter::NetworkHintsMessageFilter(int render_process_id)
    : content::BrowserMessageFilter(NetworkHintsMsgStart),
      render_process_id_(render_process_id) {}

NetworkHintsMessageFilter::~NetworkHintsMessageFilter() {
}

bool NetworkHintsMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NetworkHintsMessageFilter, message)
    IPC_MESSAGE_HANDLER(NetworkHintsMsg_DNSPrefetch, OnDnsPrefetch)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NetworkHintsMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  if (message.type() == NetworkHintsMsg_DNSPrefetch::ID)
    *thread = content::BrowserThread::UI;
}

void NetworkHintsMessageFilter::OnDnsPrefetch(
    const LookupRequest& lookup_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const std::string& hostname : lookup_request.hostname_list) {
    std::unique_ptr<DnsLookupRequest> request =
        std::make_unique<DnsLookupRequest>(render_process_id_, hostname);
    DnsLookupRequest* request_ptr = request.get();
    request_ptr->Start(std::move(request));
  }
}

}  // namespace network_hints
