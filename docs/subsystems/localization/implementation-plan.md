# VIO実装計画

## 目的と安全境界

本計画は、OV9281単眼カメラとPi直結BMI270を入力として、Raspberry Pi 5上のROS 2 JazzyでVisual-Inertial Odometry（VIO）を提供するための実装順、成果物、移行条件を定義する。

VIO、Raspberry Pi、ROS 2、CANは非安全系である。VIOの品質低下を検出した場合、上位機能へ減速・停止要求の根拠を通知するが、最終的なモータ・ブレード停止は外部MCUと独立E-stop・遮断回路が担う。VIOノードから実車CAN指令を直接送信しない。

## 現在地（2026-08-19）

- `mower_camera`はOV9281の1280x800、YUYV、約60 HzのROS配信を実機確認済み。
- 固定露光・固定ゲイン条件の個体別内部較正は、合意済みRMS閾値による受入れが未完了であり、現行較正値をVIOの承認済み入力として扱わない。
- `mower_imu`のBMI270 PoCは`feat/bmi270-raw-data`に存在するが、現在の開発ベースには未統合である。PoCは直接レジスタ初期化、ポーリング取得、Publish時のROS時刻を使用しており、公式設定ロード、FIFO、IRQ、SENSORTIME、時刻対応は未実装である。
- `mower_localization`にはBasalt非依存のVIO入力バッファと単体テストを追加済みである。Basalt推定器、ROS 2ノード、TF、`mower_description`、`mower_bringup`の統合は未実装である。
- CANと車輪オドメトリは承認済みプロトコルおよび実MCUが未確定のため、VIOとの融合および自走試験はまだ開始しない。
- 2026-08-19に`mower_localization`をビルドし、入力バッファのgtest 7件が全件合格した。

## 採用方針

- VIOの第一候補はBasalt（BSD-3-Clause）、比較候補はOKVISとする。
- Basaltの版、ビルド方法、third-partyライセンス、arm64対応を技術検証とADRで固定するまでは、アプリケーションコードを特定の未固定版へ密結合しない。
- ROS 2との境界は`mower_localization`内のアダプタに閉じ込め、センサー購読、時刻検査、状態管理、ROSメッセージ変換をBasalt本体から分離する。
- 最初にrosbag2再生で決定論的に評価し、その後Pi 5ベンチ、手押し実機試験へ進む。

## 実装段階と移行条件

### フェーズ0: 仕様と評価基準

成果物:

- `docs/subsystems/localization/specification.md`
- `docs/architecture/ros-interfaces.md`
- `docs/architecture/coordinate-frames.md`
- `docs/architecture/time-synchronization.md`
- `docs/verification/vio-acceptance-criteria.md`
- Basalt採用版・依存管理方法のADR

移行条件:

- topic、QoS、frame、TF発行者、状態、異常時動作がレビュー済みである。
- カメラRMS、Camera-IMU時刻、VIO精度・遅延・資源使用量の合格値が承認されている。
- 未確定値を実装既定値として固定していない。

### フェーズ1: カメラ入力の受入れ

1. 最終取付状態、固定露光・固定ゲインで内部較正を実施する。
2. 個体別`camera.yaml`と`record.md`に実測RMS、閾値、判定、構成識別子を記録する。
3. ImageとCameraInfoのstamp、frame、画像サイズ、較正内容の一致を確認する。
4. 実効FPS、フレーム欠落、キュー遅延、request失敗、再初期化回数をdiagnosticsへ追加する。

移行条件:

- 較正結果が合意済み基準にPASSしている。
- 固定条件で連続配信でき、欠落・遅延を観測できる。

### フェーズ2: BMI270入力の受入れ

1. Bosch公式BMI270 Sensor APIのSPI read/write/delay callbackと設定ロードを実装する。
2. 公式APIで加速度・ジャイロのODR、レンジ、フィルタを設定する。
3. FIFO、INT1、SENSORTIME、ロールオーバー処理を実装する。
4. IRQ受信時のPi単調時刻とSENSORTIMEを対応付け、ROS時刻へ変換する。
5. バイアス、スケール、軸向き、ノイズ値を個体別YAMLとして管理する。
6. SPI異常、FIFO欠損、IRQ途絶、時刻逆行、再初期化をdiagnosticsへ出す。

移行条件:

- 200 Hz取得、単位・軸、時刻単調性、欠損検出が単体・実機試験に合格している。
- 不合格・未較正データを正常なVIO入力として公開しない。

