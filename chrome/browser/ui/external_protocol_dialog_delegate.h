// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTERNAL_PROTOCOL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTERNAL_PROTOCOL_DIALOG_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/protocol_dialog_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

// Provides text for the external protocol handler dialog and handles whether
// or not to launch the application for the given protocol.
//
// Note that this class is a content::WebContentsObserver despite not overriding
// any methods from content::WebContentsObserver; it is taking advantage of
// content::WebContentsObserver's behavior where web_contents() will begin
// returning nullptr after the WebContents is destroyed to create an ersatz weak
// pointer to the WebContents.
class ExternalProtocolDialogDelegate : public ProtocolDialogDelegate,
                                       public content::WebContentsObserver {
 public:
  ExternalProtocolDialogDelegate(
      const GURL& url,
      content::WebContents* web_contents,
      const base::Optional<url::Origin>& initiating_origin);
  ~ExternalProtocolDialogDelegate() override;

  const base::string16& program_name() const { return program_name_; }

  void DoAccept(const GURL& url, bool remember) const override;

  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetMessageText() const override;
  base::string16 GetCheckboxText() const override;
  base::string16 GetTitleText() const override;

 private:
  const base::string16 program_name_;
  base::Optional<url::Origin> initiating_origin_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialogDelegate);
};

#endif  // CHROME_BROWSER_UI_EXTERNAL_PROTOCOL_DIALOG_DELEGATE_H_
