#include <Arduino.h>
#include "credentials.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <BH1750.h>

// Instâncias dos sensores
Adafruit_AHTX0 aht;
BH1750 lightMeter;

// Configuração do Sensor de Umidade do Solo Capacitivo
#define SOIL_MOISTURE_PIN 34  // Pino ADC do ESP32 (GPIO34)

// Calibração do sensor de umidade do solo
// Valores calibrados para este sensor específico
const int SOIL_DRY_VALUE = 2521;    // Valor ADC quando o solo está seco (no ar) - CALIBRADO
const int SOIL_WET_VALUE = 1200;   // Valor ADC quando o solo está molhado (na água) - CALIBRADO

// Variáveis para controle
unsigned long lastConnectionCheck = 0;
const unsigned long connectionCheckInterval = 2000; // Verifica a cada 2 segundos (mesmo que leitura)
unsigned long lastSensorRead = 0;
const unsigned long sensorReadInterval = 2000; // Lê sensores a cada 2 segundos

// Variáveis dos sensores
float temperature = 0.0;
float humidity = 0.0;
float lightLevel = 0.0;
int soilMoistureRaw = 0;        // Valor bruto do ADC
float soilMoisturePercent = 0.0; // Umidade do solo em porcentagem
int wifiRSSI = 0;               // Nível de sinal WiFi (dBm)

// Status dos sensores
bool ahtInitialized = false;
bool bh1750Initialized = false;

void setup() {
  // Inicializa Serial Monitor
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Sistema de Monitoramento ESP32 ===");
  
  // Inicializa I2C (pinos padrão ESP32: SDA=21, SCL=22)
  Wire.begin();
  
  // Configura pino ADC do sensor de umidade do solo
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  Serial.println("\nSensor de Umidade do Solo Capacitivo:");
  Serial.print("  Configurado no pino GPIO");
  Serial.println(SOIL_MOISTURE_PIN);
  Serial.println("  ✓ Pronto para leitura analógica");
  
  // Inicializa sensor AHT20/AHT21 (Temperatura e Umidade)
  Serial.println("\nInicializando sensor AHT20/AHT21...");
  if (aht.begin()) {
    Serial.println("✓ Sensor AHT20/AHT21 inicializado com sucesso!");
    ahtInitialized = true;
  } else {
    Serial.println("✗ Falha ao inicializar sensor AHT20/AHT21!");
    Serial.println("  Verifique a conexão I2C (SDA=GPIO21, SCL=GPIO22)");
  }
  
  // Inicializa sensor BH1750 (Luminosidade)
  Serial.println("\nInicializando sensor BH1750...");
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("✓ Sensor BH1750 inicializado com sucesso!");
    bh1750Initialized = true;
  } else {
    Serial.println("✗ Falha ao inicializar sensor BH1750!");
    Serial.println("  Verifique a conexão I2C (SDA=GPIO21, SCL=GPIO22)");
  }
  
  // Conecta ao WiFi
  Serial.println("\nConectando ao WiFi...");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  Serial.println();
  
  // Verifica conexão WiFi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi conectado com sucesso!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("✗ Falha na conexão WiFi!");
    Serial.println("Verifique suas credenciais e tente novamente.");
    return;
  }
  
  // Conecta ao Blynk
  Serial.println("\nConectando ao Blynk...");
  Serial.print("Template ID: ");
  Serial.println(BLYNK_TEMPLATE_ID);
  Serial.print("Auth Token: ");
  Serial.println(BLYNK_AUTH_TOKEN);
  Serial.println("Aguardando conexão (máximo 30 segundos)...");
  
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  
  // Aguarda conexão Blynk (aumentado para 60 tentativas = 30 segundos)
  int blynkAttempts = 0;
  unsigned long blynkStartTime = millis();
  const unsigned long blynkTimeout = 30000; // 30 segundos
  
  while (!Blynk.connected() && blynkAttempts < 60) {
    Blynk.run();
    
    // Verifica timeout
    if (millis() - blynkStartTime > blynkTimeout) {
      Serial.println("\n⚠ Timeout na conexão Blynk!");
      break;
    }
    
    delay(500);
    Serial.print(".");
    blynkAttempts++;
    
    // Mostra progresso a cada 5 segundos
    if (blynkAttempts % 10 == 0) {
      Serial.print(" (");
      Serial.print(blynkAttempts * 0.5);
      Serial.print("s)");
    }
  }
  
  Serial.println();
  
  if (Blynk.connected()) {
    Serial.println("✓ Blynk conectado com sucesso!");
    Serial.print("Tempo de conexão: ");
    Serial.print((millis() - blynkStartTime) / 1000.0);
    Serial.println(" segundos");
    Serial.println("\n=== Sistema Pronto ===");
    Serial.println("Virtual Pins configurados:");
    Serial.println("  V0 - Temperatura (°C)");
    Serial.println("  V1 - Umidade do Ar (%)");
    Serial.println("  V2 - Luminosidade (lux)");
    Serial.println("  V3 - Umidade do Solo (%)");
    Serial.println("  V4 - Sinal WiFi (dBm)");
  } else {
    Serial.println("✗ Falha na conexão Blynk!");
    Serial.println("O sistema continuará funcionando, mas sem envio para Blynk.");
    Serial.println("Verifique:");
    Serial.println("  - Auth Token correto");
    Serial.println("  - Template ID correto");
    Serial.println("  - Conexão com internet");
    Serial.println("  - Servidor Blynk acessível");
    Serial.println("\n⚠ Continuando sem Blynk...");
  }
}

