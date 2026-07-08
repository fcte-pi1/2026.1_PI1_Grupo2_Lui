Board (Boards Manager):

esp32 by Espressif Systems — depois selecione "ESP32 Dev Module"

Libraries (Library Manager):

VL53L0X (Pololu) — sensor de distância
Adafruit MPU6050 — giroscópio/acelerômetro
Adafruit Unified Sensor — dependência da MPU6050 (instala junto se aceitar as dependências)
ArduinoJson (Benoît Blanchon) v7.x — telemetria

Wire, WiFi e BluetoothSerial já vêm com o core do ESP32, não precisa instalar.
Obs: o projeto é PlatformIO — se puder, é mais fácil flashar com pio run -t upload (ou -e telemetria pro ambiente de telemetria), que instala tudo sozinho.o que instalar pra reconher a porta comProcesso de pensamentoProcesso de pensamentoDepende do chip USB-serial da sua placa ESP32 DevKit. Vire a placa e veja o chip perto do conector USB:

CP2102 (mais comum no DevKit v1): driver CP210x da Silicon Labs — https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers
CH340/CH9102 (placas genéricas/clones): driver CH341/CH343 da WCH — http://wch-ic.com/downloads/CH341SER_ZIP.html (CH340) ou CH343SER (CH9102)

Depois de instalar, reconecte o USB e confira no Gerenciador de Dispositivos → Portas (COM e LPT) se aparece algo como "Silicon Labs CP210x (COM3)".
Se não aparecer nada nem como dispositivo desconhecido, teste outro cabo USB — muito cabo é só de carga, sem dados.