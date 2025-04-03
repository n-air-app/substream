
## N Air sub stream

N Airに別途配信サービスへ配信する補助DLL

## ビルド方法

ビルドには Visual Studio C++ が必要です。

`git clone` して `nair-substream.sln` ソリューションを開いてビルドしてください。

コマンドラインの場合、Visual Studioのパスが通ってるとして

`MSBuild.exe /p:Configuration=Release`

コンパイル結果のファイル

`.\x64\Release\nair-substream.dll`

を、以下にコピーして使用します。

`n-air-app\node_modules\obs-studio-node\obs-plugins\64bit\`

