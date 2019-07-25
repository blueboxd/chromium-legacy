// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_SERVICE_H_
#define CONTENT_BROWSER_SMS_SMS_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/sms/sms_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;
class SmsDialog;

// SmsService handles mojo connections from the renderer, observing the incoming
// SMS messages from an SmsProvider.
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing FrameServiceBase, which observes the lifecycle of a
// RenderFrameHost and manages it own memory.
// Create() creates a self-managed instance of SmsService and binds it to the
// request.
class CONTENT_EXPORT SmsService
    : public FrameServiceBase<blink::mojom::SmsReceiver>,
      public content::SmsProvider::Observer {
 public:
  static void Create(SmsProvider*,
                     RenderFrameHost*,
                     blink::mojom::SmsReceiverRequest);

  SmsService(SmsProvider*, RenderFrameHost*, blink::mojom::SmsReceiverRequest);
  SmsService(SmsProvider*,
             const url::Origin&,
             RenderFrameHost*,
             blink::mojom::SmsReceiverRequest);
  ~SmsService() override;

  // content::SmsProvider::Observer:
  bool OnReceive(const url::Origin&, const std::string& message) override;

  // blink::mojom::SmsReceiver:
  void Receive(base::TimeDelta timeout, ReceiveCallback) override;

 private:
  void Process(blink::mojom::SmsStatus, base::Optional<std::string> sms);
  // Shows/Dismisses the dialog.
  void Prompt();
  void Dismiss();

  // Callback when the |timer_| times out.
  void OnTimeout();
  // Callback when the user manually clicks 'Continue' button.
  void OnContinue();
  // Callback when the user manually dismisses the dialog.
  void OnCancel();

  // |sms_provider_| is safe because all instances of SmsProvider are owned
  // by a SmsKeyedService, which is owned by a Profile, which transitively
  // owns SmsServices.
  SmsProvider* sms_provider_;

  // The currently opened sms dialog.
  std::unique_ptr<SmsDialog> prompt_;

  const url::Origin origin_;

  base::OneShotTimer timer_;
  ReceiveCallback callback_;
  base::Optional<std::string> sms_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(SmsService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_SERVICE_H_
