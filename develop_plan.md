  # 自律走行ロボット芝刈り機制御開発プラン
  
  ## 1. 目標と責務分担

  対象は、Raspberry Pi 5上のROS 2で自律走行するロボット芝刈り機です。

   層                            実装主体            責務
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   自律走行                      Raspberry Pi 5      カメラ・IMU取得、VIO/SLAM、地図、自己位置推定、経路計画、速度目標生成、ログ
  ────────────────────────────  ──────────────────  ─────────────────────────────────────────────────────────────────────────────
   リアルタイム制御・安全監視    外部MCU             車輪／ブレード制御、エンコーダ取得、安全入力監視、通信断検出、安全停止
  ────────────────────────────  ──────────────────  ─────────────────────────────────────────────────────────────────────────────
   独立ハードウェア安全系        E-stop・遮断回路    Pi、CAN、MCUソフトウェアを経由しないモータ／ブレード停止

  Piは非安全系です。Piが故障・フリーズ・再起動・通信断となっても、外部MCUが停止へ遷移します。さらに、E-stopは安全遮断回路でモータドライバ有効化または電力供
  給を直接遮断します。

  ## 2. ハードウェア構成

   要素               採用構成                              方針
  ━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   計算機             Raspberry Pi 5                        Ubuntu 24.04 arm64、ROS 2 Jazzy
  ─────────────────  ────────────────────────────────────  ───────────────────────────────────────
   カメラ             MIPI CSI-2単眼、IMX296またはOV9281    初期PoCはOV9281、解像度比較にIMX296
  ─────────────────  ────────────────────────────────────  ───────────────────────────────────────
   IMU                BMI270                                SPI接続を優先し、高周期で取得
  ─────────────────  ────────────────────────────────────  ───────────────────────────────────────
   CANコントローラ    MCP2515                               PiのSPI接続、Linux SocketCANでcan0化
  ─────────────────  ────────────────────────────────────  ───────────────────────────────────────
   CANトランシーバ    TJA1050                               CAN標準2.0B、外部MCUと接続
  ─────────────────  ────────────────────────────────────  ───────────────────────────────────────
   駆動制御           外部MCU                               RTOSまたはハードウェアタイマで制御
  ─────────────────  ────────────────────────────────────  ───────────────────────────────────────
   車輪情報           MCU接続エンコーダ                     CANでPiへ送信し、補助オドメトリに利用

  TJA1050は5 V系のため、Pi GPIO・3.3 V動作のMCP2515に対してRXD/TXDの電圧整合が必要です。レベル変換、保護回路、共通GND、CAN終端抵抗、EMC／ESD対策を回路レ
  ビューの必須項目とします。

  ## 3. 外部MCUのリアルタイム・安全機能

  MCUにはRTOSの周期タスク、またはハードウェアタイマ割込みを使用します。

  - 高速周期：モータ電流・速度制御、PWM、エンコーダ読取り、ドライバ故障検出
  - 中速周期：衝突、傾斜、リフト、境界、温度、バッテリ、非常停止の監視
  - CAN周期：Piの速度目標受信、MCU状態・車輪オドメトリ・故障状態送信
  - 独立ウォッチドッグ：MCUソフトウェア異常を検出して停止
  - CAN指令監視：タイムアウト、連番異常、値域外、許可状態不成立でゼロ速度・ブレード停止
  - 起動・復帰制御：通信復帰だけでは再始動せず、安全条件と明示的な再許可を要求

  安全入力は可能な限りMCUに直接配線し、PiやCANを安全判断の必須経路にしません。安全機能の設計・検証では、IEC 60335-2-107と対象市場で必要な規格を確認します。

  ## 4. CAN通信とROS 2統合

  CAN 2.0Bのアプリケーションプロトコルを先に仕様化します。

  - Pi → MCU：左右車輪速度、または並進速度・角速度、動作許可、停止要求、ハートビート
  - MCU → Pi：エンコーダ、実速度、電流、温度、バッテリ、障害、安全状態、E-stop状態
  - 指令周期：50?100 Hzを起点に、実機の制御周期・バス負荷で最適化
  - 各指令：連番、有効期限、単位、スケール、範囲、フェイルセーフ値を明文化
  - ROS 2側：SocketCANゲートウェイでCANフレームとROSメッセージを双方向変換
  - 検証：can-utils、バス負荷、ビットエラー、通信断、再接続、異常フレームを試験

  ## 5. センサー統合・時刻同期・キャリブレーション

  ### カメラ

  - libcameraベースでMIPIカメラをROS 2へ接続
  - ImageとCameraInfoを配信
  - 露光、ゲイン、フレームレートを固定・記録可能にする
  - OV9281は高フレームレート・低計算負荷を重視した初期VIO候補
  - IMX296は解像度・ランドマーク識別性を重視する比較候補
  - 防水窓の反射、芝や泥の付着、振動、逆光、日照変化を実車評価する

  ### BMI270

  - SPI接続を基本とし、sensor_msgs/Imuを配信
  - バイアス、スケール、軸向き、ノイズ特性を校正
  - カメラ露光時刻とIMU時刻のずれ・ジッタを計測
  - Camera?IMU外部パラメータをキャリブレーションし、成果物と手順をバージョン管理

  ### 車輪オドメトリ

  - MCUでエンコーダを取得し、CAN経由でPiへ送信
  - nav_msgs/Odometry相当へ正規化
  - 芝でのスリップを前提に、単独では絶対位置とみなさずVIOの補助情報として扱う

  ## 6. Visual-Inertial Odometry／SLAM構成

  商用利用を前提に、コピーレフトの強いOSSは主候補から除外します。

   用途                            第一候補                    比較候補
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   VIO                             Basalt（BSD-3-Clause）      OKVIS（BSD-3-Clause）
  ──────────────────────────────  ──────────────────────────  ────────────────────────────────────────────────
   地図・再ローカライゼーション    RTAB-Map（BSD-3-Clause）    stella_vslam（BSD-2-Clause、依存関係を要精査）

  段階導入は次の順とします。

  1. 車輪オドメトリのみ
  2. 単眼カメラ＋BMI270のBasalt VIO
  3. VIOと車輪オドメトリの融合
  4. RTAB-Mapによる地図作成、保存、再利用
  5. 既存地図に対する再ローカライゼーション
  6. 自己位置品質低下時の速度制限・停止要求

  芝生ではテクスチャ不足、草の揺れ、影、泥、水滴、車輪スリップが起こるため、VIO単独を唯一の安全根拠にしません。Piは推定品質低下を検知してMCUに減速・停止を要
  求し、MCUは通信・安全状態を独立して監視します。

  ## 7. Raspberry Pi 5の実行環境

  - Ubuntu 24.04 arm64、ROS 2 Jazzy
  - x86_64開発PC：クロス開発、Gazebo、可視化、ログ解析
  - ROS 2パッケージ分割
      - mower_description
      - mower_bringup
      - mower_camera
      - mower_imu
      - mower_can
      - mower_control
      - mower_localization
      - mower_navigation
      - mower_simulation

  - Docker／Dev Container、colcon、lint、ユニットテスト、再生テストをCI化
  - rosbag2で画像、IMU、CAN、オドメトリ、VIO、地図、速度指令、安全状態を一括記録
  - Pi 5上でCPU、メモリ、温度、消費電力、フレーム落ち、VIO遅延を計測し、解像度・fps・特徴点数を調整

  ## 8. シミュレーションとHIL

  Gazebo上で、実機と同じROSトピック・CANプロトコルを使える環境を構築します。

  - URDF/Xacroで車体、車輪、カメラ、IMU、エンコーダ、質量、慣性、摩擦をモデル化
  - 実機の駆動方式に応じ、差動二輪またはスキッドステア制御を実装
  - ros2_controlで速度指令と車輪状態を接続
  - vcan0で仮想CANを作成し、外部MCU模擬ノードとPi側ゲートウェイを接続
  - カメラフレーム欠落、IMUバイアス、車輪スリップ、CAN断、Pi停止、MCU異常、E-stopを故障注入
  - 次段階で実MCU、実CAN、モータドライバを用いるHILへ進む

  シミュレーションは制御・通信・状態遷移の検証に使い、停止距離、芝でのスリップ、物理的な安全性能は実機試験で確認します。

  ## 9. 実装・評価フェーズ

  1. 安全要求、停止状態、CAN仕様、評価指標の確定
  2. 回路・配線・電源・非常停止・MCU安全機能の設計レビュー
  3. Pi 5／ROS 2、カメラ、IMU、CANの疎通とログ取得
  4. MCU単体でのモータ制御、安全停止、CAN断試験
  5. 手押しでのVIO、キャリブレーション、地図作成評価
  6. 低速自走でのCAN速度指令、車輪オドメトリ、停止試験
  7. 芝生環境での地図・再ローカライゼーション・経路追従
  8. 日照、振動、芝丈、湿潤、汚れ、通信断、Pi再起動のロバスト性試験
  9. 長時間連続走行、HIL、最終安全検証

  評価指標は、位置・方位誤差、経路追従誤差、再ローカライゼーション成功率、停止時間、CAN遅延・エラー率、VIO遅延、CPU・メモリ・温度、地図容量、長時間安定性と
  します。

  ## 10. 商用化準備

  - RTAB-Map、Basalt、依存ライブラリ、デバイスSDK、モデル、地図データを含むSBOM作成
  - ライセンス表示・再配布条件・脆弱性対応の確認
  - ハードウェア／ソフトウェアのバージョン、キャリブレーション、試験ログのトレーサビリティ確保
  - 安全要求、設計、テスト結果、残存リスク、認証試験計画を文書化
  - 評価結果により、RTAB-Map単独またはBasalt＋RTAB-Map＋車輪オドメトリ構成の採用判断
