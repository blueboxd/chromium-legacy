// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialListViewController () <UITableViewDataSource,
                                            UISearchResultsUpdating>

// Search controller that contains search bar.
@property(nonatomic, strong) UISearchController* searchController;

// Current list of suggested passwords.
@property(nonatomic, copy) NSArray<id<Credential>>* suggestedPasswords;

// Current list of all passwords.
@property(nonatomic, copy) NSArray<id<Credential>>* allPasswords;

@end

// TODO(crbug.com/1045454): Implement this view controller's items.
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
  UITableViewCell* cell = [tableView dequeueReusableCellWithIdentifier:@"cell"];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:@"cell"];
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  }
  id<Credential> credential;
  if ([self isSuggestedPasswordSection:indexPath.section]) {
    credential = self.suggestedPasswords[indexPath.row];
  } else {
    credential = self.allPasswords[indexPath.row];
  }
  cell.textLabel.text = credential.user;
  cell.detailTextLabel.text = credential.serviceName;
  return cell;
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

@end
