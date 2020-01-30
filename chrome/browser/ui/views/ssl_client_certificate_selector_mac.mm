// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ssl_client_certificate_selector_mac.h"

#import <Cocoa/Cocoa.h>
#import <SecurityInterface/SFChooseIdentityPanel.h>
#include <objc/runtime.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ssl/ssl_client_auth_observer.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ui/views/certificate_selector.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_mac.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_platform_key_mac.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface SFChooseIdentityPanel (SystemPrivate)
// A system-private interface that dismisses a panel whose sheet was started by
// -beginSheetForWindow:modalDelegate:didEndSelector:contextInfo:identities:message:
// as though the user clicked the button identified by returnCode. Verified
// present in 10.5 through 10.12.
- (void)_dismissWithCode:(NSInteger)code;
@end

namespace {

class SSLClientCertificateSelectorDelegate;

// These Clear[Window]TableViewDataSources... functions help work around a bug
// in macOS where SFChooseIdentityPanel leaks a window and some views, including
// an NSTableView. Future events may make cause the table view to query its
// dataSource, which will have been deallocated.
//
// Note that this was originally thought to be 10.12+ but this reliably crashes
// on 10.11 (says avi@).
//
// Linking against the 10.12 SDK does not "fix" this issue, since
// NSTableView.dataSource is a "weak" reference, which in non-ARC land still
// translates to "raw pointer".
//
// See https://crbug.com/653093, https://crbug.com/750242 and rdar://29409207
// for more information.

void ClearTableViewDataSources(NSView* view) {
  if (auto table_view = base::mac::ObjCCast<NSTableView>(view)) {
    table_view.dataSource = nil;
  } else {
    for (NSView* subview in view.subviews) {
      ClearTableViewDataSources(subview);
    }
  }
}

void ClearWindowTableViewDataSources(NSWindow* window) {
  ClearTableViewDataSources(window.contentView);
}

}  // namespace

// This is the main class that runs the certificate selector panel. It's in
// Objective-C mainly because the only way to get a result out of that panel is
// a callback of a target/selector pair.
@interface SSLClientCertificateSelectorMac : NSObject

- (instancetype)
    initWithClientCerts:(net::ClientCertIdentityList)clientCerts
               delegate:(base::WeakPtr<SSLClientCertificateSelectorDelegate>)
                            delegate;

- (void)showSheetForWindow:(NSWindow*)window;

- (void)closeSelectorSheetWithCode:(NSModalResponse)response;

@end

// A testing helper object to run a OnceClosure when deallocated. Attach it as
// an associated object to test for deallocation of an object without
// subclassing.
@interface DeallocClosureCaller : NSObject

- (instancetype)initWithDeallocClosure:(base::OnceClosure)deallocClosure;

@end

@implementation DeallocClosureCaller {
  base::OnceClosure _deallocClosure;
}

- (instancetype)initWithDeallocClosure:(base::OnceClosure)deallocClosure {
  if ((self = [super init])) {
    _deallocClosure = std::move(deallocClosure);
  }
  return self;
}

- (void)dealloc {
  std::move(_deallocClosure).Run();
  [super dealloc];
}

@end

