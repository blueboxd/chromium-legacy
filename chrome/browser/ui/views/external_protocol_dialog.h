// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class ProtocolDialogDelegate;

namespace content {
class WebContents;
}

namespace test {
class ExternalProtocolDialogTestApi;
}

namespace views {
class Checkbox;
class MessageBoxView;
}

class ExternalProtocolDialog : public views::DialogDelegateView {
 public:
  // Show by calling ExternalProtocolHandler::RunExternalProtocolDialog().
  ExternalProtocolDialog(std::unique_ptr<const ProtocolDialogDelegate> delegate,
                         content::WebContents* web_contents);

  ~ExternalProtocolDialog() override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  bool ShouldShowCloseButton() const override;
  int GetDefaultDialogButton() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetWindowTitle() const override;
  bool Cancel() override;
  bool Accept() override;
  views::View* GetContentsView() override;
  ui::ModalType GetModalType() const override;

 private:
  friend class test::ExternalProtocolDialogTestApi;

  const std::unique_ptr<const ProtocolDialogDelegate> delegate_;

  views::Checkbox* remember_decision_checkbox_;

  // The message box whose commands we handle.
  views::MessageBoxView* message_box_view_;

  // The time at which this dialog was created.
  base::TimeTicks creation_time_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
