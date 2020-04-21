// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_MIXED_CONTENT_DOWNLOAD_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_MIXED_CONTENT_DOWNLOAD_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "components/download/public/common/download_item.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

// An infobar that asks if user wants to download an insecurely delivered file
// initiated from a secure context.  Note that this infobar does not expire if
// the user subsequently navigates, since such navigations won't automatically
// cancel the underlying download.
class MixedContentDownloadInfoBarDelegate
    : public ConfirmInfoBarDelegate,
      public download::DownloadItem::Observer {
 public:
  static void Create(InfoBarService* infobar_service,
                     download::DownloadItem* download_item);

  ~MixedContentDownloadInfoBarDelegate() override;

  // download::DownloadItem::Observer:
  void OnDownloadDestroyed(download::DownloadItem* download_item) override;

 private:
  explicit MixedContentDownloadInfoBarDelegate(
      download::DownloadItem* download_item);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // The download item that is requesting the infobar. Could get deleted while
  // the infobar is showing, so we also copy the info we need from it.
  download::DownloadItem* download_item_;
  base::string16 message_text_;
  download::DownloadItem::MixedContentStatus mixed_content_status_;

  DISALLOW_COPY_AND_ASSIGN(MixedContentDownloadInfoBarDelegate);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_MIXED_CONTENT_DOWNLOAD_INFOBAR_DELEGATE_H_