## 11. IMU構成とBMI270実装計画

### 11.1 役割分担

Pi直結のBMI270は、カメラ同期、VIO/SLAM、姿勢・移動推定に用いる。転倒、異常加速度、姿勢異常に基づく安全停止の最終判断は、Pi、ROS 2、CANの停止や遅延に依存しない外部MCU直結の安全用IMUで行う。

### 11.2 Raspberry Pi 5 GPIO・配線計画

BMI270とMCP2515はSPI0を共有し、CSと割込み線を専用化する。カメラはCSI接続のため40ピンGPIOとは競合しない。

| 用途 | 信号 | BCM GPIO | 物理ピン | 接続先 |
| --- | --- | ---: | ---: | --- |
| 共通SPI0 | MOSI | GPIO10 | 19 | BMI270 SDI、MCP2515 SI |
| 共通SPI0 | MISO | GPIO9 | 21 | BMI270 SDO、MCP2515 SO |
| 共通SPI0 | SCLK | GPIO11 | 23 | BMI270 SCK、MCP2515 SCK |
| BMI270専用 | CS | GPIO8 / CE0 | 24 | BMI270 CSB |
| BMI270専用 | データレディIRQ | GPIO25 | 22 | BMI270 INT1 |
| MCP2515専用 | CS | GPIO7 / CE1 | 26 | MCP2515 CS |
| MCP2515専用 | CAN受信IRQ | GPIO24 | 18 | MCP2515 INT |
| 電源 | 3.3 V | - | 1または17 | BMI270 VDD/VDDIO、3.3 V対応MCP2515 |
| GND | GND | - | 6、9等 | 両モジュールのGND |

