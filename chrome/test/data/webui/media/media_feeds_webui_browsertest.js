// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Media Feeds WebUI.
 */

const EXAMPLE_URL_1 = 'https://example.com/feed.json';

GEN('#include "base/run_loop.h"');
GEN('#include "chrome/browser/media/history/media_history_keyed_service.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "media/base/media_switches.h"');

function MediaFeedsWebUIBrowserTest() {}

MediaFeedsWebUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://media-feeds',

  featureList: {enabled: ['media::kMediaFeeds']},

  isAsync: true,

  testGenPreamble: function() {
    GEN('auto* service =');
    GEN('  media_history::MediaHistoryKeyedService::Get(');
    GEN('    browser()->profile());');
    GEN('service->DiscoverMediaFeed(GURL("' + EXAMPLE_URL_1 + '"));');
    GEN('auto items = std::vector<media_feeds::mojom::MediaFeedItemPtr>();');
    GEN('auto item = media_feeds::mojom::MediaFeedItem::New();');
    GEN('item->name = base::ASCIIToUTF16("The Movie");');
    GEN('item->type = media_feeds::mojom::MediaFeedItemType::kMovie;');
    GEN('item->date_published = base::Time::FromDeltaSinceWindowsEpoch(');
    GEN('  base::TimeDelta::FromMinutes(10));');
    GEN('item->is_family_friendly = true;');
    GEN('item->action_status =');
    GEN('  media_feeds::mojom::MediaFeedItemActionStatus::kPotential;');
    GEN('item->genre.push_back("test");');
    GEN('item->genre.push_back("test2");');
    GEN('item->duration = base::TimeDelta::FromSeconds(30);');
    GEN('item->live = media_feeds::mojom::LiveDetails::New();');
    GEN('item->live->start_time = base::Time::FromDeltaSinceWindowsEpoch(');
    GEN('  base::TimeDelta::FromMinutes(20));');
    GEN('item->live->end_time = base::Time::FromDeltaSinceWindowsEpoch(');
    GEN('  base::TimeDelta::FromMinutes(30));');
    GEN('item->shown_count = 3;');
    GEN('item->clicked = true;');
    GEN('item->author = media_feeds::mojom::Author::New();');
    GEN('item->author->name = "Media Site";');
    GEN('item->author->url = GURL("https://www.example.com");');
    GEN('item->action = media_feeds::mojom::Action::New();');
    GEN('item->action->start_time = base::TimeDelta::FromSeconds(3);');
    GEN('item->action->url = GURL("https://www.example.com");');
    GEN('item->interaction_counters.emplace(');
    GEN('  media_feeds::mojom::InteractionCounterType::kLike, 10000);');
    GEN('item->interaction_counters.emplace(');
    GEN('  media_feeds::mojom::InteractionCounterType::kDislike, 20000);');
    GEN('item->interaction_counters.emplace(');
    GEN('  media_feeds::mojom::InteractionCounterType::kWatch, 30000);');
    GEN('item->content_ratings.push_back(');
    GEN('  media_feeds::mojom::ContentRating::New("MPAA", "PG-13"));');
    GEN('item->content_ratings.push_back(');
    GEN('  media_feeds::mojom::ContentRating::New("agency", "TEST2"));');
    GEN('item->identifiers.push_back(');
    GEN('  media_feeds::mojom::Identifier::New(');
    GEN('    media_feeds::mojom::Identifier::Type::kPartnerId, "TEST1"));');
    GEN('item->identifiers.push_back(');
    GEN('  media_feeds::mojom::Identifier::New(');
    GEN('    media_feeds::mojom::Identifier::Type::kTMSId, "TEST2"));');
    GEN('item->tv_episode = media_feeds::mojom::TVEpisode::New();');
    GEN('item->tv_episode->name = "TV Episode Name";');
    GEN('item->tv_episode->season_number = 1;');
    GEN('item->tv_episode->episode_number = 2;');
    GEN('item->tv_episode->identifiers.push_back(');
    GEN('  media_feeds::mojom::Identifier::New(');
    GEN('    media_feeds::mojom::Identifier::Type::kPartnerId, "TEST3"));');
    GEN('item->play_next_candidate = ');
    GEN('    media_feeds::mojom::PlayNextCandidate::New();');
    GEN('item->play_next_candidate->name = "Next TV Episode Name";');
    GEN('item->play_next_candidate->season_number = 1;');
    GEN('item->play_next_candidate->episode_number = 3;');
    GEN('item->play_next_candidate->duration =');
    GEN('    base::TimeDelta::FromSeconds(10);');
    GEN('item->play_next_candidate->action = ');
    GEN('    media_feeds::mojom::Action::New();');
    GEN('item->play_next_candidate->action->start_time =');
    GEN('    base::TimeDelta::FromSeconds(3);');
    GEN('item->play_next_candidate->action->url = ');
    GEN('    GURL("https://www.example.com");');
    GEN('item->play_next_candidate->identifiers.push_back(');
    GEN('  media_feeds::mojom::Identifier::New(');
    GEN('    media_feeds::mojom::Identifier::Type::kPartnerId, "TEST4"));');
    GEN('item->safe_search_result =');
    GEN('  media_feeds::mojom::SafeSearchResult::kSafe;');
    GEN('media_session::MediaImage image1;');
    GEN('image1.src = GURL("https://www.example.org/image1.png");');
    GEN('item->images.push_back(image1);');
    GEN('media_session::MediaImage image2;');
    GEN('image2.src = GURL("https://www.example.org/image2.png");');
    GEN('item->images.push_back(image2);');
    GEN('items.push_back(std::move(item));');
    GEN('std::vector<media_session::MediaImage> logos;');
    GEN('media_session::MediaImage logo1;');
    GEN('logo1.src = GURL("https://www.example.org/logo1.png");');
    GEN('logos.push_back(logo1);');
    GEN('media_session::MediaImage logo2;');
    GEN('logo2.src = GURL("https://www.example.org/logo2.png");');
    GEN('logos.push_back(logo2);');
    GEN('service->StoreMediaFeedFetchResult(');
    GEN('  1, std::move(items), media_feeds::mojom::FetchResult::kSuccess,');
    GEN('  base::Time::FromDeltaSinceWindowsEpoch(');
    GEN('  base::TimeDelta::FromMinutes(40)), logos, "Test Feed");');
    GEN('base::RunLoop run_loop;');
    GEN('service->PostTaskToDBForTest(run_loop.QuitClosure());');
    GEN('run_loop.Run();');
  },

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('MediaFeedsWebUIBrowserTest', 'All', function() {
  suiteSetup(function() {
    return whenPageIsPopulatedForTest();
  });

  test('check feeds table is loaded', function() {
    const feedsHeaders =
        Array.from(document.querySelector('#feed-table-header').children);

    assertDeepEquals(
        [
          'ID', 'Url', 'Display Name', 'Last Discovery Time', 'Last Fetch Time',
          'User Status', 'Last Fetch Result', 'Fetch Failed Count',
          'Cache Expiry Time', 'Last Fetch Item Count',
          'Last Fetch Play Next Count', 'Last Fetch Content Types', 'Logos',
          'Actions'
        ],
        feedsHeaders.map(x => x.textContent.trim()));

    const feedsContents =
        document.querySelector('#feed-table-body').childNodes[0];

    assertEquals('1', feedsContents.childNodes[0].textContent.trim());
    assertEquals(EXAMPLE_URL_1, feedsContents.childNodes[1].textContent.trim());
    assertEquals('Test Feed', feedsContents.childNodes[2].textContent.trim());
    assertEquals('Auto', feedsContents.childNodes[5].textContent.trim());
    assertEquals('Success', feedsContents.childNodes[6].textContent.trim());
    assertEquals('0', feedsContents.childNodes[7].textContent.trim());
    assertNotEquals('', feedsContents.childNodes[8].textContent.trim());
    assertEquals('1', feedsContents.childNodes[9].textContent.trim());
    assertEquals('1', feedsContents.childNodes[10].textContent.trim());
    assertEquals('Movie', feedsContents.childNodes[11].textContent.trim());
    assertEquals(
        'https://www.example.org/logo1.pnghttps://www.example.org/logo2.png',
        feedsContents.childNodes[12].textContent.trim());
    assertEquals(
        'Show Contents', feedsContents.childNodes[13].textContent.trim());

    // Click on the show contents button.
    feedsContents.childNodes[13].firstChild.click();

    return whenFeedTableIsPopulatedForTest().then(() => {
      assertEquals(
          EXAMPLE_URL_1, document.querySelector('#current-feed').textContent);

      const feedItemsHeaders = Array.from(
          document.querySelector('#feed-items-table thead tr').children);

      assertDeepEquals(
          [
            'Type', 'Name', 'Author', 'Date Published', 'Family Friendly',
            'Action Status', 'Action URL', 'Action Start Time (secs)',
            'Interaction Counters', 'Content Ratings', 'Genre', 'Live Details',
            'TV Episode', 'Play Next Candidate', 'Identifiers', 'Shown Count',
            'Clicked', 'Images', 'Safe Search Result'
          ],
          feedItemsHeaders.map(x => x.textContent.trim()));

      const feedItemsContents =
          document.querySelector('#feed-items-table tbody').childNodes[0];

      assertEquals('Movie', feedItemsContents.childNodes[0].textContent.trim());
      assertEquals(
          'The Movie', feedItemsContents.childNodes[1].textContent.trim());
      assertEquals(
          'Media Site', feedItemsContents.childNodes[2].textContent.trim());
      assertNotEquals('', feedItemsContents.childNodes[3].textContent.trim());
      assertEquals('Yes', feedItemsContents.childNodes[4].textContent.trim());
      assertEquals(
          'Potential', feedItemsContents.childNodes[5].textContent.trim());
      assertEquals(
          'https://www.example.com/',
          feedItemsContents.childNodes[6].textContent.trim());
      assertEquals('3', feedItemsContents.childNodes[7].textContent.trim());
      assertEquals(
          'Watch=30000 Like=10000 Dislike=20000',
          feedItemsContents.childNodes[8].textContent.trim());
      assertEquals(
          'MPAA PG-13, agency TEST2',
          feedItemsContents.childNodes[9].textContent.trim());
      assertEquals(
          'test, test2', feedItemsContents.childNodes[10].textContent.trim());
      assertTrue(
          feedItemsContents.childNodes[11].textContent.trim().includes('Live'));
      assertEquals(
          'TV Episode Name EpisodeNumber=2 SeasonNumber=1 PartnerId=TEST3',
          feedItemsContents.childNodes[12].textContent.trim());
      assertEquals(
          'Next TV Episode Name EpisodeNumber=3 SeasonNumber=1 PartnerId=TEST4 ActionURL=https://www.example.com/ ActionStartTimeSecs=3 DurationSecs=10',
          feedItemsContents.childNodes[13].textContent.trim());
      assertEquals(
          'PartnerId=TEST1 TMSId=TEST2',
          feedItemsContents.childNodes[14].textContent.trim());
      assertEquals('3', feedItemsContents.childNodes[15].textContent.trim());
      assertEquals('Yes', feedItemsContents.childNodes[16].textContent.trim());
      assertEquals(
          'https://www.example.org/image1.pnghttps://www.example.org/image2.png',
          feedItemsContents.childNodes[17].textContent.trim());
      assertEquals('Safe', feedItemsContents.childNodes[18].textContent.trim());
    });
  });

  mocha.run();
});
