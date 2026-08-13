# OV9281 カメラキャリブレーション手順

この手順は、`mower_camera` のOV9281（1280x800、YUYV、60 Hz）に対する単眼内部パラメータ較正用です。較正値はカメラ個体、レンズの焦点・固定状態、防水窓、取付位置、画像サイズごとに有効です。これらのいずれかを変更した場合は再較正します。

## 前提

- 車体、カメラおよび防水窓を最終取付状態に固定する。防水窓を使う場合は必ず装着して較正する。
- 露光とゲインを固定し、画像が飽和・ぶれない明るさにする。
- 印刷精度を確認したチェッカーボードを用いる。`--size`は内側コーナー数、`--square`は正確に測った一辺のメートル値である。
- VIO担当と、最低枚数、距離・傾きの分布、許容再投影誤差を合意してから開始する。未合意の値を合格値として扱わない。

## 撮影と算出

Pi上でカメラを起動する。

```bash
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 launch mower_camera libcamera.launch.py \
  camera_info_url:='' exposure_time_us:=4000 analogue_gain:=2.0
```

別端末でROSの較正ツールを導入して起動する。以下の`8x6`と`0.024`は例であり、使用するボードの値へ置き換える。

```bash
sudo apt install ros-jazzy-camera-calibration
ros2 run camera_calibration cameracalibrator \
  --size 8x6 --square 0.024 --no-service-check \
  camera:=/ image:=image_raw
```

ボードを画面の中央・四隅、近距離・遠距離、前傾・後傾・左右傾斜へ十分に動かす。GUIが要求するX/Y/Size/Skewをすべて満たしてからCALIBRATEを実行し、SAVEで出力したYAMLを保存する。走行中、モーター稼働中、振動している状態では実施しない。

## 成果物とレビュー

出力YAMLは次の構成で版管理する。数値を推測してテンプレートへ書かない。

```text
calibration/
  ov9281/<camera-serial-or-asset-id>/1280x800/
    camera.yaml
    record.md
```

`record.md`には、実施日時、カメラ個体ID、レンズ・防水窓・取付状態、ソフトウェアのコミット、画像サイズ・画素形式・FPS、露光・ゲイン、チェッカーボードの内側コーナー数と実測square長、採用画像数、再投影誤差、合意済み閾値と判定を記載する。元画像・rosbagは外部保管先を記録し、Gitへ直接追加しない。

YAMLの`image_width`/`image_height`、`camera_matrix`、`distortion_model`、`distortion_coefficients`、`projection_matrix`が埋まっていることをレビューする。`camera_name`は個体を識別できる値にする。解像度を変える場合は、同じ係数を流用せず、その解像度で撮影し直す。

## 適用と受入れ

```bash
ros2 launch mower_camera libcamera.launch.py \
  camera_info_url:=file:///absolute/path/to/camera.yaml \
  exposure_time_us:=4000 analogue_gain:=2.0
ros2 topic echo --once /camera_info
```

ノードはYAMLのロード失敗、画像サイズ不一致、空の歪み係数、または無効な焦点距離を検出すると起動を拒否する。`/image_raw`と`/camera_info`の幅、高さ、stamp、`frame_id`が一致することを確認する。再投影誤差が合意した閾値を超える場合、または防水窓・取付状態が較正時と異なる場合はVIOへ投入せず、原因と再較正結果を記録する。

この較正はCamera-IMU外部パラメータや時刻同期を含まない。それらはBMI270のFIFO/IRQ時刻対応が完了後に別手順で実施する。