- BMI270はSPI0/CE0（`/dev/spidev0.0`）、MCP2515はSPI0/CE1に割り当てる。
- GPIO25とGPIO24はLinux GPIO割込みとして個別に監視し、共有しない。
- Pi GPIOは3.3 V専用であり、5 V信号を接続しない。MCP2515はPi直結部が3.3 Vロジック対応の構成を選ぶ。TJA1050を用いる場合は、Piとの接続に必要なレベル変換を設ける。
- CSは起動時の誤選択を防ぐプルアップを設け、起動診断でBMI270とMCP2515を個別に認識できることを確認する。GPIO18、GPIO17、GPIO16およびSPI1は将来の拡張用に予約する。

### 11.3 mower_imuの実装

- `bmi270_node`：SPI通信、初期化、FIFO取得、時刻付け、ROS 2 Publishを担当する。
- `bmi270_driver`：Bosch公式BMI270 Sensor APIを利用し、Pi依存部はSPI read/write、delay、GPIO割込み処理に限定する。
- `imu_calibration`：バイアス、スケール、ノイズ、Camera-IMU外部パラメータをYAMLから適用する。
- `imu_diagnostics`：通信、FIFO、IRQ、時刻同期、セルフテスト、再初期化を診断する。
- `imu_tools`：静止較正、六面較正、Allan分散解析、セルフテスト実行を提供する。

起動時はチップIDとSPI通信を確認し、リセット、公式設定ロード、加速度・ジャイロ・FIFO・データレディ割込みの有効化、較正値適用の順に初期化する。正常サンプルの連続取得を確認後にデータを公開する。

### 11.4 ROS 2インターフェースと時刻同期

- `/imu/data_raw`に`sensor_msgs/Imu`をPublishする。加速度は`m/s²`、角速度は`rad/s`、`frame_id`は`imu_link`とする。orientationはVIO等が別途推定するため未設定扱いとする。
- `/diagnostics`にSPI通信エラー数、FIFOオーバーフロー・欠損数、実効サンプルレート、時刻オフセット・ジッタ、セルフテスト結果、再初期化回数を出力する。
- `/imu/run_self_test`を停止状態でのみ受け付ける保守用サービスとして提供する。
- FIFOをバースト読み出しし、BMI270のSENSORTIMEとIRQ受信時のPi単調クロックを対応付けてROS時刻を推定する。SENSORTIMEのロールオーバーを処理し、時刻逆行または過大なジッタのサンプルは破棄して診断へ記録する。
- カメラのフレーム開始、BMI270 IRQ、画像タイムスタンプをロジアナで同時測定し、平均オフセットと最大ジッタを記録する。要求精度を満たさない場合は、共通トリガまたはハードウェアタイムスタンプ経路を追加する。

### 11.5 較正とセルフテスト

