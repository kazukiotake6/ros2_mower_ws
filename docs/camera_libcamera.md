# `mower_camera` libcamera CSIカメラ仕様

## 目的と範囲

`mower_camera` はRaspberry Pi 5にCSI接続したカメラをlibcameraで取得し、ROS 2 Jazzyへ画像と較正情報を配信するパッケージです。初期プロファイルはOV9281の1280x800・60 Hz・YUYVです。IMX296も、libcameraが受理するサイズと画素形式に設定すれば同じノードで使用できます。

このノードは安全系ではありません。カメラ・VIOの停止や品質低下を検出した上位ノードが停止要求を出しても、最終的なモータ・ブレード停止は外部MCUと独立安全回路が担います。

## 提供するインターフェース

| 名前 | 型 | 内容 |
| --- | --- | --- |
| `/image_raw` | `sensor_msgs/msg/Image` | キャプチャ画像。Sensor Data QoSで配信する。 |
| `/camera_info` | `sensor_msgs/msg/CameraInfo` | `camera_info_url` の較正値と、実際に設定された画像サイズを配信する。 |
| `/diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | 起動成功時に `mower_camera/libcamera` の状態を配信する。 |

`frame_id` の既定値は `camera_optical_frame` です。画像ヘッダの時刻はROSクロックのPublish時刻です。libcameraの単調クロックをROS時刻としてそのまま使わないため、Camera-IMU同期を評価する際はフレーム開始とBMI270 IRQをハードウェアで同時計測します。

## パラメータ

| パラメータ | 型・既定値 | 説明 |
| --- | --- | --- |
| `width` | `int`, `1280` | 要求画像幅。正数が必要。 |
| `height` | `int`, `800` | 要求画像高。正数が必要。 |
| `frame_rate` | `double`, `60.0` | 要求フレームレート（0より大きく240以下）。FrameDurationLimitsへ反映する。 |
| `pixel_format` | `string`, `YUYV` | `YUYV`、`RGB888`、`BGR888`、`R8`/`Y8`/`MONO8`。大文字小文字を区別しない。 |
| `frame_id` | `string`, `camera_optical_frame` | Image/CameraInfoのフレームID。空文字列不可。 |
| `camera_id` | `string`, `""` | libcameraのカメラID。空なら最初に列挙されたカメラを使う。 |
| `camera_info_url` | `string`, `""` | camera_info_manager互換の較正YAML URL。 |
| `exposure_time_us` | `int`, `0` | 0なら自動露光。正数なら露光時間を固定する。 |
| `analogue_gain` | `double`, `0.0` | 0なら自動ゲイン。正数ならアナログゲインを固定する。 |

要求値をカメラが対応しない場合、libcameraは設定を調整または拒否します。起動後は実際の`/image_raw`と`/camera_info`を必ず確認し、その解像度で較正します。

## 依存関係

Raspberry Pi 5（Ubuntu 24.04 arm64）では、CSIカメラを接続したうえで次を導入します。

```bash
sudo apt update
sudo apt install libcamera-dev libcamera-ipa
```

`libcamera-ipa` はセンサー用IPAモジュールを提供します。欠けていると `No IPA found` が出力されます。コンテナを使う場合は、ホストのCSIデバイスと必要な`/dev/media*`・`/dev/video*`をコンテナへ公開し、コンテナ内にもlibcameraの実行時ライブラリとIPAモジュールを導入します。

`/boot/firmware/config.txt` の `camera_auto_detect=1` を有効にし、接続するセンサーに必要なoverlayをカメラモジュールの資料に従って設定します。CSIフレックスケーブルの向き、コネクタのロック、電源断状態での抜き差しを確認してください。

## ビルドと起動

ワークスペースのルートで実行します。

```bash
source /opt/ros/jazzy/setup.bash
colcon build --packages-select mower_camera
source install/setup.bash
ros2 launch mower_camera libcamera.launch.py
```

既定設定は `src/mower_camera/config/libcamera.yaml` にあります。起動時に変更する例です。

```bash
ros2 run mower_camera libcamera_node --ros-args \
  -p width:=1280 -p height:=800 -p frame_rate:=60.0 \
  -p pixel_format:=YUYV -p camera_info_url:=file:///path/to/camera.yaml
```

固定露光・ゲインでVIO評価を行う例です。

```bash
ros2 run mower_camera libcamera_node --ros-args \
  -p exposure_time_us:=4000 -p analogue_gain:=2.0
```

## 起動確認

別端末で以下を実行します。

```bash
ros2 node list
ros2 topic list -t
ros2 topic hz /image_raw
ros2 topic echo --once /camera_info
```

期待値は、`/image_raw`、`/camera_info`、`/diagnostics`が存在し、`/image_raw`の実効レートが要求値に近いことです。次にチェッカーボードで内部パラメータを較正し、`camera_info_url`を指定して歪み・画像サイズを確認します。

## カメラキャリブレーション

OV9281のチェッカーボード較正、成果物の版管理、受入れ条件は[`camera_calibration.md`](camera_calibration.md)を参照してください。較正YAMLを指定した場合、ノードはロード成功に加え、YAMLの解像度が要求streamと一致し、歪み係数と有効な内部パラメータを持つことを確認します。不一致の場合は誤った内部パラメータを配信せず、起動に失敗します。

較正結果の数値はカメラ個体と取付状態に固有です。実機で生成・レビューした値だけを追加し、サンプル値や他個体の値を流用しないでください。

## トラブルシューティング

- `no libcamera camera found`: センサーが列挙されていません。ケーブル、電源、`camera_auto_detect`、overlay、カーネルログを確認します。`/dev/media*`の存在だけではCSIセンサーの認識を保証しません。
- `No IPA found`: `libcamera-ipa` を導入し、`/usr/lib/aarch64-linux-gnu/libcamera` にIPAモジュールがあることを確認します。
- `requested camera stream is unsupported`: `width`、`height`、`pixel_format`、`frame_rate`をセンサー対応値へ戻します。
- トピックはあるがレートが低い: USB/CSI接続、露光時間、解像度、CPU負荷を確認し、rosbag2でフレーム欠落を記録します。

## 実機検証記録（2026-08-12）

Raspberry Pi 5・Ubuntu 24.04 arm64上で`ros2 run mower_camera libcamera_node`を起動したところ、libcamera 0.2.0は起動したもののカメラを列挙できず、ノードは安全に終了しました。

```text
WARN IPAManager: No IPA found in '/usr/lib/aarch64-linux-gnu/libcamera'
FATAL [libcamera_node]: no libcamera camera found
```

初回検証時は`libcamera-ipa`が未導入でした。その後導入済みの状態で再試行し、IPA警告は解消されましたが、カメラの列挙は依然0台でした。カーネルにはCSI受信機、OV9281、IMX296に対応するデバイス・ログが見つかっていません。CSIケーブル、コネクタ、電源断時の再接続、センサー用overlayを確認した後、「起動確認」のコマンドを再実行して実機合格とします。
