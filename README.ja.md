\[ [en](README.md) | ja \]

# Chromium-legacy

公式でのサポートが終了しているMac OS X/OS X/macOS、

- Mac OS X 10.7 / Lion
- OS X 10.8 / Mountain Lion
- OS X 10.9 / Mavericks
- OS X 10.10 / Yosemite
- OS X 10.11 / El Capitan
- macOS 10.12 / Sierra

向けの最新版Chromium (Chromeとほぼ同等)です。
一応10.13以降でも動作自体はしますが、一部機能が対応OS上であっても無効化されていることがあるため、特に理由がなければ公式ビルドの利用をお勧めします。

## 機能

基本的に[下記](#制限)のようなOSが対応していない機能を除き、[同バージョンの公式Chromium](https://chromestatus.com/features)と同等です。

## システム要件

Mac OS X 10.7 以降の最新ビルド
- 10.7.5 (11G63)
- 10.8.5 (12F2560)
- 10.9.5 (13F1911)
- 10.10.5 (14F2511)
- 10.11.6 (15G22010)
- 10.12.6 (16G2136)

## リリース

下記各channelのダウンロードやアップデートには[アップデータ](https://github.com/blueboxd/chromium-updater)も利用可能です。

### canary (Chrome canary channel)

Canary版は一日1回自動でビルドされたものが**テストなし**でアップロードされているため、更新内容とタイミングによっては起動しなかったり正常に動作しない可能性もあります。
日常用途としてはなるべく安定しているビルドを使い、頻繁なアップデートは避けた方がいいかもしれません。(これはChrome CanaryやオリジナルのChromiumにおいても同様です)

直近のビルド一覧は[Releases](../../releases)です。

### stable (Chrome stable channel)

Stable版はChromeの安定版ブランチをlegacy化したもので、最低限のテスト(起動、HTML5/JS、メディア再生など)を10.7上で行なっています。
現状手動でマージとビルドを行なっているため、Chromeのリリースから若干遅れることがあります。ご了承ください。

現在のリリースは[releases/tag/stable](../../releases/tag/stable)にあります。

## 制限

- UI
  - ウインドウ
    - 10.9以下に対応した統一されたタイトルバーとタブバーの移植ができていないため、従来のタイトルバーと別個のタブバーという構成になっています
      - Chrome 49の頃のUIをforward-portできないか調査中です
  - メニュー/シート
    - 影が表示されず、背景と区別しづらい
      - 10.9以下では透明なwindowの背景をfillしないとダメなようですが、調査中です
  - スクロールバー
    - “スクロール時に表示”オプション時でもスクロールバーが消えない
      - 下記のGPU Compotisiting無効化の影響のようです
- GPU
  - 10.7ではOpenGLが古すぎるためGPUは利用できず、すべてソフトウェアレンダリングやソフトウェアデコードとなります
  - 10.8/10.9では描画に問題が出るため`--disable-gpu-compositing`オプションによってGPU Compotisitingが無効化されています
- DRM
  - 10.7/10.8ではDRM保護されたメディアは利用できません
  - 10.9以上では[Widevineライブラリをインストール](../../wiki/DRM)する必要があります
- U2F/WebAuthn/FIDO2
  - 10.7ではUSBキーの利用に[パッチを当てた `IOHIDFamily.kext`](../../../IOHIDFamily-368.21)が必要です

## ビルド

ソースからのビルド手順については[Building](../../wiki/Building)をご参照ください。