- 起動時の静止較正、六面静止較正、Allan分散試験、Camera-IMU較正を実施する。結果はセンサ個体番号、取付状態、実施日、ソフトウェア版とともにYAMLで版管理し、走行中の自動再較正は行わない。
- BMI270内蔵の加速度計セルフテストを、起動時および保守時の健全性確認として実装する。内部刺激に対する正負それぞれの3軸応答差分を、データシートの合格閾値と比較する。
- セルフテストはモーター・ブレード停止、機体静止、VIO・自律走行停止中に限定する。通常Publishを停止またはテスト中状態にしてから、データシート指定のレンジ、ODR、フィルタへ一時変更する。
- テスト後はBMI270をリセットし、通常設定、FIFO、IRQ、較正値を再適用する。連続した正常サンプルを確認してからPublishを再開する。
- 不合格、通信失敗、タイムアウト時はERROR診断を発行し、IMUデータを正常入力としてVIOへ渡さない。再初期化を一度だけ試行し、再失敗時は保守が必要な状態とする。結果、実施時刻、各軸の応答差、失敗理由、復帰結果をrosbagおよび保守ログに記録する。

セルフテストは走行中の安全監視や安全認証の代替ではない。安全停止は外部MCUと安全用IMUで継続して担保する。

### 11.6 異常処理と検証

- SPI通信異常、FIFOオーバーフロー、IRQ途絶を検出し、再試行後にBMI270を再初期化する。復旧不能時はVIO入力を無効化して上位へ通知する。
- 単体テストでSPIレジスタ操作、単位変換、FIFO解析、SENSORTIMEロールオーバー、較正値適用、セルフテスト判定を確認する。
- ベンチ試験で静止時の重力方向、ジャイロバイアス、実効ODR、温度変化、振動耐性を測定する。
- HIL試験でSPI断、FIFO過負荷、IRQ途絶、セルフテスト失敗、Pi再起動を模擬し、診断・復旧・安全系分離を確認する。
- VIO統合試験で、IMUなし、未較正、較正済み、時刻補正済みの追跡損失、姿勢ドリフト、再現性を比較する。


## 12. BMI270実装の再開手順（2026-08-10時点）

### 12.1 現在の作業状態

- 作業ブランチは `feat/bmi270-raw-data`。変更は未コミットである。
- `mower_imu` に `bmi270_raw_node` を追加済み。SPI0/CE0（既定 `/dev/spidev0.0`）を開き、チップID確認、直接レジスタ設定、加速度・ジャイロのポーリング取得、`/imu/data_raw` へのSI単位Publishを実装している。
- `spi_device`、`spi_speed_hz`、`accel_range_g`、`gyro_range_dps`、`poll_rate_hz`、`frame_id` はROSパラメータ化済みである。
- `colcon build --packages-select mower_imu` は成功している。
- Bosch公式BMI270 Sensor API（BSD-3-Clause）は `src/mower_imu/third_party/bmi270/` に取り込み済みで、ライセンスファイルも保持している。
- CMakeには公式APIのビルド対象追加を開始しているが、ノードはまだ公式APIを呼び出していない。

### 12.2 実装上の重要な未完了事項

BMI270はリセット後に公式の8 KiB設定ファイルをロードし、初期化成功を確認しなければならない。現行ノードの直接レジスタ初期化だけでは実機データ取得を保証できないため、フェーズ1は未完了として扱う。

次回は以下をこの順で実施する。

1. `bmi270_raw_node` にSPI read/write/delayコールバックを実装する。書込みコールバックは設定ファイルの連続バースト書込みに対応させる。
2. `bmi2_dev` をSPIインターフェースとして設定し、`bmi270_init()` を呼び出す。戻り値と初期化状態を検査して失敗時はPublishしない。
3. 公式API経由で加速度・ジャイロのODR、レンジ、フィルタを設定し、有効化する。
4. 公式API経由で加速度・ジャイロのサンプルを取得し、既存の`/imu/data_raw` Publishへ接続する。
5. `colcon build --packages-select mower_imu` を実行し、警告なしでビルドできることを確認する。
6. 実機でチップID、重力方向、200 Hz取得、SPI通信断からの復帰を確認する。
7. 実機確認後にコミットとPRを作成する。

### 12.3 後続フェーズ

- フェーズ2：FIFO、INT1（GPIO25）割込み、SENSORTIME、Pi時刻対応、`/diagnostics`。
- フェーズ3：静止・六面・Allan分散・Camera-IMU較正、カメラ同期計測。
- フェーズ4：加速度計セルフテスト、異常処理、HIL、VIO統合と実走試験。

## 13. libcamera実装の再開手順（2026-08-13時点）

### 13.1 現在の作業状態

- 作業ブランチは `feat/camera-libcamera`。
- `mower_camera` に `libcamera_node` を追加済み。MIPI CSIカメラをlibcameraで取得し、`/image_raw`（`sensor_msgs/Image`）、`/camera_info`、`/diagnostics`を配信する。
- `width`、`height`、`frame_rate`、`pixel_format`、`frame_id`、`camera_id`、`camera_info_url`、`exposure_time_us`、`analogue_gain`をROSパラメータ化済みである。`frame_rate`はFrameDurationLimits、固定露光・ゲインはlibcamera ControlListへ反映する。
- YUYV、RGB888、BGR888、R8/MONO8をサポートし、既定プロファイルはOV9281向けの1280x800・60 Hz・YUYVとする。
- `libcamera.launch.py` と既定YAMLを追加済み。`colcon build --packages-select mower_camera`とパラメータ検証のユニットテストは成功している。

