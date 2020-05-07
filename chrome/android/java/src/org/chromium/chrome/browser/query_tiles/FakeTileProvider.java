// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.TileProvider;

import java.util.ArrayList;
import java.util.List;

/**
 * A test tile provider class that fakes the backend and provides example data.
 */
public class FakeTileProvider implements TileProvider {
    private List<QueryTile> mTiles = new ArrayList<>();

    public FakeTileProvider() {
        FakeTile tile = null;
        List<QueryTile> children = new ArrayList<>();

        children.clear();
        children.add(new FakeTile("tile1_1", "India",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcTCr5Ene2snzAE_tOxcZ6AlKrH8CLA4aYQYYLRepngj5oh5bwHagRF0ootjfRDlM1k",
                null));
        children.add(new FakeTile("tile1_2", "Latest news",
                "http://t1.gstatic.com/images?q=tbn:ANd9GcQZIopJYcA7KFGcpA2ye385iiEEtkOfZn6uDQrDvTlRD_S-e9-KOkKNh9oE2V2K_Uk",
                null));
        children.add(new FakeTile("tile1_3", "Video",
                "http://t1.gstatic.com/images?q=tbn:ANd9GcSSUTrzE2L7LWwsvqevSwwj-CJYmhzYsXER-Qy9jCUAF9Ncu6VJcSymtksFq6EbAZU",
                null));
        children.add(new FakeTile("tile1_4", "News live",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcRjuMvz4MlWifrdoKEv9Ysjx9TL_Q6p9PZQf1nmnmqcz4OUgpU9gf3WMkKB9_JQK-U",
                null));
        children.add(new FakeTile("tile1_5", "Entertainment",
                "http://t1.gstatic.com/images?q=tbn:ANd9GcQ1DgiH1n3zwD9M6fGlvXjUc1q7Vviod8kE3Ak4gqPG6sfEnJB3Ek0Ag_WekvBc1c0",
                null));
        tile = new FakeTile("1", "News",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcTFlesDfqnMIxCvcotuKHBR-U4cSOG1ceOcoitEOWuiRq9MqNn05e6agwcQHVXiQ3A",
                new ArrayList<>(children));
        mTiles.add(tile);

        children.clear();
        children.add(new FakeTile("tile2_1", "New releases",
                "http://t3.gstatic.com/images?q=tbn:ANd9GcT0TXKxHnR-ZsSgGoZQU-q3p3IJinh04R7_-ApKqvU5dG08GeEI_tBrposmeXBj_sM",
                null));
        children.add(new FakeTile("tile2_2", "Best 2019",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcQj0bEH217cUI5_RZIFu70U6HyxHjQAJjr_cRE_90KUzitct9UmmbiPIth64NVmBbw",
                null));
        children.add(new FakeTile("tile2_3", "Action",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcRRfln9zLxGtoxSNm7A5aZONaU0UKItu-QMCPFBB3vB278uJzzx0-DyCnEFJbWrFd4",
                null));
        children.add(new FakeTile("tile2_4", "Romantic",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcRtP-cfkS_wTEiLoJWzhqTPsS_K3ibnMEPis3Mc4VFSyKKKhFjrpLzr5HqGIxgd8Xk",
                null));
        children.add(new FakeTile("tile2_5", "Video",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcTzYvuKtm_0lZlf2gJiY7NAFARzFmM8CdsZsRlUw5v86bZCHBDATZ_meercGPrPiiA",
                null));
        tile = new FakeTile("2", "Films",
                "http://t1.gstatic.com/images?q=tbn:ANd9GcRuSbDebh0CCLeMEr2Wh8qmHpWSKdbqrZFWZsndsu7TMtPeeNDYIKrQqexISQ4Bk0U",
                new ArrayList<>(children));
        mTiles.add(tile);

        children.clear();
        children.add(new FakeTile("tile3_1", "Sports News",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcTPE3E_bwbTfnnbncoJWr0nVGjYq5Xo1KVzsd1RjVzYzQ7bDmkIfiJq5YICMO5oOC0",
                null));
        children.add(new FakeTile("tile3_2", "Cricket",
                "http://t1.gstatic.com/images?q=tbn:ANd9GcSKpIPKbz3xQNTAFcj0RjvAMy4VkQQ59UB9MHbrWv18UnWMwzziQ0gDf31bBGCWW84",
                null));
        children.add(new FakeTile("tile3_3", "Live Scores",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcTNe2t7j38aebbbrDhrPWqVoSxJontlVju_wpoMF1RPZC8tdTFhTsVJM_Ax7miNQnA",
                null));
        children.add(new FakeTile("tile3_4", "India national cricket team",
                "http://t1.gstatic.com/images?q=tbn:ANd9GcTdAED2AVXWGbch4xSJdjAsQVjMN-g2COE38eCxuJ-g3NoAFFtXsZY-OEvHNj3o1qU",
                null));
        children.add(new FakeTile("tile3_5", "Shikhar Dhawan",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcRM7ciJIz1xCbj7Yoo0IwagKAmZ9w6bcyoPAb0aGiAIDPFtW6uGyZgqsoeqspfZGjk",
                null));
        tile = new FakeTile("3", "Sports",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcTPOt9GGURBjUQxg-U5-cVt8lJqmvsO6xV1utP--yyHt_YVfAbi3c4c6JPG3mQyYXA",
                new ArrayList<>(children));
        mTiles.add(tile);

        children.clear();
        children.add(new FakeTile("tile4_1", "Stock market",
                "http://t1.gstatic.com/images?q=tbn:ANd9GcQx19VxfDCoQB6ju0ar378vNq8Wvkjkbw9rdEYD4f5JlpEFMIJBJj93yonFoEKPD4E",
                null));
        children.add(new FakeTile("tile4_2", "Financial news",
                "http://t1.gstatic.com/images?q=tbn:ANd9GcSUAGi0zETa0LKs8mddhIRQd1kzLvm8JLcsOpDWnaBqmH8gu-Ildf5GO0mkW_xIrNY",
                null));
        children.add(new FakeTile("tile4_3", "Real estate",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcTFlesDfqnMIxCvcotuKHBR-U4cSOG1ceOcoitEOWuiRq9MqNn05e6agwcQHVXiQ3A",
                null));
        children.add(new FakeTile("tile4_4", "Mutual fund",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcRM7ciJIz1xCbj7Yoo0IwagKAmZ9w6bcyoPAb0aGiAIDPFtW6uGyZgqsoeqspfZGjk",
                null));
        children.add(new FakeTile("tile4_5", "Gold",
                "http://t3.gstatic.com/images?q=tbn:ANd9GcRarq7ZX7nv7RWjWl7NvmWp83XO6NgPpkU3eY_FzjfBsoOLBGBBNZK5UvRl4KhK6tc",
                null));
        tile = new FakeTile("4", "Investment",
                "http://t1.gstatic.com/images?q=tbn:ANd9GcSUAGi0zETa0LKs8mddhIRQd1kzLvm8JLcsOpDWnaBqmH8gu-Ildf5GO0mkW_xIrNY",
                new ArrayList<>(children));
        mTiles.add(tile);

        children.clear();
        children.add(new FakeTile("tile5_1", "Food",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcR4QAsqpRFDC1bXNGrhSOEOZR-nD-jr7wIuV_p-BCRy6o2f_iWbvD8-5Ay8govJfao",
                null));
        children.add(new FakeTile("tile5_2", "Diet",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcR4QAsqpRFDC1bXNGrhSOEOZR-nD-jr7wIuV_p-BCRy6o2f_iWbvD8-5Ay8govJfao",
                null));
        children.add(new FakeTile("tile5_3", "Women",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcR4QAsqpRFDC1bXNGrhSOEOZR-nD-jr7wIuV_p-BCRy6o2f_iWbvD8-5Ay8govJfao",
                null));
        children.add(new FakeTile("tile5_4", "Kids",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcR4QAsqpRFDC1bXNGrhSOEOZR-nD-jr7wIuV_p-BCRy6o2f_iWbvD8-5Ay8govJfao",
                null));
        children.add(new FakeTile("tile5_5", "Exercise",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcR4QAsqpRFDC1bXNGrhSOEOZR-nD-jr7wIuV_p-BCRy6o2f_iWbvD8-5Ay8govJfao",
                null));
        tile = new FakeTile("5", "Health",
                "http://t2.gstatic.com/images?q=tbn:ANd9GcR4QAsqpRFDC1bXNGrhSOEOZR-nD-jr7wIuV_p-BCRy6o2f_iWbvD8-5Ay8govJfao",
                new ArrayList<>(children));
        mTiles.add(tile);

        children.clear();
        children.add(new FakeTile("tile6_1", "New Releases",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcSb3yVUlA_2p1hnTB-iAWHiHbamV7WXWuTtF7ph8UMVypPZtNcWijS9iJanH_Tph0M",
                null));
        children.add(new FakeTile("tile6_2", "Best 2019",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcSb3yVUlA_2p1hnTB-iAWHiHbamV7WXWuTtF7ph8UMVypPZtNcWijS9iJanH_Tph0M",
                null));
        children.add(new FakeTile("tile6_3", "Hindi",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcSb3yVUlA_2p1hnTB-iAWHiHbamV7WXWuTtF7ph8UMVypPZtNcWijS9iJanH_Tph0M",
                null));
        children.add(new FakeTile("tile6_4", "Music Download",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcSb3yVUlA_2p1hnTB-iAWHiHbamV7WXWuTtF7ph8UMVypPZtNcWijS9iJanH_Tph0M",
                null));
        children.add(new FakeTile("tile6_5", "Live streaming",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcSb3yVUlA_2p1hnTB-iAWHiHbamV7WXWuTtF7ph8UMVypPZtNcWijS9iJanH_Tph0M",
                null));
        tile = new FakeTile("6", "Music",
                "http://t0.gstatic.com/images?q=tbn:ANd9GcSb3yVUlA_2p1hnTB-iAWHiHbamV7WXWuTtF7ph8UMVypPZtNcWijS9iJanH_Tph0M",
                new ArrayList<>(children));
        mTiles.add(tile);

        children.clear();
        children.add(new FakeTile("tile7_1", "Subtopic", "", null));
        children.add(new FakeTile("tile7_2", "Subtopic", "", null));
        tile = new FakeTile("7", "Tile 7", "", children);
        mTiles.add(tile);
        tile = new FakeTile("8", "Tile 8", "", children);
        mTiles.add(tile);
        tile = new FakeTile("9", "Tile 9", "", children);
        mTiles.add(tile);
        tile = new FakeTile("10", "Tile 10", "", children);
        mTiles.add(tile);
        tile = new FakeTile("11", "Tile 11", "", children);
        mTiles.add(tile);
        tile = new FakeTile("12", "Tile 12", "", children);
        mTiles.add(tile);
    }

    @Override
    public void getQueryTiles(Callback<List<QueryTile>> callback) {
        callback.onResult(mTiles);
    }

    private QueryTile findTile(String id) {
        if (id == null) return null;
        for (QueryTile tile : mTiles) {
            if (tile.id.equals(id)) return tile;
            for (QueryTile subtile : tile.children) {
                if (subtile.id.equals(id)) return subtile;
            }
        }
        return null;
    }

    private ImageFetcher getImageFetcher() {
        return ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                GlobalDiscardableReferencePool.getReferencePool());
    }

    private static class FakeTile extends QueryTile {
        public String url;

        public FakeTile(String id, String displayTitle, String url, List<QueryTile> children) {
            super(id, displayTitle, displayTitle, displayTitle, new String[] {url}, children);
            this.url = url;
        }
    }
}