### フェーズ3: Camera-IMU較正と時刻同期

1. カメラのフレーム開始、BMI270 IRQ、画像Publishをロジックアナライザで同時計測する。
2. 平均オフセット、最大ジッタ、連続運転時ドリフトを記録する。
3. 全軸を十分に励振したCamera-IMU較正データセットを取得する。
4. Camera-IMU外部変換、時間オフセット、IMUノイズを推定し、個体・取付状態と対応付ける。
5. 較正ファイルの座標変換方向、単位、適用順を自動テストする。

移行条件:

- `docs/architecture/time-synchronization.md`の閾値を満たす。
- 内部・外部・時刻較正の全成果物が同一ハードウェア構成へ追跡できる。

### フェーズ4: Basalt技術検証と依存固定

1. x86_64とPi 5 arm64で、GUIなしのBasaltライブラリとCLIをビルドする。
2. 公開データセットで上流の基準動作を再現する。
3. ROS 2依存との競合、メモリ、起動時間、利用可能な推定品質情報を調査する。
4. `basalt_vendor`等の隔離方法、固定tag/commit、パッチ方針をADRに記録する。
5. Basalt本体とthird-party依存をSBOM・ライセンス一覧へ追加する。

移行条件:

- CIとPi 5で再現可能にビルドできる。
- 更新・脆弱性・ライセンス管理方法が決まっている。

### フェーズ5: ROS 2 VIOノード

`mower_localization`へlifecycle node `basalt_vio_node`を実装する。

- Image、CameraInfo、Imuを購読する。
- IMUを時刻順の有界キューへ保持し、画像時刻までのサンプルを推定器へ投入する。
- 時刻逆行、過大gap、古い画像、未較正入力、非有限値を拒否する。
- YUYVからグレースケールへの変換とメモリコピー量を測定可能にする。
- `/vio/odometry`、`/vio/status`、`/diagnostics`をPublishする。
- TF発行はパラメータで明示的に有効化し、車輪融合ノードとの二重発行を防ぐ。
- 入力停止または追跡喪失後に、古いOdometryを正常値として継続Publishしない。

移行条件:

- 単体テストでキュー、時刻、較正、座標変換、状態遷移、異常入力を確認する。
- lifecycle inactive/error中に推定結果を正常出力しない。

### フェーズ6: 再生・Pi 5・手押し評価

1. 公開データセットで精度と再現性を確認する。
2. mower形式rosbag2を低速、実時間、高倍率で再生し、結果の一貫性を確認する。
3. Pi 5でCPU、メモリ、温度、処理遅延、入力・出力欠落を測定する。
4. 静止、直線、旋回、閉ループ、低テクスチャ、影、振動を含む手押し試験を行う。
5. カメラ停止、IMU欠損、時刻ジャンプ、過負荷、ノード再起動を故障注入する。

移行条件:

- `docs/verification/vio-acceptance-criteria.md`の合格条件を満たす。
- 試験記録にGit、OS、ROS、libcamera、Basalt、ハードウェア、設定、較正、rosbag識別子がある。

### フェーズ7: 上位統合

1. VIO品質を上位監督機能へ通知する。
2. 承認済みCAN仕様と車輪Odometry完成後、VIOと車輪オドメトリを融合する。
3. RTAB-Mapによる地図作成、保存、再利用、再ローカライゼーションへ進む。
4. 低速自走はMCU独立停止、CAN断、復帰試験の合格後に限る。

## 推奨PR分割

1. VIO計画、ROSインターフェース、TF、時刻同期、受入れ基準
2. BMI270公式API初期化
3. BMI270 FIFO・IRQ・SENSORTIME・diagnostics
4. 固定条件カメラ較正とCamera-IMU較正
5. Basalt arm64検証、vendor化、ADR、SBOM
6. `basalt_vio_node`と単体テスト
7. rosbag2再生試験と基準結果
8. Pi 5ベンチと手押し試験記録
9. VIO品質の上位統合
10. 車輪融合とRTAB-Map

## 未確定事項

次の値は合意前に固定しない。

- カメラ再投影RMS上限
- Camera-IMU平均時刻オフセット、最大ジッタ、ドリフト上限
- VIOの位置・方位誤差、閉ループドリフト、追跡損失率
- 最大処理遅延、CPU、メモリ、温度、連続運転時間
- `DEGRADED`および`LOST`への遷移閾値
- Basaltの固定版、カメラモデル、特徴点・キーフレーム設定
