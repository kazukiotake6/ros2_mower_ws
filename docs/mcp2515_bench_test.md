# MCP2515実機ベンチ試験手順

この手順はモータ・ブレードを無効化したベンチでのみ実施する。MCP2515の発振器周波数、CANビットレート、CAN ID、MCUの受信フォーマットを承認済みの値へ置換するまで、`/mower/enable`を`true`にしてはならない。Pi→MCUの暫定バイト配置は[can_protocol.md](can_protocol.md)を参照する。

## 1. 配線レビュー

- MCP2515はSPI0 CE1、INTはGPIO24、BMI270はSPI0 CE0のままであることを確認する。
- Pi/MCP2515のロジックは3.3 Vであり、TJA1050の5 V信号を直結しない。
- CAN_H/CAN_L、共通GND、両端の120 Ω終端、電源極性を確認する。

## 2. PiのCAN起動

対象カーネルの`/boot/firmware/overlays/README`でoverlay名とパラメータを確認してから、boot設定へMCP2515 overlayを追加する。例（発振器周波数は実部品の値へ置換）は次のとおり。

```
dtoverlay=mcp2515-can1,oscillator=16000000,interrupt=24
```

再起動後、MCUと一致するビットレートでinterfaceを起動する。

```
sudo ip link set can0 up type can bitrate 500000 restart-ms 100
ip -details -statistics link show can0
```

`can0`が存在しない、またはerror-passive/bus-offとなる場合は、overlay、発振器、INT、終端、配線、MCU側ビットタイミングを確認する。

## 3. can-utilsによる疎通

別端末で`candump -e -t a can0`を実行し、MCUからのフレームとerror frameを記録する。承認済みの無害な診断IDだけを使い、送受信を確認する。モータ指令用IDを`cansend`で試験してはならない。

## 4. ROS 2 gateway試験

```
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 launch mower_can can_gateway.launch.py
ros2 lifecycle set /can_gateway_node configure
ros2 lifecycle set /can_gateway_node activate
ros2 topic echo /diagnostics
```

既定のgatewayはenableがfalseであり、周期的な停止フレームとheartbeatだけを送信する。MCUプロトコルのレビュー後にのみ、YAMLでID・上限値を確定して`/mower/enable`を有効化する。`/cmd_vel`の値域外、NaN、期限切れは送信指令として受理されず、診断カウンタで確認できる。

## 5. 合格基準

- 連続100回の起動で`can0`を認識し、BMI270のSPI通信を妨げない。
- MCUとの送受信中にbus-off、error frameの増加、CAN再起動がない。
- Pi停止、MCU停止、CAN線断、INT断でMCUが独立停止し、gatewayが診断異常を出す。
- 実走前にCAN ID・フレーム形式・timeout・停止状態がMCU仕様書で承認済みである。
