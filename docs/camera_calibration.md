# OV9281 カメラキャリブレーション手順

この手順は、`mower_camera` のOV9281（1280x800、YUYV、60 Hz）に対する単眼内部パラメータ較正用です。較正値はカメラ個体、レンズの焦点・固定状態、防水窓、取付位置、画像サイズごとに有効です。これらのいずれかを変更した場合は再較正します。

## 前提

- 車体、カメラおよび防水窓を最終取付状態に固定する。防水窓を使う場合は必ず装着して較正する。
- 露光とゲインを固定し、画像が飽和・ぶれない明るさにする。
- 印刷精度を確認したチェッカーボードを用いる。`--size`は内側コーナー数、`--square`は正確に測った一辺のメートル値である。
- VIO担当と、最低枚数、距離・傾きの分布、許容再投影誤差を合意してから開始する。未合意の値を合格値として扱わない。

### カメラ個体IDとRMS閾値の管理

本リポジトリで管理するOV9281の個体IDは`cam-ov9281-001`とする。基板およびレンズには製造者シリアル番号などの識別表示がないため、このIDはプロジェクト内で付与した資産IDである。較正成果物、`record.md`、`camera_name`ではこの値を一貫して使用する。

再投影RMSの受入れ閾値は、VIO担当と合意した値を`--rms-threshold-px`に指定する。閾値そのものはカメラから取得する値ではなく、撮影条件とVIOの要求に対する受入れ基準である。較正ツールが算出したRMS値（px）と指定した閾値、判定を`record.md`へ記録する。合意前はRMS値を取得・記録しても合格判定を行わない。

## 撮影と算出

Pi上でカメラを起動する。

```bash
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 launch mower_camera libcamera.launch.py \
  exposure_time_us:=4000 analogue_gain:=2.0
```

別端末でROSの較正ツールを導入して起動する。以下の`8x6`と`0.024`は例であり、使用するボードの値へ置き換える。

```bash
sudo apt install ros-jazzy-camera-calibration
ros2 run camera_calibration cameracalibrator \
  --size 8x6 --square 0.024 --no-service-check \
  camera:=/ image:=image_raw
```

GUIを使えない場合は、同梱のヘッドレスツールを用いる。個体ID、固定露光・ゲイン、合意済みRMS閾値、Git revisionを必須入力とし、YAMLと`record.md`を同時に出力する。

```bash
ros2 run mower_camera headless_camera_calibration -- \
  --image-topic /image_raw --size 8x6 --square 0.030 --samples 80 \
  --camera-name ov9281_cam-ov9281-001 --camera-id cam-ov9281-001 \
  --exposure-time-us 4000 --analogue-gain 2.0 \
  --rms-threshold-px <agreed-threshold> --software-revision "$(git rev-parse HEAD)" \
  --output calibration/ov9281/cam-ov9281-001/1280x800/camera.yaml \
  --record calibration/ov9281/cam-ov9281-001/1280x800/record.md
```

実行前に、同じ`exposure_time_us`と`analogue_gain`を指定して`libcamera.launch.py`を起動する。

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

## 実機実施記録（2026-08-15、OV9281）

このリポジトリの`config/ov9281_1280x800.yaml`は、Pi 5でlibcameraが認識した
OV9281を用いて取得した単眼内部パラメータである。基板およびレンズに製造者
シリアル番号などの識別表示がないため、当該個体には後日プロジェクト資産ID
`cam-ov9281-001`を付与した。カメラ、レンズ、
防水窓、取付状態を変更した場合は、この結果を使用しない。

| 項目 | 記録値 |
| --- | --- |
| センサー | OV9281（libcameraで認識） |
| カメラ個体ID | `cam-ov9281-001`（プロジェクト資産ID、製造者シリアル番号の表示なし） |
| ストリーム | 1280x800、RGB888、単眼 |
| キャリブレーションターゲット | 内側コーナー8x6、正方形一辺0.030 m |
| 最終セッションの保存画像 | 80枚（`/tmp/calibrationdata.tar.gz`、Git管理外） |
| 採用したK | `fx=726.361879`、`fy=726.836967`、`cx=650.337715`、`cy=372.377493` |
| 歪みモデル・係数 | `plumb_bob`; `[0.08992585, -0.11889319, 0.00210632, -0.00071327, 0]` |
| 露光・ゲイン | libcamera自動制御（固定値は未設定） |
| CameraInfo配信 | `/camera/camera_info`で1280x800および上記K/D/Pを確認 |

### 実施手順

1. `cam -l`でOV9281が検出されることを確認した。
2. `camera_ros`を、1280x800・RGB888・`camera_optical_frame`で起動した。
   Piのローカルlibcameraを優先する必要がある環境では、ROSをsourceした後に
   `LD_LIBRARY_PATH=/usr/local/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH`を設定する。
3. `camera_calibration`を`--size 8x6 --square 0.030`で起動し、硬い平板へ貼り付けた
   チェッカーボードを中央、各辺、四隅、距離・傾きの異なる姿勢へ静止させて提示した。
4. GUIでCALIBRATE、SAVE、COMMITを実行した。SAVEは`/tmp/calibrationdata.tar.gz`を生成し、
   COMMITは`~/.ros/camera_info/`へYAMLを書き込んだ。
5. 最終YAMLを本パッケージの`config/ov9281_1280x800.yaml`へ転記し、launch既定値から参照する。

### 検証結果と残存事項

- 最初の56画像セッションは独立OpenCV再計算で再投影RMS 2.573253 pxとなり、採用しなかった。
- 最終80画像セッションは上記の値をCOMMITしてCameraInfo配信まで確認した。
  保存アーカイブに対する独立RMS再計算は、このPi環境でOpenCVの最終最適化呼出しが
  終了コードを返さず完了しなかったため、RMS値を記録できていない。
- したがって、このYAMLは解像度・数値構造・ROS配信の検証を通過しているが、VIOへ投入する
  前に、固定露光・固定ゲインで取得した別セッションについて合意済み閾値のRMSを記録し、
  直線性と実走VIOを受入れ確認する必要がある。

### GUI較正の保留記録（2026-08-17）

`cam-ov9281-001`に対して、固定露光`4000 us`・アナログゲイン`2.0`、内側コーナー8x6・square 0.030 mの条件でGUI較正を試行した。`/tmp/calibrationdata.tar.gz`には116枚の画像が保存されたが、GUIのフリーズ、重複起動、終了後の残留プロセスが発生したため、安定して完了できなかった。

この試行では新しいYAMLのCOMMITおよび再投影RMSの記録を確認できていない。したがって、このアーカイブや既存のYAMLを今回の較正成果物として扱わず、VIOへ投入しない。GUI較正は保留とし、再開時はGUIの安定化を確認するか、合意済みのRMS閾値を指定して`headless_camera_calibration`を実行する。新しい`camera.yaml`、`record.md`、実測RMS、閾値および判定がそろってから受入れを行う。

この記録はCamera--IMU外部パラメータ、露光開始時刻とBMI270 IRQのオフセット、または安全機能の検証を含まない。