### 13.2 実装上の重要な未完了事項

ホスト側実装とOV9281での基本配信は検証済みである。画像ヘッダの時刻は、libcameraの単調クロックをROS時刻として誤用しないため、現状はPublish時のROSクロックを用いる。Camera-IMU同期評価の前に、ハードウェアのフレーム開始時刻との対応付けを実装・実測する必要がある。

OV9281のPi 5上での検出、起動、1280x800・YUYV・60 Hz配信は13.6で完了している。次回以降は以下をこの順で実施する。

1. **OV9281較正**：チェッカーボードで内部パラメータ・歪みを算出し、較正YAMLを版管理する。`camera_info_url`指定時の画像サイズ、`frame_id`、歪み係数、再投影誤差を確認する。受入れ可能な再投影誤差と撮影距離・枚数はVIO担当と合意して記録する。
2. **配信品質と診断**：固定露光・ゲイン、逆光、振動、汚れ、フレーム欠落をrosbag2で試験する。実効FPS、連続フレーム番号、キュー遅延、要求失敗、再起動回数を`/diagnostics`へ追加し、欠落・遅延を観測可能にする。
3. **IMX296実機対応**：Pi 5での検出・起動、libcameraが実際に受理した解像度・画素形式・実効FPSを記録する。OV9281と混同しないセンサー別の既定YAMLまたは起動プロファイルを用意し、各プロファイルで`/image_raw`と`/camera_info`を確認する。
4. **Camera-IMU時刻同期**：フレーム開始、BMI270 IRQ、画像Publish時刻をロジアナで同時計測し、平均オフセット、最大ジッタ、連続運転時のドリフトを記録する。結果を基に、フレーム開始のハードウェア時刻をROS時刻へ対応付ける処理を実装し、時刻逆行・過大ジッタを診断する。
5. **VIO統合**：未較正、較正済み、時刻補正済みの条件で追跡損失、姿勢ドリフト、再現性を比較する。要求性能を満たさなければ、共通トリガまたはハードウェアタイムスタンプ経路を設計する。
6. **実走耐性評価**：モータ・ブレードの安全条件を満たした状態で、芝生、日照変化、振動、防水窓の汚れを含む低速試験を実施する。推定品質低下時の上位停止要求を確認するが、最終停止は外部MCUと独立安全回路に委ねる。
7. 実機結果、測定ログ、残存リスクを反映してコミットとPRを作成する。

### 13.3 後続フェーズ

- フェーズ2：フレーム開始のハードウェア時刻対応、BMI270との時刻同期、フレーム欠落・キュー遅延診断。
- フェーズ3：OV9281／IMX296のVIO比較、Camera-IMU外部パラメータ較正、rosbag再生試験。
- フェーズ4：芝生・日照・振動・防水窓汚れを含む実走評価と、推定品質低下時の上位停止要求統合。

### 13.4 実機CSI起動検証（2026-08-12）

Raspberry Pi 5・Ubuntu 24.04 arm64で`ros2 run mower_camera libcamera_node`を実行した。libcamera 0.2.0は起動したが、カメラは列挙されず、ノードは`no libcamera camera found`で安全に終了した。このため、`/image_raw`、`/camera_info`、`/diagnostics`の実機配信は未達である。

初回検証時点では、`/dev/media0`から`/dev/media2`および`/dev/video19`から`/dev/video37`は存在し、`/boot/firmware/config.txt`には`camera_auto_detect=1`が設定されていた。一方で`libcamera-ipa`は未導入であり、`No IPA found in '/usr/lib/aarch64-linux-gnu/libcamera'`を確認した。IPA導入後の再検証結果は13.5を参照する。詳細な仕様と手順は`docs/camera_libcamera.md`を参照する。

### 13.5 IPA導入後の再検証（2026-08-12）

`libcamera-ipa` 0.2.0-3fakesync1build6 の導入を確認後、`libcamera_node`を再起動した。IPAモジュール（`ipa_rpi_vc4.so`を含む）は`/usr/lib/aarch64-linux-gnu/libcamera`に存在し、初回に出ていたIPA警告は解消された。しかしlibcameraの列挙カメラ数は依然0台であり、`/image_raw`、`/camera_info`、`/diagnostics`は配信されなかった。

カーネル側にはCSI受信機、OV9281、IMX296に対応するvideo/mediaデバイスおよびログが見つからなかった。存在する`video19`から`video37`はPiSPおよびrpivid用であり、CSIセンサーの認識を示さない。次の実施には、電源断状態でのCSIケーブル・コネクタ確認と、使用センサーに対応するdevice-tree overlayの適用が必要である。

