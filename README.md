# N Air Substream

## 概要

N Air Substreamは、N Airアプリケーションから複数の配信サービスへ同時配信するための拡張機能（OBSプラグイン）です。名前付きパイプを使用したJSON形式の通信によって、N Airからのコマンドに応じてストリーミング配信の開始・停止などを制御します。

### 主な機能

- 複数の配信サービスへの同時配信
- エンコーダータイプの列挙
- ストリーミングの開始・停止
- ストリーミング状態の監視
- エラーハンドリングとステータス管理

## ビルド

### 必要なもの

- Visual Studio 2026
  - C++によるデスクトップ開発
  - v145 C++ツールセット
  - Windows SDK
- `thirdparty/obs-libs` に同梱されたLibOBS SDK

### LibOBS SDKの互換性

このプラグインはN Airと同じプロセスで動作し、LibOBSへ直接リンクします。そのため、
ビルドに使うヘッダーと `obs.lib` は、N Airに組み込まれているLibOBSと同じビルドのものが
必要です。通常版OBSや、近いバージョンのSDKでは代用できません。バージョンがずれると、
ビルドに成功してもロード失敗やクラッシュ、予期しない動作が発生する可能性があります。

現在の対応関係は次のとおりです。

- N Air `obs-studio-node`: 0.26.28
- LibOBS: 31.1.2sl19
- LibOBSソース: [`streamlabs/obs-studio`](https://github.com/streamlabs/obs-studio)
- 同梱SDK: `thirdparty/obs-libs/include` および `thirdparty/obs-libs/lib/obs.lib`

バージョンは [`thirdparty/obs-libs/VERSION`](thirdparty/obs-libs/VERSION) と、N Airが利用する
[`native-deps` のリリース](https://github.com/n-air-app/native-deps/releases/tag/osn0.26.28)
で確認できます。N Airの配布パッケージは同リリースの
[`osn-0.26.28-release-win64.tar.gz`](https://github.com/n-air-app/native-deps/releases/download/osn0.26.28/osn-0.26.28-release-win64.tar.gz)
から取得されています。`obs-studio-node` 側のビルド定義は
[`streamlabs/obs-studio-node` の0.26.28タグ](https://github.com/streamlabs/obs-studio-node/tree/0.26.28)、
LibOBSの対応ソースは
[`streamlabs/obs-studio` の31.1.2sl19タグ](https://github.com/streamlabs/obs-studio/tree/31.1.2sl19)
（commit `14cd01f72cb2a4e6b4e0e8d6da80533b44fd900d`）で確認できます。

SDKを更新するときは、次の順序で対応バージョンを確認します。

1. [N Airの`package.json`](https://github.com/n-air-app/n-air-app/blob/n-air_development/package.json)
   で、使用している `obs-studio-node` のバージョンを確認する
   （例: `osn0.26.28` の配布URLならバージョンは `0.26.28`）
2. 対応する [`native-deps` のリリース](https://github.com/n-air-app/native-deps/releases)
   で `LibOBSVersion` を確認する
3. [`streamlabs/obs-studio` のタグ一覧](https://github.com/streamlabs/obs-studio/tags)
   から同じバージョンのタグを開き、ソースとビルド定義を確認する
4. 次のURLの `{LibOBSVersion}` を確認した値に置き換えてSDKを取得する

```text
https://obsstudios3.streamlabs.com/libobs-windows64-release-{LibOBSVersion}.7z
```

現在は `LibOBSVersion=31.1.2sl19` なので、取得先は
[`libobs-windows64-release-31.1.2sl19.7z`](https://obsstudios3.streamlabs.com/libobs-windows64-release-31.1.2sl19.7z)
です。このアーカイブのSHA-256は
`EE7CF5078DD7D243303F98D589F5E3D7F7B060B6C9F40BB0CAA7F3976ABF260C` です。

取得したSDKから `include/` とWindows x64用の `lib/obs.lib` を必ず同時に置き換え、
`thirdparty/obs-libs/VERSION` も更新します。

### ビルド手順

リポジトリをクローンします。

```powershell
git clone https://github.com/n-air-app/substream.git
cd substream
```

Developer PowerShell for Visual Studioで次のコマンドを実行します。

```powershell
msbuild nair-substream.sln /p:Configuration=Release /p:Platform=x64
```

成果物は `x64\Release\nair-substream.dll` に出力されます。N Airで使用するには、
このDLLをN Air側の `obs-plugins\64bit` ディレクトリへ配置してください。

## 使用方法

このプラグインはN Airアプリケーションと連携して動作します。N Airの設定画面から「サブストリーム設定」を選択し、追加の配信先を設定してください。

## 開発者向け情報

プラグインはUTF-8のJSONを名前付きパイプ `\\.\pipe\NAirSubstream` で送受信します。
メッセージは改行 (`\n`) 区切りで、各リクエストの末尾にも改行が必要です。

```json
{"id": "1", "fn": "status", "arg": {}}
```

応答は同じ `id` と、結果またはエラーを格納した `res` を返します。

```json
{"id": "1", "res": {"active": true, "status": "started"}}
```

## ライセンス

N Air Substreamは、N Air本体と同じくGNU General Public License v3のもとで
公開されています。詳細は
`LICENSE` を参照してください。同梱している第三者ソフトウェアのライセンスについては
`thirdparty/README.md` および `thirdparty/LICENSES/` を参照してください。
