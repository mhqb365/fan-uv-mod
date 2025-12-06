# ESP32 UV Fan Controller

Dự án điều khiển quạt và đèn UV LED sử dụng ESP32-C3 Super Mini.

## Tính năng

- **Điều khiển quạt** với 4 mức tốc độ (35%, 50%, 75%, 100%)
- **Điều khiển đèn UV LED** (bật/tắt)
- **Bảo vệ xung đột**: Quạt và đèn UV không thể hoạt động cùng lúc
- **Chống dội nút bấm** để đảm bảo hoạt động ổn định

## Phần cứng

### Chân kết nối

| Chức năng | GPIO | Loại |
|-----------|------|------|
| Nút điều khiển quạt | GPIO 6 | INPUT_PULLUP |
| Nút điều khiển UV | GPIO 5 | INPUT_PULLUP |
| Nguồn quạt | GPIO 10 | OUTPUT |
| PWM quạt | GPIO 3 | OUTPUT (PWM) |
| Đèn UV | GPIO 2 | OUTPUT |

### Linh kiện cần thiết

- ESP32-C3 Super Mini
- Quạt DC (12V hoặc 5V)
- Đèn UV LED
- 2 nút bấm
- Mạch điều khiển nguồn (relay hoặc MOSFET)

## Cách sử dụng

### Nút điều khiển quạt
- **Single click**: Tăng cấp độ quạt theo chu kỳ
  - OFF → 35% → 50% → 75% → 100% → OFF...
- **Double click**: Tắt quạt ngay lập tức

### Nút điều khiển UV
- **Click**: Bật/tắt đèn UV LED

### Lưu ý
- Khi bật quạt, đèn UV sẽ tự động tắt
- Khi bật đèn UV, quạt sẽ tự động tắt
- Tần số PWM: 25kHz (phù hợp với hầu hết các loại quạt DC)

## Cài đặt

1. Cài đặt [Arduino IDE](https://www.arduino.cc/en/software)
2. Thêm ESP32 board support:
   - Vào **File > Preferences**
   - Thêm URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Cài đặt board **ESP32** từ Board Manager
4. Chọn board: **ESP32C3 Dev Module**
5. Upload code vào ESP32-C3

## Thông số kỹ thuật

- Tần số PWM: 25kHz
- Độ phân giải PWM: 8-bit (0-255)
- Thời gian chống dội: 50ms
- Thời gian double click: 300ms

## License

MIT License
