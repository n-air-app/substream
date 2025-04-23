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
   ```
   MSBuild.exe /p:Configuration=Release
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

