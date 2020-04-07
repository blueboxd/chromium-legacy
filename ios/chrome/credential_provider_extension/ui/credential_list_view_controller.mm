// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* kHeaderIdentifier = @"clvcHeader";
NSString* kCellIdentifier = @"clvcCell";

const CGFloat kHeaderHeight = 70;
const CGFloat kFallbackIconSize = 24;
const CGFloat kFallbackRoundedCorner = 3;

// Returns an icon with centered letter from given |string| and some
// colors computed from the string.
// TODO(crbug.com/1045454): draw actual icon or default icon.
UIImage* GetFallbackImageWithStringAndColor(NSString* string) {
  int hash = string.hash;
  UIColor* backgroundColor =
      [UIColor colorWithRed:0.5 + (hash & 0xff) / 510.0
                      green:0.5 + ((hash >> 8) & 0xff) / 510.0
                       blue:0.5 + ((hash >> 16) & 0xff) / 510.0
                      alpha:1.0];

  UIColor* textColor = [UIColor colorWithRed:(hash & 0xff) / 510.0
                                       green:((hash >> 8) & 0xff) / 510.0
                                        blue:((hash >> 16) & 0xff) / 510.0
                                       alpha:1.0];

  CGRect rect = CGRectMake(0, 0, kFallbackIconSize, kFallbackIconSize);

  UIGraphicsBeginImageContext(rect.size);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSetFillColorWithColor(context, [backgroundColor CGColor]);
  CGContextSetStrokeColorWithColor(context, [textColor CGColor]);

  UIBezierPath* rounded =
      [UIBezierPath bezierPathWithRoundedRect:rect
                                 cornerRadius:kFallbackRoundedCorner];
  [rounded fill];

  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  font = [font fontWithSize:(kFallbackIconSize / 2)];
  CGRect textRect = CGRectMake(0, (kFallbackIconSize - [font lineHeight]) / 2,
                               kFallbackIconSize, [font lineHeight]);
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  [paragraphStyle setAlignment:NSTextAlignmentCenter];
  NSMutableDictionary* attributes = [[NSMutableDictionary alloc] init];
  [attributes setValue:font forKey:NSFontAttributeName];
  [attributes setValue:textColor forKey:NSForegroundColorAttributeName];
  [attributes setValue:paragraphStyle forKey:NSParagraphStyleAttributeName];
  [[string substringToIndex:1] drawInRect:textRect withAttributes:attributes];

  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return image;
}
}

@interface CredentialListViewController () <UITableViewDataSource,
                                            UISearchResultsUpdating>

// Search controller that contains search bar.
@property(nonatomic, strong) UISearchController* searchController;

// Current list of suggested passwords.
@property(nonatomic, copy) NSArray<id<Credential>>* suggestedPasswords;

// Current list of all passwords.
@property(nonatomic, copy) NSArray<id<Credential>>* allPasswords;

@end

@implementation CredentialListViewController

@synthesize delegate;

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.navigationItem.rightBarButtonItem = [self navigationCancelButton];

  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.searchResultsUpdater = self;
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.tableView.tableHeaderView = self.searchController.searchBar;
  self.navigationController.navigationBar.translucent = NO;

  // Presentation of searchController will walk up the view controller hierarchy
  // until it finds the root view controller or one that defines a presentation
  // context. Make this class the presentation context so that the search
  // controller does not present on top of the navigation controller.
  self.definesPresentationContext = YES;

  [self.tableView registerClass:[UITableViewHeaderFooterView class]
      forHeaderFooterViewReuseIdentifier:kHeaderIdentifier];
  self.tableView.sectionHeaderHeight = kHeaderHeight;
}

#pragma mark - CredentialListConsumer

- (void)presentSuggestedPasswords:(NSArray<id<Credential>>*)suggested
                     allPasswords:(NSArray<id<Credential>>*)all {
  self.suggestedPasswords = suggested;
  self.allPasswords = all;
  [self.tableView reloadData];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return [self numberOfSections];
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  if ([self isSuggestedPasswordSection:section]) {
    return self.suggestedPasswords.count;
  } else {
    return self.allPasswords.count;
  }
}

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  if ([self isEmptyTable]) {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NO_SEARCH_RESULTS",
                             @"No search results found");
  } else if ([self isSuggestedPasswordSection:section]) {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_SUGGESTED_PASSWORDS",
                             @"Suggested Passwords");
  } else {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_ALL_PASSWORDS",
                             @"All Passwords");
  }
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kCellIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:kCellIdentifier];
    cell.accessoryType = UITableViewCellAccessoryDetailButton;
  }

  id<Credential> credential = [self credentialForIndexPath:indexPath];
  cell.imageView.image = GetFallbackImageWithStringAndColor(credential.user);
  cell.imageView.contentMode = UIViewContentModeScaleAspectFit;
  cell.textLabel.text = credential.user;
  cell.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  cell.detailTextLabel.text = credential.serviceName;
  cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return cell;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UITableViewHeaderFooterView* view = [self.tableView
      dequeueReusableHeaderFooterViewWithIdentifier:kHeaderIdentifier];
  view.textLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  view.contentView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  return view;
}

- (void)tableView:(UITableView*)tableView
    accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath {
  id<Credential> credential = [self credentialForIndexPath:indexPath];
  [self.delegate showDetailsForCredential:credential];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  [self.delegate updateResultsWithFilter:searchController.searchBar.text];
}

#pragma mark - Private

// Creates a cancel button for the navigation item.
- (UIBarButtonItem*)navigationCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self.delegate
                           action:@selector(navigationCancelButtonWasPressed:)];
  return cancelButton;
}

// Returns number of sections to display based on |suggestedPasswords| and
// |allPasswords|. If no sections with data, returns 1 for the 'no data' banner.
- (int)numberOfSections {
  if ([self.suggestedPasswords count] == 0 || [self.allPasswords count] == 0) {
    return 1;
  }
  return 2;
}

// Returns YES if there is no data to display.
- (BOOL)isEmptyTable {
  return [self.suggestedPasswords count] == 0 && [self.allPasswords count] == 0;
}

// Returns YES if given section is for suggested passwords.
- (BOOL)isSuggestedPasswordSection:(int)section {
  int sections = [self numberOfSections];
  if ((sections == 2 && section == 0) ||
      (sections == 1 && self.suggestedPasswords.count)) {
    return YES;
  } else {
    return NO;
  }
}

- (id<Credential>)credentialForIndexPath:(NSIndexPath*)indexPath {
  if ([self isSuggestedPasswordSection:indexPath.section]) {
    return self.suggestedPasswords[indexPath.row];
  } else {
    return self.allPasswords[indexPath.row];
  }
}

@end
