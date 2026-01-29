// Minimal test for Waveshare 7B

void setup() {
  // Use RS485 UART which definitely works (GPIO15 TX, GPIO16 RX)
  Serial1.begin(115200, SERIAL_8N1, 16, 15);
  delay(100);
  Serial1.println("\n\n=== MINIMAL TEST BOOT ===");
  
  // Also try default UART
  Serial.begin(115200);
  delay(100);
  Serial.println("=== MINIMAL TEST BOOT (Serial) ===");
}

void loop() {
  Serial1.println("Alive via RS485 UART");
  Serial.println("Alive via Serial");
  delay(1000);
}
