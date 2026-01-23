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

#### 必要なもの

- Visual Studio 2019以上（C++開発環境）

#### ビルド手順

1. リポジトリをクローン
   ```
   git clone https://github.com/user/nair-substream.git
   cd nair-substream
   ```

2. Visual Studioでソリューションを開く
   ```
   start nair-substream.sln
   ```

3. ビルド設定を「Release」に変更し、ビルドを実行

   または、コマンドラインからビルド:
   ```powershell
   # MSBuildをフルパスで実行
   & "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" nair-substream.sln /p:Configuration=Release /p:Platform=x64
   
   # または、MSBuildをPATHに追加してから実行
   $env:Path += ";C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64"
   msbuild nair-substream.sln /p:Configuration=Release /p:Platform=x64
   ```

4. ビルド成果物を配置
   ```
   copy .\x64\Release\nair-substream.dll n-air-app\node_modules\obs-studio-node\obs-plugins\64bit\
   ```

## 使用方法

このプラグインはN Airアプリケーションと連携して動作します。N Airの設定画面から「サブストリーム設定」を選択し、追加の配信先を設定してください。

## 開発者向け情報

### 通信プロトコル

プラグインは名前付きパイプ(`\\.\pipe\NAirSubstream`)を使用してN Airアプリケーションと通信します。JSONフォーマットでコマンドを送受信します。

#### メッセージフォーマット

- **デリミタ**: 改行文字 (`\n`) で区切られたJSON
- **エンコーディング**: UTF-8
- **リクエスト形式**:
  ```json
  {"id": "unique-id", "fn": "function-name", "arg": {...}}\n
  ```

- **レスポンス形式**:
  ```json
  {"id": "unique-id", "res": {...}}\n
  ```

**重要**: 送信側は必ず各JSONメッセージの末尾に改行文字 (`\n`) を付けて送信してください。改行がないとプラグイン側でメッセージを認識できません。

#### 利用可能なコマンド

##### 1. `enumEncoderTypes` - エンコーダータイプの列挙

**リクエスト**:
```json
{"id": "1", "fn": "enumEncoderTypes", "arg": {}}\n
```

**レスポンス**:
```json
{
  "id": "1",
  "res": {
    "encoders": {
      "audio": [{"id": "...", "name": "..."}],
      "video": [{"id": "...", "name": "..."}]
    }
  }
}\n
```

##### 2. `start` - ストリーミング開始

**リクエスト**:
```json
{
  "id": "2",
  "fn": "start",
  "arg": {
    "output": {...},
    "service": {...},
    "videoId": "obs_x264",
    "video": {...},
    "audioId": "ffmpeg_aac",
    "audio": {...}
  }
}\n
```

**レスポンス**:
```json
{"id": "2", "res": {"result": "OK"}}\n
```

または、エラー時:
```json
{"id": "2", "res": {"error": "エラーメッセージ"}}\n
```

##### 3. `stop` - ストリーミング停止

**リクエスト**:
```json
{"id": "3", "fn": "stop", "arg": {}}\n
```

**レスポンス**:
```json
{"id": "3", "res": {"result": "OK"}}\n
```

##### 4. `status` - ステータス取得

**リクエスト**:
```json
{"id": "4", "fn": "status", "arg": {}}\n
```

**レスポンス**:
```json
{
  "id": "4",
  "res": {
    "active": true,
    "status": "started",
    "error": "",
    "busy": false,
    "streaming": true,
    "duration": 120,
    "connectTime": 1234,
    "bytes": 1048576,
    "frames": 3600,
    "congestion": 0.0,
    "dropped": 0
  }
}\n
```

**ステータス値**:
- `starting` - ストリーミング開始処理中
- `started` - ストリーミング配信中
- `stopping` - ストリーミング停止処理中
- `stopped` - ストリーミング停止
- `reconnect` - 再接続試行中
- `reconnected` - 再接続成功
- `deactive` - 非アクティブ化

##### 5. `info` - プラグイン情報取得

**リクエスト**:
```json
{"id": "5", "fn": "info", "arg": {}}\n
```

**レスポンス**:
```json
{"id": "5", "res": {"version": "1.0.0", "process": 12345}}\n
```