void loop() {
  // Mantém conexões ativas
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado! Tentando reconectar...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(2000);
  }
  
  Blynk.run();
  
  // Lê sensores a cada 2 segundos
  if (millis() - lastSensorRead >= sensorReadInterval) {
    lastSensorRead = millis();
    
    // Lê sensor AHT20/AHT21 (Temperatura e Umidade)
    if (ahtInitialized) {
      sensors_event_t humid, temp;
      aht.getEvent(&humid, &temp);
      
      temperature = temp.temperature;
      humidity = humid.relative_humidity;
      
      // Envia para Blynk
      if (Blynk.connected()) {
        Blynk.virtualWrite(V0, temperature);
        Blynk.virtualWrite(V1, humidity);
      }
    }
    
    // Lê sensor BH1750 (Luminosidade)
    if (bh1750Initialized) {
      lightLevel = lightMeter.readLightLevel();
      
      // Envia para Blynk
      if (Blynk.connected()) {
        Blynk.virtualWrite(V2, lightLevel);
      }
    }
    
    // Lê Sensor de Umidade do Solo Capacitivo (Analógico)
    soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN);
    
    // Converte para porcentagem (0% = seco, 100% = molhado)
    // Inverte a leitura porque valores maiores = mais seco
    soilMoisturePercent = map(soilMoistureRaw, SOIL_DRY_VALUE, SOIL_WET_VALUE, 0, 100);
    
    // Limita entre 0 e 100%
    if (soilMoisturePercent < 0) soilMoisturePercent = 0;
    if (soilMoisturePercent > 100) soilMoisturePercent = 100;
    
    // Envia para Blynk
    if (Blynk.connected()) {
      Blynk.virtualWrite(V3, soilMoisturePercent);
    }
    
    // Lê Nível de Sinal WiFi (RSSI)
    if (WiFi.status() == WL_CONNECTED) {
      wifiRSSI = WiFi.RSSI();
      
      // Envia para Blynk
      if (Blynk.connected()) {
        Blynk.virtualWrite(V4, wifiRSSI);
      }
    }
    
    // Exibe no Serial Monitor (mesma frequência do Blynk - 2 segundos)
    Serial.println("\n--- Leituras dos Sensores ---");
    
    if (ahtInitialized) {
      Serial.print("🌡️  Temperatura: ");
      Serial.print(temperature, 1);
      Serial.println(" °C");
      
      Serial.print("💧 Umidade Ar: ");
      Serial.print(humidity, 1);
      Serial.println(" %");
    }
    
    if (bh1750Initialized) {
      Serial.print("☀️  Luminosidade: ");
      Serial.print(lightLevel, 0);
      Serial.println(" lux");
    }
    
    Serial.print("🌱 Umidade Solo: ");
    Serial.print(soilMoisturePercent, 0);
    Serial.print(" % (ADC: ");
    Serial.print(soilMoistureRaw);
    Serial.println(")");
      
    // Exibe nível de sinal WiFi
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("📶 Sinal WiFi: ");
      Serial.print(wifiRSSI);
      Serial.print(" dBm");
      
      // Indicador de qualidade do sinal
      if (wifiRSSI > -50) {
        Serial.println(" (Excelente)");
      } else if (wifiRSSI > -60) {
        Serial.println(" (Muito Bom)");
      } else if (wifiRSSI > -70) {
        Serial.println(" (Bom)");
      } else if (wifiRSSI > -80) {
        Serial.println(" (Fraco)");
      } else {
        Serial.println(" (Muito Fraco)");
      }
    } else {
      Serial.println("📶 Sinal WiFi: Desconectado");
    }
  }
  
  // Verifica conexões a cada 2 segundos
  if (millis() - lastConnectionCheck >= connectionCheckInterval) {
    lastConnectionCheck = millis();
    // Verificações de conexão podem ser adicionadas aqui se necessário
  }
  
  delay(100);
}