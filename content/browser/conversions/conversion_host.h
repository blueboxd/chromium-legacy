// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_H_

#include "base/gtest_prod_util.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"

namespace content {

class ConversionManager;
class RenderFrameHost;
class WebContents;

// Class responsible for listening to conversion events originating from blink,
// and verifying that they are valid. Owned by the WebContents. Lifetime is
// bound to lifetime of the WebContents.
class CONTENT_EXPORT ConversionHost : public blink::mojom::ConversionHost {
 public:
  explicit ConversionHost(WebContents* web_contents);
  ConversionHost(const ConversionHost& other) = delete;
  ConversionHost& operator=(const ConversionHost& other) = delete;
  ~ConversionHost() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest, ConversionInSubframe_BadMessage);
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest,
                           ConversionOnInsecurePage_BadMessage);
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest,
                           ConversionWithInsecureReportingOrigin_BadMessage);
  FRIEND_TEST_ALL_PREFIXES(ConversionHostTest, ValidConversion_NoBadMessage);

  // blink::mojom::ConversionHost:
  void RegisterConversion(blink::mojom::ConversionPtr conversion) override;

  // Sets the target frame on |receiver_|.
  void SetCurrentTargetFrameForTesting(RenderFrameHost* render_frame_host);

  // Gets the manager for this web contents. Can be null.
  ConversionManager* GetManager();

  WebContents* web_contents_;

  WebContentsFrameReceiverSet<blink::mojom::ConversionHost> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_HOST_H_
