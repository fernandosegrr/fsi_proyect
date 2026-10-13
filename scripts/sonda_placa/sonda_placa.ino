/*
 * sonda_placa.ino — Programa mínimo para saber si una placa ESP32 tiene PSRAM.
 *
 * GUÍA DE LECTURA
 *  1. Lo usa scripts/cargar.ps1 UNA sola vez por placa (después recuerda la dirección MAC).
 *  2. Se compila para "ESP32 Wrover Module" (con PSRAM activada). Si la placa tiene PSRAM
 *     (la Freenove WROVER con cámara) imprime psram=SI; si no la tiene (ESP32 DevKit) el
 *     arranque falla con un mensaje de PSRAM en el puerto serie, o imprime psram=NO.
 *  3. Imprime una línea por segundo a 115200 baudios. No usa WiFi ni motores ni pines.
 */

void setup() {
  Serial.begin(115200);
  delay(500);
}

void loop() {
  Serial.printf("SONDA psram=%s tam=%u chip=%s rev=%d mac=%012llX\n",
                psramFound() ? "SI" : "NO",
                (unsigned)ESP.getPsramSize(),
                ESP.getChipModel(),
                (int)ESP.getChipRevision(),
                (unsigned long long)ESP.getEfuseMac());
  delay(1000);
}