### 13.6 PiSP対応libcamera導入後の実機配信検証（2026-08-13）

Ubuntu標準のlibcamera 0.2.0はPi 5のPiSPパイプラインを含まずOV9281を列挙できなかったため、Raspberry Pi版libcamera `v0.7.1+rpt20260429`を`/usr/local`へ導入した。`ipa_rpi_pisp.so`の存在を確認し、`cam -l`は`/base/axi/pcie@120000/rp1/i2c@88000/ov9281@60`のOV9281を1台列挙した。カーネルログでもI2Cアドレス`10-0060`のOV9281とCSI受信器`/dev/video0`を確認した。

`mower_camera`をlibcamera 0.7.1へクリーン再ビルド後、`ros2 launch mower_camera libcamera.launch.py`で起動した。`/image_raw`、`/camera_info`、`/diagnostics`が作成され、`/image_raw`は1280x800、`yuv422_yuy2`、`camera_optical_frame`で配信された。12秒測定の実効レートは`/image_raw`が59.70から60.11 Hz、`/camera_info`が59.96から60.00 Hzであり、60 Hz配信要件を満たした。

`camera_info_url`は未指定のため、CameraInfoの歪みモデルと内部パラメータは未較正である。IMX296の実機確認、較正、固定露光・ゲイン、フレーム欠落、Camera-IMU時刻同期は後続フェーズで実施する。

### 13.7 追加テスト計画と完了条件（2026-08-13追記）

現行の単体テストはパラメータの基本検証に限られる。以下を実装と同時に追加し、実機試験だけでソフトウェアの異常系を見落とさない。

| 区分 | 追加テスト項目 | 完了条件 |
| --- | --- | --- |
| 単体 | 全画素形式と大文字小文字、空文字列、NaN/Infを含むFPS・ゲイン、露光・解像度の境界値を検証する。 | 不正なパラメータを例外で拒否し、有効な設定を正しいROS encodingへ変換する。 |
| 単体 | 画像メッセージの`width`、`height`、`step`、`encoding`、バイト数を、各対応画素形式とstrideの不連続な場合に検証する。 | `step × height`とデータ長の整合を確認し、短いplaneや不正なmetadataをpublishしない。 |
| 結合 | libcameraを抽象化またはテスト用カメラで置き換え、カメラ未検出、指定`camera_id`なし、非対応stream設定、バッファ割当て失敗、requestキャンセル、再queue失敗を再現する。 | 例外・異常診断・クリーンな停止を確認し、停止中にバッファを再利用しない。 |
| 結合 | `camera_info_url`の正常、欠損、画像サイズ不一致を確認する。 | ImageとCameraInfoのstamp・frame_id・画像サイズを一致させ、不正または未較正状態を診断で区別する。 |
| 結合 | diagnosticsの実効FPS、欠落数、キュー遅延、要求失敗、再初期化回数を検証する。 | 起動時だけでなくストリーミング中に更新され、異常注入時にWARN/ERRORへ遷移する。 |
| 実機 | OV9281とIMX296の各プロファイルで、起動、実際に選ばれたformat、連続配信、露光・ゲイン制御を確認する。 | 目標FPSで連続配信し、設定値と観測値・診断値を測定ログへ保存する。 |
| 実機 | 十分な連続運転でFPS、欠落、CPU・メモリ使用量、遅延の増加、停止・再起動を測定する。 | リソース使用量と遅延に継続的増加がなく、停止・再起動後も正常に再配信できる。測定時間と許容値は対象Piの負荷条件とともに事前に決める。 |
| 実機 | 較正の再投影誤差、解像度変更後の較正無効化、Camera-IMUのオフセット・最大ジッタ・ドリフトを測定する。 | 較正YAMLの適用を確認し、合意済みの精度閾値を満たす。満たさない場合はVIO投入を禁止して原因と対策を記録する。 |
| HIL/統合 | カメラケーブル断、センサー電源断、libcamera再起動、低FPS化、フレーム停止を注入し、VIOと上位停止要求を確認する。 | カメラ障害を正常画像として扱わず、診断と上位通知を行う。外部MCUの独立安全機能に影響を与えない。 |
### 13.8 実機再検証の再開記録（2026-08-15）

