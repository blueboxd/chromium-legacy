// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_STREAMING_CONNECTION_ESTABLISHER_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_STREAMING_CONNECTION_ESTABLISHER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/chromeos/android_sms/connection_establisher.h"

namespace chromeos {

namespace android_sms {

// Concrete ConnectionEstablisher implementation that initiates a background
// connection between the Android Messages for Web service worker and the
// Tachyon server by dispatching a known message string to it.
//
// To allow the service worker to continue running past it's default timeout
// this message is sent using a special long-running dispatch.
class StreamingConnectionEstablisher : public ConnectionEstablisher {
 public:
  explicit StreamingConnectionEstablisher(base::Clock* clock);
  ~StreamingConnectionEstablisher() override;

  void EstablishConnection(
      const GURL& url,
      ConnectionMode connection_mode,
      content::ServiceWorkerContext* service_worker_context) override;

  void TearDownConnection(
      const GURL& url,
      content::ServiceWorkerContext* service_worker_context) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(StreamingConnectionEstablisherTest,
                           EstablishConnection);

  void SendStartStreamingMessageIfNotConnected(
      const GURL& url,
      ConnectionMode connection_mode,
      content::ServiceWorkerContext* service_worker_context);

  void OnMessageDispatchResult(bool status);

  static const char kStartStreamingMessage[];
  static const char kResumeStreamingMessage[];
  base::Clock* clock_;
  bool is_connected_ = false;
  base::Time start_connection_message_time_;
  base::WeakPtrFactory<StreamingConnectionEstablisher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StreamingConnectionEstablisher);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_STREAMING_CONNECTION_ESTABLISHER_H_
