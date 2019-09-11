// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/host_address_request.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "jingle/glue/utils.h"
#include "third_party/blink/public/common/features.h"

namespace content {

P2PAsyncAddressResolver::P2PAsyncAddressResolver(
    P2PSocketDispatcher* dispatcher)
    : dispatcher_(dispatcher), state_(STATE_CREATED) {
  AddRef();  // Balanced in Destroy().
}

P2PAsyncAddressResolver::~P2PAsyncAddressResolver() {
  DCHECK(state_ == STATE_CREATED || state_ == STATE_FINISHED);
}

void P2PAsyncAddressResolver::Start(const rtc::SocketAddress& host_name,
                                    DoneCallback done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(STATE_CREATED, state_);

  state_ = STATE_SENT;
  done_callback_ = std::move(done_callback);
  bool enable_mdns = base::FeatureList::IsEnabled(
      blink::features::kWebRtcHideLocalIpsWithMdns);
  dispatcher_->GetP2PSocketManager()->get()->GetHostAddress(
      host_name.hostname(), enable_mdns,
      base::BindOnce(&P2PAsyncAddressResolver::OnResponse,
                     base::Unretained(this)));
}

void P2PAsyncAddressResolver::Cancel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != STATE_FINISHED)
    state_ = STATE_FINISHED;

  done_callback_.Reset();
}

void P2PAsyncAddressResolver::OnResponse(const net::IPAddressList& addresses) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == STATE_SENT) {
    state_ = STATE_FINISHED;
    std::move(done_callback_).Run(addresses);
  }
}

}  // namespace content
