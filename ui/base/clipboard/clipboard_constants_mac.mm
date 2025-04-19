// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_constants.h"

#import <Foundation/Foundation.h>

namespace ui {

NSString* const kUTTypeChromiumImageAndHTML = @"org.chromium.image-html";

NSString* const kUTTypeChromiumInitiatedDrag =
    @"org.chromium.chromium-initiated-drag";

NSString* const kUTTypeChromiumPrivilegedInitiatedDrag =
    @"org.chromium.chromium-privileged-initiated-drag";

NSString* const kUTTypeChromiumRendererInitiatedDrag =
    @"org.chromium.chromium-renderer-initiated-drag";

NSString* const kUTTypeChromiumDataTransferCustomData =
    @"org.chromium.web-custom-data";

NSString* const kImageSvg = @"public.svg-image";
// TODO(dcheng): This name is temporary. See crbug.com/106449.
NSString* const kWebCustomDataPboardType = @"org.chromium.web-custom-data";
NSString* const kWebSmartPastePboardType = @"NeXT smart paste pasteboard type";

// It is the common convention on the Mac and on iOS that password managers tag
// confidential data with the flavor "org.nspasteboard.ConcealedType". Obey this
// convention. See http://nspasteboard.org/ for more info.
NSString* const kUTTypeConfidentialData = @"org.nspasteboard.ConcealedType";

NSString* const kUTTypeChromiumSourceURL = @"org.chromium.source-url";

}  // namespace ui
