// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TACHYON_CONSTANTS_H_
#define CHROME_BROWSER_NEARBY_SHARING_TACHYON_CONSTANTS_H_

#if defined(NDEBUG)
const char kTachyonReceiveMessageAPI[] =
    "https://tachyon-playground-autopush-grpc.sandbox.googleapis.com/v1/"
    "messages:receiveExpress";

const char kTachyonSendMessageAPI[] =
    "https://tachyon-playground-autopush-grpc.sandbox.googleapis.com/v1/"
    "message:sendExpress";
#else
const char kTachyonReceiveMessageAPI[] =
    "https://instantmessaging-pa.googleapis.com/v1/messages:receiveExpress";

const char kTachyonSendMessageAPI[] =
    "https://instantmessaging-pa.googleapis.com/v1/message:sendExpress";
#endif  // defined(NDEBUG)

// Template for optional OAuth2 authorization HTTP header.
const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";

#endif  // CHROME_BROWSER_NEARBY_SHARING_TACHYON_CONSTANTS_H_
