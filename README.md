VS2022 で環境構築。<br>
「DirectX12の魔導書」を読みながら進め、IKまで実装して動作済み。<br>

## 環境構築
  ビルドには下記の追加ライブラリが必要。<br>
  clone してライブラリのヘッダインクルードパス、リンカの追加ライブラリパスに通しておく。
  https://github.com/microsoft/DirectXTex
  
  DirectXTex.lib を作成するために1回だけビルドも必要。ビルドした DirectXTex.lib は以下に作成される。
  DirectXTex\Bin\Desktop_2022\x64\Debug
