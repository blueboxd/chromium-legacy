// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <SecurityInterface/SFCertificatePanel.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#include "chrome/browser/certificate_viewer.h"
#include "components/remote_cocoa/browser/window.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_ios_and_mac.h"
#include "net/cert/x509_util_mac.h"

@interface SSLCertificateViewerMac : NSObject
// Initializes |certificates_| with the certificate chain for a given
// certificate.
- (instancetype)initWithCertificate:(net::X509Certificate*)certificate;

// Shows the certificate viewer as a Cocoa sheet.
- (void)showCertificateSheet:(NSWindow*)window;

// Called when the certificate viewer Cocoa sheet is closed.
- (void)sheetDidEnd:(NSWindow*)parent
         returnCode:(NSInteger)returnCode
            context:(void*)context;
@end

@implementation SSLCertificateViewerMac {
  base::scoped_nsobject<NSArray> _certificates;
  base::scoped_nsobject<SFCertificatePanel> _panel;
  NSWindow* _remoteViewsCloneWindow;
}

- (instancetype)initWithCertificate:(net::X509Certificate*)certificate {
  if ((self = [super init])) {
    base::ScopedCFTypeRef<CFArrayRef> certChain(
        net::x509_util::CreateSecCertificateArrayForX509Certificate(
            certificate));
    if (!certChain)
      return self;
    NSArray* certificates = base::mac::CFToNSCast(certChain.get());
    _certificates.reset([certificates retain]);
  }

  // Explicitly disable revocation checking, regardless of user preferences
  // or system settings. The behaviour of SFCertificatePanel is to call
  // SecTrustEvaluate on the certificate(s) supplied, effectively
  // duplicating the behaviour of net::X509Certificate::Verify(). However,
  // this call stalls the UI if revocation checking is enabled in the
  // Keychain preferences or if the cert may be an EV cert. By disabling
  // revocation checking, the stall is limited to the time taken for path
  // building and verification, which should be minimized due to the path
  // being provided in |certificates|. This does not affect normal
  // revocation checking from happening, which is controlled by
  // net::X509Certificate::Verify() and user preferences, but will prevent
  // the certificate viewer UI from displaying which certificate is revoked.
  // This is acceptable, as certificate revocation will still be shown in
  // the page info bubble if a certificate in the chain is actually revoked.
  base::ScopedCFTypeRef<CFMutableArrayRef> policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!policies.get()) {
    NOTREACHED();
    return self;
  }
  // Add a basic X.509 policy, in order to match the behaviour of
  // SFCertificatePanel when no policies are specified.
  base::ScopedCFTypeRef<SecPolicyRef> basicPolicy;
  OSStatus status =
      net::x509_util::CreateBasicX509Policy(basicPolicy.InitializeInto());
  if (status != noErr) {
    NOTREACHED();
    return self;
  }
  CFArrayAppendValue(policies, basicPolicy.get());

  status = net::x509_util::CreateRevocationPolicies(false, policies);
  if (status != noErr) {
    NOTREACHED();
    return self;
  }

  _panel.reset([[SFCertificatePanel alloc] init]);
  [_panel setPolicies:base::mac::CFToNSCast(policies.get())];
  return self;
}

- (void)showCertificateSheet:(NSWindow*)window {
  // TODO(https://crbug.com/913303): The certificate viewer's interface to
  // Cocoa should be wrapped in a mojo interface in order to allow
  // instantiating across processes. As a temporary solution, create a
  // transparent in-process window to the front.
  if (remote_cocoa::IsWindowRemote(window)) {
    _remoteViewsCloneWindow =
        remote_cocoa::CreateInProcessTransparentClone(window);
    window = _remoteViewsCloneWindow;
  }
  [_panel beginSheetForWindow:window
                modalDelegate:self
               didEndSelector:@selector(sheetDidEnd:returnCode:context:)
                  contextInfo:nil
                 certificates:_certificates
                    showGroup:YES];
}

- (void)sheetDidEnd:(NSWindow*)parent
         returnCode:(NSInteger)returnCode
            context:(void*)context {
  [_remoteViewsCloneWindow close];
  _remoteViewsCloneWindow = nil;
  _panel.reset();
}

@end

void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  base::scoped_nsobject<SSLCertificateViewerMac> viewer(
      [[SSLCertificateViewerMac alloc] initWithCertificate:cert]);
  [viewer showCertificateSheet:parent.GetNativeNSWindow()];
}