namespace {

// A fully transparent, borderless web-modal dialog used to display the
// OS-provided client certificate selector.
class SSLClientCertificateSelectorDelegate
    : public views::WidgetDelegateView,
      public chrome::OkAndCancelableForTesting,
      public SSLClientAuthObserver {
 public:
  SSLClientCertificateSelectorDelegate(
      content::WebContents* contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate)
      : SSLClientAuthObserver(contents->GetBrowserContext(),
                              cert_request_info,
                              std::move(delegate)) {
    StartObserving();
    // Note this may call ShowSheet() synchronously or in a separate event loop
    // iteration.
    constrained_window::ShowWebModalDialogWithOverlayViews(
        this, contents,
        base::BindOnce(&SSLClientCertificateSelectorDelegate::ShowSheet,
                       weak_factory_.GetWeakPtr(), std::move(client_certs)));
  }

  ~SSLClientCertificateSelectorDelegate() override {
    // Note that the SFChooseIdentityPanel takes a reference to its delegate
    // (|certificate_selector_|) in its -beginSheetForWindow:... method. Break
    // the retain cycle by explicitly canceling the dialog.
    [certificate_selector_ closeSelectorSheetWithCode:NSModalResponseAbort];

    // This matches the StartObserving() call if ShowSheet() was never called
    // and the request was canceled.
    StopObserving();
  }

  // WidgetDelegate:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

  // OkAndCancelableForTesting:
  void ClickOkButton() override {
    // Tests should not call ClickOkButton() on a sheet that has not yet been
    // shown.
    DCHECK(certificate_selector_);
    [certificate_selector_ closeSelectorSheetWithCode:NSModalResponseOK];
  }

  void ClickCancelButton() override {
    // Tests should not call ClickCancelButton() on a sheet that has not yet
    // been shown.
    DCHECK(certificate_selector_);
    [certificate_selector_ closeSelectorSheetWithCode:NSModalResponseCancel];
  }

  // SSLClientAuthObserver implementation:
  void OnCertSelectedByNotification() override {
    [certificate_selector_ closeSelectorSheetWithCode:NSModalResponseStop];
  }

  void SetDeallocClosureForTesting(base::OnceClosure dealloc_closure) {
    dealloc_closure_ = std::move(dealloc_closure);
    SetDeallocClosureIfReady();
  }

  base::OnceClosure GetCancellationCallback() {
    return base::BindOnce(&SSLClientCertificateSelectorDelegate::CloseSelector,
                          weak_factory_.GetWeakPtr());
  }

  void CloseWidgetWithReason(views::Widget::ClosedReason reason) {
    GetWidget()->CloseWithReason(reason);
  }

 private:
  void CloseSelector() {
    cancelled_ = true;
    if (certificate_selector_) {
      [certificate_selector_ closeSelectorSheetWithCode:NSModalResponseStop];
    } else {
      CloseWidgetWithReason(views::Widget::ClosedReason::kUnspecified);
    }
  }

  void ShowSheet(net::ClientCertIdentityList client_certs,
                 views::Widget* overlay_window) {
    DCHECK(!certificate_selector_);
    if (cancelled_) {
      // If CloseSelector() is called before the sheet is shown, it should
      // synchronously destroy the dialog, which means ShowSheet() cannot later
      // be called, but check for this in case the dialog logic changes.
      NOTREACHED();
      return;
    }

    certificate_selector_.reset([[SSLClientCertificateSelectorMac alloc]
        initWithClientCerts:std::move(client_certs)
                   delegate:weak_factory_.GetWeakPtr()]);
    SetDeallocClosureIfReady();
    [certificate_selector_ showSheetForWindow:overlay_window->GetNativeWindow()
                                                  .GetNativeNSWindow()];
  }

  // Attaches |dealloc_closure_| to |certificate_selector_| if both are created.
  // |certificate_selector_| is not created until ShowSheet(), so this method
  // allows SetDeallocClosureForTesting() to take effect when ShowSheet() is
  // deferred.
  void SetDeallocClosureIfReady() {
    if (!certificate_selector_ || dealloc_closure_.is_null())
      return;

    base::scoped_nsobject<DeallocClosureCaller> caller(
        [[DeallocClosureCaller alloc]
            initWithDeallocClosure:std::move(dealloc_closure_)]);
    // The use of the caller as the key is deliberate; nothing needs to ever
    // look it up, so it's a convenient unique value.
    objc_setAssociatedObject(certificate_selector_.get(), caller, caller,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }

  base::scoped_nsobject<SSLClientCertificateSelectorMac> certificate_selector_;
  bool cancelled_ = false;
  base::OnceClosure dealloc_closure_;
  base::WeakPtrFactory<SSLClientCertificateSelectorDelegate> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(SSLClientCertificateSelectorDelegate);
};

}  // namespace

@implementation SSLClientCertificateSelectorMac {
  // The list of SecIdentityRefs offered to the user.
  base::scoped_nsobject<NSMutableArray> _secIdentities;

  // The corresponding list of ClientCertIdentities.
  net::ClientCertIdentityList _certIdentities;

  // A C++ object to report the client certificate selection to.
  base::WeakPtr<SSLClientCertificateSelectorDelegate> _delegate;

  base::scoped_nsobject<SFChooseIdentityPanel> _panel;
}

- (instancetype)
    initWithClientCerts:(net::ClientCertIdentityList)clientCerts
               delegate:(base::WeakPtr<SSLClientCertificateSelectorDelegate>)
                            delegate {
  if ((self = [super init])) {
    _delegate = delegate;
    _certIdentities = std::move(clientCerts);

    _secIdentities.reset([[NSMutableArray alloc] init]);
    for (const auto& cert : _certIdentities) {
      DCHECK(cert->sec_identity_ref());
      [_secIdentities addObject:(id)cert->sec_identity_ref()];
    }
  }
  return self;
}

// The selector sheet ended. There are four possibilities for the return code.
//
// These two return codes are actually generated by the SFChooseIdentityPanel,
// although for testing purposes the OkAndCancelableForTesting implementation
// will also generate them to simulate the user clicking buttons.
//
// - NSModalResponseOK/Cancel: The user clicked the "OK" or "Cancel" button; the
//   SSL auth system needs to be told of this choice.
//
// These two return codes are generated by the
// SSLClientCertificateSelectorDelegate to force the SFChooseIdentityPanel to be
// closed for various reasons.
//
// - NSModalResponseAbort: The user closed the owning tab; the SSL auth system
//   needs to be told of this cancellation.
// - NSModalResponseStop: The SSL auth system already has an answer; just tear
//   down the dialog.
//
// Note that there is a disagreement between the docs and the SDK header file as
// to the type of the return code. It has empirically been determined to be an
// int, not an NSInteger. rdar://45344010
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
            context:(void*)context {
  views::Widget::ClosedReason closedReason =
      views::Widget::ClosedReason::kUnspecified;
  if (_delegate) {
    if (returnCode == NSModalResponseAbort) {
      _delegate->CancelCertificateSelection();
    } else if (returnCode == NSModalResponseOK ||
               returnCode == NSModalResponseCancel) {
      net::ClientCertIdentity* cert = nullptr;
      if (returnCode == NSModalResponseOK) {
        NSUInteger index = [_secIdentities indexOfObject:(id)[_panel identity]];
        if (index != NSNotFound)
          cert = _certIdentities[index].get();
        closedReason = views::Widget::ClosedReason::kAcceptButtonClicked;
      } else {
        closedReason = views::Widget::ClosedReason::kCancelButtonClicked;
      }

      if (cert) {
        _delegate->CertificateSelected(
            cert->certificate(),
            CreateSSLPrivateKeyForSecIdentity(cert->certificate(),
                                              cert->sec_identity_ref())
                .get());
      } else {
        _delegate->CertificateSelected(nullptr, nullptr);
      }
    } else {
      DCHECK_EQ(NSModalResponseStop, returnCode);
      _delegate->StopObserving();
    }
  } else {
    // This should be impossible, assuming _dismissWithCode: synchronously calls
    // this method. (SSLClientCertificateSelectorDelegate calls
    // closeSelectorSheetWithCode: on destruction.)
    NOTREACHED();
  }

  // See comment at definition; this works around a bug.
  ClearWindowTableViewDataSources(sheet);

  // Do not release SFChooseIdentityPanel here. Its -_okClicked: method, after
  // calling out to this method, keeps accessing its ivars, and if panel_ is the
  // last reference keeping it alive, it will crash.
  _panel.autorelease();

  if (_delegate) {
    // This asynchronously releases |self|.
    _delegate->CloseWidgetWithReason(closedReason);
  }
}

- (void)showSheetForWindow:(NSWindow*)window {
  // Get the message to display:
  NSString* message = l10n_util::GetNSStringF(
      IDS_CLIENT_CERT_DIALOG_TEXT,
      base::ASCIIToUTF16(
          _delegate->cert_request_info()->host_and_port.ToString()));

  // Create and set up a system choose-identity panel.
  _panel.reset([[SFChooseIdentityPanel alloc] init]);
  [_panel setInformativeText:message];
  [_panel setDefaultButtonTitle:l10n_util::GetNSString(IDS_OK)];
  [_panel setAlternateButtonTitle:l10n_util::GetNSString(IDS_CANCEL)];
  base::ScopedCFTypeRef<SecPolicyRef> sslPolicy;
  if (net::x509_util::CreateSSLClientPolicy(sslPolicy.InitializeInto()) ==
      noErr) {
    [_panel setPolicies:(id)sslPolicy.get()];
  }

  NSString* title = l10n_util::GetNSString(IDS_CLIENT_CERT_DIALOG_TITLE);
  [_panel beginSheetForWindow:window
                modalDelegate:self
               didEndSelector:@selector(sheetDidEnd:returnCode:context:)
                  contextInfo:nil
                   identities:_secIdentities
                      message:title];
}

- (void)closeSelectorSheetWithCode:(NSModalResponse)response {
  // Closing the sheet using -[NSApp endSheet:] doesn't work, so use the private
  // method. If the sheet is already closed then this is a message send to nil
  // and thus a no-op.
  [_panel _dismissWithCode:response];
}

@end

namespace chrome {

base::OnceClosure ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Not all WebContentses can show modal dialogs.
  //
  // TODO(davidben): Move this hook to the WebContentsDelegate and only try to
  // show a dialog in Browser's implementation. https://crbug.com/456255
  if (!CertificateSelector::CanShow(contents))
    return base::OnceClosure();

  auto* selector_delegate = new SSLClientCertificateSelectorDelegate(
      contents, cert_request_info, std::move(client_certs),
      std::move(delegate));

  return selector_delegate->GetCancellationCallback();
}

OkAndCancelableForTesting* ShowSSLClientCertificateSelectorMacForTesting(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate,
    base::OnceClosure dealloc_closure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* dialog_delegate = new SSLClientCertificateSelectorDelegate(
      contents, cert_request_info, std::move(client_certs),
      std::move(delegate));
  dialog_delegate->SetDeallocClosureForTesting(std::move(dealloc_closure));
  return dialog_delegate;
}

}  // namespace chrome
