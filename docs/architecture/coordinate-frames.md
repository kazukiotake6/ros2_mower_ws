# 座標フレーム仕様

## 基本方針

ROS REP-103の右手系を用いる。車体frameはx前方、y左方、z上方とする。光学frameはx右方、y下方、z前方とする。

## frame

| frame | 用途 | 発行者 |
| --- | --- | --- |
| `base_link` | 車体基準 | ロボットモデル |
| `imu_link` | BMI270センサー基準 | `mower_description`の固定TF |
| `camera_link` | カメラ筐体・取付基準 | `mower_description`の固定TF |
| `camera_optical_frame` | 画像の光学基準 | `mower_description`の固定TF |
| `vio_odom` | VIO開始時を原点とする連続局所座標 | VIOまたは評価用TF発行機能 |
| `odom` | 車輪・VIO融合後の連続局所座標 | 将来の融合ノード |
| `map` | 地図・再ローカライゼーション座標 | 将来の地図localization |

## 変換

- `base_link -> imu_link`と`base_link -> camera_link`は実測取付値で定義する。
- `camera_link -> camera_optical_frame`はROS光学frame規約に従う。
- Camera-IMU較正結果を取り込む際は、較正ツールが出力する変換方向を明記し、逆変換の取り違えをテストする。
- サンプル値、別個体、別取付状態の外部パラメータを使用しない。

実測値が未確定の間はURDF/Xacroへ推測値を固定せず、VIO実機受入れを行わない。
