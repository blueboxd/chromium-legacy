// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_table_view_controller.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/target_device_info.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_image_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Text color for the Cancel button.
const CGFloat kSendButtonBackgroundColorBlue = 0x1A73E8;

// Accessibility identifier of the Modal Cancel Button.
NSString* const kSendTabToSelfModalCancelButton =
    @"kSendTabToSelfModalCancelButton";
// Accessibility identifier of the Modal Cancel Button.
NSString* const kSendTabToSelfModalSendButton =
    @"kSendTabToSelfModalSendButton";

}  // namespace

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierDevicesToSend = kSectionIdentifierEnumZero,
  SectionIdentifierActionButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSend = kItemTypeEnumZero,
  ItemTypeDevice,
};

@interface SendTabToSelfTableViewController () {
  std::map<std::string, send_tab_to_self::TargetDeviceInfo> _target_device_map;
}

// Item that holds the cancel Button for this Infobar. e.g. "Never Save for this
// site".
@property(nonatomic, strong) TableViewTextButtonItem* sendToDevice;
@end

@implementation SendTabToSelfTableViewController

- (instancetype)initWithModel:
    (send_tab_to_self::SendTabToSelfModel*)sendTabToSelfModel {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];

  if (self) {
    _target_device_map =
        sendTabToSelfModel->GetTargetDeviceNameToCacheInfoMap();
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];
  self.styler.cellBackgroundColor = [UIColor whiteColor];
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:nil];
  cancelButton.accessibilityIdentifier = kSendTabToSelfModalCancelButton;

  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.title =
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_TITLE);
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  [self loadModel];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierDevicesToSend];

  for (auto iter = _target_device_map.begin(); iter != _target_device_map.end();
       ++iter) {
    int daysSinceLastUpdate =
        (base::Time::Now() - iter->second.last_updated_timestamp).InDays();

    SendTabToSelfImageDetailTextItem* deviceItem =
        [[SendTabToSelfImageDetailTextItem alloc] initWithType:ItemTypeDevice];
    deviceItem.text = base::SysUTF8ToNSString(iter->first);
    deviceItem.detailText =
        [self sendTabToSelfdaysSinceLastUpdate:daysSinceLastUpdate];
    switch (iter->second.device_type) {
      case sync_pb::SyncEnums::TYPE_TABLET:
        deviceItem.iconImageName = @"send_tab_to_self_tablet";
        break;
      case sync_pb::SyncEnums::TYPE_PHONE:
        deviceItem.iconImageName = @"send_tab_to_self_smartphone";
        break;
      case sync_pb::SyncEnums::TYPE_WIN:
      case sync_pb::SyncEnums::TYPE_MAC:
      case sync_pb::SyncEnums::TYPE_LINUX:
      case sync_pb::SyncEnums::TYPE_CROS:
        deviceItem.iconImageName = @"send_tab_to_self_laptop";
        break;
      default:
        deviceItem.iconImageName = @"send_tab_to_self_devices";
    }

    if (iter == _target_device_map.begin()) {
      deviceItem.selected = YES;
    }

    [model addItem:deviceItem
        toSectionWithIdentifier:SectionIdentifierDevicesToSend];
  }

  [model addSectionWithIdentifier:SectionIdentifierActionButton];
  self.sendToDevice =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSend];
  self.sendToDevice.buttonText =
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
  self.sendToDevice.buttonTextColor = [UIColor whiteColor];
  ;
  self.sendToDevice.buttonBackgroundColor =
      UIColorFromRGB(kSendButtonBackgroundColorBlue);
  self.sendToDevice.boldButtonText = NO;
  self.sendToDevice.accessibilityIdentifier = kSendTabToSelfModalSendButton;
  [model addItem:self.sendToDevice
      toSectionWithIdentifier:SectionIdentifierActionButton];
}

#pragma mark - Helpers

- (NSString*)sendTabToSelfdaysSinceLastUpdate:(int)days {
  NSString* active_time;
  if (days == 0) {
    active_time = l10n_util::GetNSString(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_TODAY);
  } else if (days == 1) {
    active_time = l10n_util::GetNSString(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_DAY);
  } else {
    active_time = l10n_util::GetNSStringF(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_DAYS,
        base::NumberToString16(days));
  }
  return active_time;
}

@end