- 作業ブランチは `feat/camera-ros-publisher-hardening`、worktreeは `.worktrees/camera-ros-publisher`。画像・CameraInfo配信実装は `189bb8b`。
- Pi 5カーネルはOV9281（`10-0060`）とCSI `/dev/video0` を認識しているが、ROS Jazzyのlibcamera 0.7.1は `RPI pisp.cpp: Unable to acquire a CFE instance` によりカメラ0台となる。ROSノードも `no libcamera camera found` で終了し、画像・CameraInfoは未配信。
- 再起動後は `sudo apt-get update && sudo apt-get upgrade` 後に、`uname -r`、`dpkg-query -W linux-image-raspi linux-firmware-raspi libcamera0.2 libcamera-ipa ros-jazzy-libcamera`、`cam -l` を実行する。OV9281が列挙されなければ `LIBCAMERA_LOG_LEVELS='*:DEBUG' cam -l` のログを保存し、ROS試験へ進まない。
- 列挙された場合はworktreeで `source /opt/ros/jazzy/setup.bash && colcon build --packages-select mower_camera && source install/setup.bash` を実行し、`ros2 launch mower_camera libcamera.launch.py` と、別端末の `ros2 topic hz /image_raw`、`ros2 topic echo --once /camera_info`、`ros2 topic echo --once /diagnostics` を確認する。合格条件は1280x800・`yuv422_yuy2`・60 Hz付近の連続配信と正常診断である。

## 14. MCP2515 CAN通信の設計・実装計画
### 14.1 目的と安全境界

`mower_can` は、Pi接続のMCP2515をLinuxの`mcp251x`ドライバとSocketCANで`can0`として使用し、CAN 2.0BとROS 2の指令・状態を相互変換する。アプリケーションからMCP2515をSPIで直接制御しないため、実機CAN、`vcan0`、HILで同一のゲートウェイを検証できる。

これは非安全系の通信アダプタである。MCUはCAN指令の喪失、期限切れ、連番異常、値域外または安全状態不成立時に、Piを介さずゼロ速度・ブレード停止へ遷移する。独立E-stop、MCUウォッチドッグ、安全入力を迂回してはならない。フェーズ1の完了条件は、`can0`の安定起動、`vcan0`/実CANでの送受信と再接続、期限・範囲・状態による指令拒否、異常の`/diagnostics`観測とする。実走はMCUのフェイルセーフ試験合格後まで開始しない。

### 14.2 ハードウェア・SocketCAN設計

SPI0はBMI270と共有し、MCP2515はCE1、受信割込みはGPIO24を専用利用する（11.2節）。実装前に次の値を`config/mcp2515.yaml`とハードウェア台帳に記録し、回路・MCU担当とレビューする。

| 項目 | 確定・確認内容 |
| --- | --- |
| 発振器周波数 | モジュール仕様と実装部品で確認する。8/16 MHzを推測で設定しない。 |
| ビットタイミング | ビットレート、SJW、サンプルポイントをMCUと一致させる。初期候補は500 kbit/s。 |
| INT信号 | MCP2515のアクティブLow割込み、GPIO24のプルアップ、極性を回路図と起動試験で確認する。 |
| 物理層 | TJA1050の5 VロジックをPi/MCP2515の3.3 V領域へ直結しない。レベル変換または3.3 V対応品、共通GND、120 Ω終端、ESD/EMCを確認する。 |
| CAN IDと周期 | ID、標準/拡張形式、Pi→MCUのmotion command／heartbeat送信周期10 ms、MCU側heartbeat監視timeout 50 ms以下をプロトコルレビューで固定する。50 msは5送信周期であり、最大並進速度0.55 m/sではtimeout中の移動距離が最大27.5 mmとなる。 |

PiのDevice Tree overlayはSPI0 CE1へMCP2515を割当てる。設定例は`dtoverlay=mcp2515-can1,oscillator=<確定値>,interrupt=24`とするが、実際のoverlay名・引数は対象カーネルの`/boot/firmware/overlays/README`で検証する。BMI270のCE0を無効化せず、`spidev0.1`との競合がないことをPi 5実機で確認する。

### 14.3 CANプロトコルと状態遷移

実装前に`docs/can_protocol.md`を作成し、各フレームのID、方向、周期、DLC、バイト配置、符号、エンディアン、単位、スケール、予約ビット、CRC、連番、許容範囲、受信失敗時動作を固定する。暫定値はmotion command／heartbeatを10 ms（MCU実行周期と同期）、Pi側command timeoutを50 ms、MCU側heartbeat監視timeoutを50 ms以下とする。50 msの通信断検知時間に加え、MCU処理・駆動系応答・機械制動に要する距離を実機で測定し、安全要件を満たす値へ確定する。破壊的変更はprotocol versionを上げ、Pi/MCUの版不一致ではREADYに遷移させない。IDはMCUとの合意前に実車へ送らない。

| 論理フレーム | 方向 | 内容・防御 |
| --- | --- | --- |
| `motion_command` | Pi → MCU | 線/角速度（または左右速度）、enable、停止要求、連番、有効期限。PiとMCUの双方で範囲、期限、状態を検査する。 |
| `heartbeat` | Pi → MCU | protocol version、連番、Pi通信健全性。MCUは50 ms以下の欠落で停止し、通信復旧だけでは再始動しない。 |
| `mcu_status` | MCU → Pi | MCU状態、安全入力、E-stop、故障、通信監視、最終受理連番。Piの表示・上位停止に使うが安全判定を代替しない。 |
| `wheel_odometry` | MCU → Pi | 左右エンコーダ、実速度、連番または時刻。単位、原点、ロールオーバーを明記する。 |
| `power_thermal` | MCU → Pi | バッテリ、電流、温度。未実装値をゼロで偽装しない。 |

