// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hsts_query.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

// Helper since a once-callback may need to be called from two paths.
class HSTSCallbackHelper : public base::RefCounted<HSTSCallbackHelper> {
 public:
  HSTSCallbackHelper(HSTSCallback user_callback)
      : user_callback_(std::move(user_callback)) {}

  void ReportResult(bool result) {
    std::move(user_callback_).Run(result ? HSTSResult::kYes : HSTSResult::kNo);
  }

  void ReportError() { std::move(user_callback_).Run(HSTSResult::kError); }

 private:
  friend class base::RefCounted<HSTSCallbackHelper>;
  ~HSTSCallbackHelper() = default;

  HSTSCallback user_callback_;

  DISALLOW_COPY_AND_ASSIGN(HSTSCallbackHelper);
};

}  // namespace

void PostHSTSQueryForHostAndNetworkContext(
    const url::Origin& origin,
    network::mojom::NetworkContext* network_context,
    HSTSCallback callback) {
  if (origin.opaque()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HSTSResult::kNo));
    return;
  }

  if (!network_context) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HSTSResult::kError));
    return;
  }

  scoped_refptr<HSTSCallbackHelper> callback_helper =
      base::MakeRefCounted<HSTSCallbackHelper>(std::move(callback));
  network_context->IsHSTSActiveForHost(
      origin.host(),
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(&HSTSCallbackHelper::ReportResult, callback_helper),
          base::BindOnce(&HSTSCallbackHelper::ReportError, callback_helper)));
}

}  // namespace password_manager
