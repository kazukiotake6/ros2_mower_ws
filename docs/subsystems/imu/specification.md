# BMI270入力仕様

## 範囲と安全境界

Pi直結BMI270はVIO・SLAM用の非安全系センサーである。転倒や姿勢異常による最終停止判断には使用せず、外部MCU、安全用センサー、独立E-stop・遮断回路の責務を代替しない。

## 初期化契約

`mower_imu`はBosch BMI270 Sensor APIを使用し、次の順で初期化する。

1. `/dev/spidev0.0`をSPI mode 0、8 bit wordで開く。
2. 公式APIへSPI read/write/delay callbackを設定する。
3. `bmi270_init()`でチップID確認、ソフトリセット、公式設定ファイルのロードを行う。
4. 加速度4 g、ジャイロ2000 deg/s、各200 Hzを公式APIで設定する。
5. 加速度・ジャイロを有効化する。

いずれかが失敗した場合は初期化完了とせず、IMUデータを正常値として公開しない。初期化段階と公式APIの戻り値を`/diagnostics`へERRORとして公開する。成功時も準備完了状態を診断へ公開する。

現在の200 Hz、レンジ、フィルタ設定は初期ベンチ評価用であり、実機評価後に設定プロファイル化する。FIFO、INT1、SENSORTIME、時刻変換、サンプルPublish、再初期化は本段階の範囲外である。

## SPI callback契約

- read callbackはコマンドに続くBMI270 SPIのダミーバイトを含めて公式APIへ返し、ダミーバイトの除去は公式APIへ一元化する。
- write callbackは公式8 KiB設定ファイルを分割転送する連続書込みに対応し、単一レジスタ書込みへ限定しない。
- transportの失敗は成功として扱わず、公式APIへinterface errorを返す。
- delay callbackは指定されたマイクロ秒以上待機する。

## 未検証事項

Pi 5と実BMI270によるチップID、設定ロード、実効ODR、SPI信号品質は未検証である。実機確認まではVIO-G2を合格扱いにしない。