状態は`DOWN → CONFIGURING → WAITING_FOR_MCU → READY → FAULT`とする。READYにはinterface UP、version一致、連続した正常status、MCUの安全許可、上位の明示enableを必要とする。bus-off、送信失敗継続、status timeout、version不一致、MCU fault、E-stopはFAULTとし、Piはゼロ速度・disableの送信を試みた後に通常指令を止める。復帰にはMCUの安全復帰条件と明示enableを改めて必要とする。

### 14.4 ROS 2構成

`mower_can`にC++/`rclcpp` lifecycle node `can_gateway_node`を実装する。CAN RAW socketを非同期受信し、送信周期はROS timer、timeoutは単調クロックで評価する。interface/ソケット障害には指数バックオフで再接続し、`can_interface`は既定`can0`、試験時`vcan0`とする。

| 種別 | インターフェース | 方針 |
| --- | --- | --- |
| Subscribe | `/cmd_vel` (`geometry_msgs/TwistStamped`) | 受信時刻、frame_id、有限値、上限を検査し、期限切れは送信しない。 |
| Subscribe | `/mower/enable` (`std_msgs/Bool`) | 明示許可要求のみ。安全保証・再始動の代替ではない。 |
| Publish | `/wheel/odometry` (`nav_msgs/Odometry`) | MCU情報を正規化し、座標系・共分散・時刻の出所を仕様化する。 |
| Publish | `/battery/state` (`sensor_msgs/BatteryState`) | 取得可能な値だけをSI単位で公開する。 |
| Publish | `/mower/mcu_status`、`/mower/can_link_status` | `mower_can`のrosidl messageとして、安全状態、故障、通信統計を表す。 |
| Publish | `/diagnostics` | bus-off、error frame、timeout、再接続、拒否指令数を出す。 |

### 14.5 実装・検証の段階

実機CANベンチ試験への移行条件は、MCP2515の発振器・INT・ビットタイミング・物理層・終端抵抗の確認、`can0`起動、MCUとのID／フレーム／timeoutの合意、MCUの独立停止実装、および`vcan0`での正常・欠落・timeout・再接続試験の合格とする。この段階ではモータ・ブレードを無効にする。モータを有効にした低速統合は、MCU status decodeとPi側のstatus timeout／FAULT遷移を実装し、MCUの通信断時独立停止試験に合格した後に限る。

1. **仕様・回路レビュー**：12.2の未確定値、停止状態、ID、バイト配置、timeoutを承認し、`docs/can_protocol.md`と配線チェックリストを作る。未承認なら実車指令を送らない。
2. **SocketCAN基盤**：overlayと起動設定を導入する。`ip -details link show can0`、`candump`、`cansend`で認識、ビットレート、送受信、再起動後の復帰を確認する。
3. **codec**：CAN frameのencode/decode、DLC・範囲・連番・version検査を純粋関数として実装し、任意バイト列と未定義IDを安全に拒否する。
4. **gateway**：lifecycle、socket、状態遷移、`cmd_vel` watchdog、ROS変換、diagnostics、再接続を実装する。inactive/error中はmotion commandを送らない。
5. **仮想CAN/HIL**：`vcan0`とMCU simulatorで正常、欠落、期限切れ、状態遷移、再起動を自動試験した後、モータ・ブレード無効の実CANで負荷、bus-off、Pi/MCU再起動、SPI/INT断を試験する。
6. **低速統合**：MCU停止試験後に低速・無負荷で指令、オドメトリ、通信断停止、復帰をログ化し、残存リスクを更新してから範囲を広げる。

単体テストは全frame、境界値、符号/エンディアン、DLC不一致、未知ID、NaN/Inf、連番ロールオーバー、期限、状態遷移を対象とする。結合テストは`vcan0`で順序乱れ、欠落、再接続、topic変換、diagnosticsを確認する。実機では起動100回、連続通信、最大想定バス負荷、error counter、bus-off復旧を測定し、SPI共有時のBMI270とCANの要求レートも同時確認する。CAN_H/L断、終端不良、MCU/Pi停止、MCP2515電源断、SPI/INT断、異常ID/DLC、期限切れを故障注入し、MCUの独立停止とPiのFAULT/診断を確認する。

成果物はprotocol仕様、配線・overlay手順、`mower_can`のcodec/gateway/message/launch/config、MCU simulator、単体・結合・HIL結果、CAN log、残存リスク一覧である。発振器、物理層、CAN ID、MCU timeoutはリポジトリだけでは確定できないため、承認済みの値で埋まるまで実装は`vcan0`とモータ無効ベンチ試験に限定する。
