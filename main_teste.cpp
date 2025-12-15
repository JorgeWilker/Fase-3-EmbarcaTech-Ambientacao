#include <Arduino.h> 
#include "credentials.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <BH1750.h>

// ======================== CONFIGURAÇÃO DE TESTE ========================
#define TEST_MODE true              // Modo de teste ativado
#define TEST_DURATION_MS 300000     // 5 minutos de teste
#define METRIC_INTERVAL_MS 1000     // Coleta de métricas a cada 1 segundo

// Instâncias dos sensores
Adafruit_AHTX0 aht;
BH1750 lightMeter;

// Configuração do Sensor de Umidade do Solo Capacitivo
#define SOIL_MOISTURE_PIN 34
const int SOIL_DRY_VALUE = 2521;
const int SOIL_WET_VALUE = 1200;

// Variáveis para controle
unsigned long lastConnectionCheck = 0;
const unsigned long connectionCheckInterval = 2000; // Mesma frequência que leitura (2 segundos)
unsigned long lastSensorRead = 0;
const unsigned long sensorReadInterval = 2000;

// Variáveis dos sensores
float temperature = 0.0;
float humidity = 0.0;
float lightLevel = 0.0;
int soilMoistureRaw = 0;
float soilMoisturePercent = 0.0;
int wifiRSSI = 0;               // Nível de sinal WiFi (dBm)

// Status dos sensores
bool ahtInitialized = false;
bool bh1750Initialized = false;

// ======================== MÉTRICAS DE TESTE ========================
struct TestMetrics {
  // Contadores
  unsigned long totalReadings = 0;
  unsigned long successfulReadings = 0;
  unsigned long failedReadings = 0;
  
  // Tempo de resposta (em microsegundos)
  unsigned long minReadTime = 999999;
  unsigned long maxReadTime = 0;
  unsigned long totalReadTime = 0;
  
  // Sensor específico
  unsigned long ahtReadCount = 0;
  unsigned long bh1750ReadCount = 0;
  unsigned long soilReadCount = 0;
  unsigned long wifiReadCount = 0;
  
  unsigned long ahtFailCount = 0;
  unsigned long bh1750FailCount = 0;
  
  // Comunicação
  unsigned long blynkSendCount = 0;
  unsigned long blynkFailCount = 0;
  unsigned long wifiDisconnects = 0;
  unsigned long blynkDisconnects = 0;
  
  // Reconexões
  unsigned long wifiReconnects = 0;
  unsigned long blynkReconnects = 0;
  
  // Latência de comunicação
  unsigned long minBlynkLatency = 999999;
  unsigned long maxBlynkLatency = 0;
  unsigned long totalBlynkLatency = 0;
  
  // Memória
  unsigned long minFreeHeap = 999999;
  unsigned long maxFreeHeap = 0;
  
  // Início do teste
  unsigned long testStartTime = 0;
};

TestMetrics metrics;

// Flags de estado anterior
bool wasWifiConnected = false;
bool wasBlynkConnected = false;

// ======================== FUNÇÕES DE TESTE ========================

void printTestHeader() {
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║          SISTEMA DE TESTES - SENSORES ESP32               ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  Serial.println("║  Modo: TESTE SISTEMÁTICO                                  ║");
  Serial.println("║  Duração: 5 minutos                                       ║");
  Serial.println("║  Intervalo de métricas: 1 segundo                         ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println();
}

void printMetrics() {
  unsigned long elapsedTime = millis() - metrics.testStartTime;
  unsigned long elapsedSeconds = elapsedTime / 1000;
  
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║                    MÉTRICAS DE TESTE                       ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  
  Serial.print("║ Tempo decorrido: ");
  Serial.print(elapsedSeconds);
  Serial.println(" s");
  
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  Serial.println("║                 LEITURAS DE SENSORES                       ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  
  Serial.print("║ Total de leituras: ");
  Serial.println(metrics.totalReadings);
  Serial.print("║ Leituras bem-sucedidas: ");
  Serial.println(metrics.successfulReadings);
  Serial.print("║ Leituras falhadas: ");
  Serial.println(metrics.failedReadings);
  
  if (metrics.totalReadings > 0) {
    float successRate = (metrics.successfulReadings * 100.0) / metrics.totalReadings;
    Serial.print("║ Taxa de sucesso: ");
    Serial.print(successRate, 2);
    Serial.println(" %");
  }
  
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  Serial.println("║              DESEMPENHO POR SENSOR                         ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  
  Serial.print("║ AHT20/21: ");
  Serial.print(metrics.ahtReadCount);
  Serial.print(" leituras, ");
  Serial.print(metrics.ahtFailCount);
  Serial.println(" falhas");
  
  Serial.print("║ BH1750: ");
  Serial.print(metrics.bh1750ReadCount);
  Serial.print(" leituras, ");
  Serial.print(metrics.bh1750FailCount);
  Serial.println(" falhas");
  
  Serial.print("║ Solo: ");
  Serial.print(metrics.soilReadCount);
  Serial.println(" leituras");
  
  Serial.print("║ WiFi RSSI: ");
  Serial.print(metrics.wifiReadCount);
  Serial.println(" leituras");
  
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  Serial.println("║               TEMPO DE RESPOSTA (μs)                       ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  
  Serial.print("║ Tempo mínimo: ");
  Serial.print(metrics.minReadTime);
  Serial.println(" μs");
  
  Serial.print("║ Tempo máximo: ");
  Serial.print(metrics.maxReadTime);
  Serial.println(" μs");
  
  if (metrics.successfulReadings > 0) {
    unsigned long avgReadTime = metrics.totalReadTime / metrics.successfulReadings;
    Serial.print("║ Tempo médio: ");
    Serial.print(avgReadTime);
    Serial.println(" μs");
  }
  
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  Serial.println("║                  COMUNICAÇÃO BLYNK                         ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  
  Serial.print("║ Envios bem-sucedidos: ");
  Serial.println(metrics.blynkSendCount);
  Serial.print("║ Envios falhados: ");
  Serial.println(metrics.blynkFailCount);
  
  if (metrics.blynkSendCount > 0) {
    float blynkSuccessRate = (metrics.blynkSendCount * 100.0) / (metrics.blynkSendCount + metrics.blynkFailCount);
    Serial.print("║ Taxa de sucesso Blynk: ");
    Serial.print(blynkSuccessRate, 2);
    Serial.println(" %");
    
    unsigned long avgBlynkLatency = metrics.totalBlynkLatency / metrics.blynkSendCount;
    Serial.print("║ Latência média Blynk: ");
    Serial.print(avgBlynkLatency);
    Serial.println(" μs");
  }
  
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  Serial.println("║              ESTABILIDADE DE CONEXÃO                       ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  
  Serial.print("║ WiFi desconexões: ");
  Serial.println(metrics.wifiDisconnects);
  Serial.print("║ WiFi reconexões: ");
  Serial.println(metrics.wifiReconnects);
  Serial.print("║ Blynk desconexões: ");
  Serial.println(metrics.blynkDisconnects);
  Serial.print("║ Blynk reconexões: ");
  Serial.println(metrics.blynkReconnects);
  
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  Serial.println("║                 CONSUMO DE MEMÓRIA                         ║");
  Serial.println("╠════════════════════════════════════════════════════════════╣");
  
  uint32_t currentHeap = ESP.getFreeHeap();
  Serial.print("║ Heap livre atual: ");
  Serial.print(currentHeap);
  Serial.println(" bytes");
  
  Serial.print("║ Heap livre mínimo: ");
  Serial.print(metrics.minFreeHeap);
  Serial.println(" bytes");
  
  Serial.print("║ Heap livre máximo: ");
  Serial.print(metrics.maxFreeHeap);
  Serial.println(" bytes");
  
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println();
}

void printFinalReport() {
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════════════════════╗");
  Serial.println("║              RELATÓRIO FINAL DE TESTE                      ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println();
  
  printMetrics();
  
  Serial.println("╔════════════════════════════════════════════════════════════╗");
  Serial.println("║                  DADOS PARA CSV                            ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println();
  Serial.println("Métrica,Valor");
  Serial.print("Total Leituras,"); Serial.println(metrics.totalReadings);
  Serial.print("Leituras Sucesso,"); Serial.println(metrics.successfulReadings);
  Serial.print("Leituras Falha,"); Serial.println(metrics.failedReadings);
  Serial.print("Tempo Min (μs),"); Serial.println(metrics.minReadTime);
  Serial.print("Tempo Max (μs),"); Serial.println(metrics.maxReadTime);
  
  if (metrics.successfulReadings > 0) {
    Serial.print("Tempo Médio (μs),"); 
    Serial.println(metrics.totalReadTime / metrics.successfulReadings);
  }
  
  Serial.print("AHT Leituras,"); Serial.println(metrics.ahtReadCount);
  Serial.print("AHT Falhas,"); Serial.println(metrics.ahtFailCount);
  Serial.print("BH1750 Leituras,"); Serial.println(metrics.bh1750ReadCount);
  Serial.print("BH1750 Falhas,"); Serial.println(metrics.bh1750FailCount);
  Serial.print("Solo Leituras,"); Serial.println(metrics.soilReadCount);
  Serial.print("WiFi RSSI Leituras,"); Serial.println(metrics.wifiReadCount);
  Serial.print("Blynk Envios,"); Serial.println(metrics.blynkSendCount);
  Serial.print("Blynk Falhas,"); Serial.println(metrics.blynkFailCount);
  Serial.print("WiFi Desconexões,"); Serial.println(metrics.wifiDisconnects);
  Serial.print("WiFi Reconexões,"); Serial.println(metrics.wifiReconnects);
  Serial.print("Blynk Desconexões,"); Serial.println(metrics.blynkDisconnects);
  Serial.print("Blynk Reconexões,"); Serial.println(metrics.blynkReconnects);
  Serial.print("Heap Min (bytes),"); Serial.println(metrics.minFreeHeap);
  Serial.print("Heap Max (bytes),"); Serial.println(metrics.maxFreeHeap);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  printTestHeader();
  
  metrics.testStartTime = millis();
  
  // Inicializa I2C
  Wire.begin();
  
  // Configura pino ADC do sensor de umidade do solo
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  Serial.println("✓ Sensor de umidade do solo configurado");
  
  // Inicializa sensor AHT20/AHT21
  Serial.print("Inicializando AHT20/AHT21... ");
  if (aht.begin()) {
    Serial.println("✓ OK");
    ahtInitialized = true;
  } else {
    Serial.println("✗ FALHA");
  }
  
  // Inicializa sensor BH1750
  Serial.print("Inicializando BH1750... ");
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("✓ OK");
    bh1750Initialized = true;
  } else {
    Serial.println("✗ FALHA");
  }
  
  // Conecta ao WiFi
  Serial.print("Conectando ao WiFi... ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" ✓ OK");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI inicial: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    wasWifiConnected = true;
  } else {
    Serial.println(" ✗ FALHA");
  }
  
  // Conecta ao Blynk
  Serial.print("Conectando ao Blynk (max 30s)... ");
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  
  int blynkAttempts = 0;
  unsigned long blynkStartTime = millis();
  const unsigned long blynkTimeout = 30000; // 30 segundos
  
  while (!Blynk.connected() && blynkAttempts < 60) {
    Blynk.run();
    
    // Verifica timeout
    if (millis() - blynkStartTime > blynkTimeout) {
      Serial.println("\n⚠ Timeout!");
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
  
  if (Blynk.connected()) {
    Serial.println(" ✓ OK");
    Serial.print("Tempo de conexão: ");
    Serial.print((millis() - blynkStartTime) / 1000.0);
    Serial.println("s");
    Serial.println("Virtual Pins: V0-V4 (Temp, Umid, Luz, Solo, WiFi)");
    wasBlynkConnected = true;
  } else {
    Serial.println(" ✗ FALHA - Continuando sem Blynk");
  }
  
  Serial.println("\n✓ Teste iniciado!");
  Serial.println("Coletando métricas...\n");
}

void loop() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - metrics.testStartTime;
  
  // Verifica se o teste terminou
  if (elapsedTime >= TEST_DURATION_MS) {
    printFinalReport();
    Serial.println("✓ Teste finalizado! O sistema será pausado.");
    while(1) { delay(1000); } // Para o sistema
  }
  
  // Monitora conexões
  bool currentWifiStatus = (WiFi.status() == WL_CONNECTED);
  bool currentBlynkStatus = Blynk.connected();
  
  if (wasWifiConnected && !currentWifiStatus) {
    metrics.wifiDisconnects++;
    Serial.println("⚠ WiFi desconectado!");
  }
  if (!wasWifiConnected && currentWifiStatus) {
    metrics.wifiReconnects++;
    Serial.println("✓ WiFi reconectado!");
  }
  
  if (wasBlynkConnected && !currentBlynkStatus) {
    metrics.blynkDisconnects++;
    Serial.println("⚠ Blynk desconectado!");
  }
  if (!wasBlynkConnected && currentBlynkStatus) {
    metrics.blynkReconnects++;
    Serial.println("✓ Blynk reconectado!");
  }
  
  wasWifiConnected = currentWifiStatus;
  wasBlynkConnected = currentBlynkStatus;
  
  // Tenta reconectar WiFi se necessário
  if (!currentWifiStatus) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(100);
  }
  
  Blynk.run();
  
  // Lê sensores a cada 2 segundos
  if (currentTime - lastSensorRead >= sensorReadInterval) {
    lastSensorRead = currentTime;
    
    unsigned long readStartTime = micros();
    bool readSuccess = true;
    
    metrics.totalReadings++;
    
    // Lê sensor AHT20/AHT21
    if (ahtInitialized) {
      sensors_event_t humid, temp;
      if (aht.getEvent(&humid, &temp)) {
        temperature = temp.temperature;
        humidity = humid.relative_humidity;
        metrics.ahtReadCount++;
      } else {
        metrics.ahtFailCount++;
        readSuccess = false;
      }
    }
    
    // Lê sensor BH1750
    if (bh1750Initialized) {
      float reading = lightMeter.readLightLevel();
      if (reading >= 0) {
        lightLevel = reading;
        metrics.bh1750ReadCount++;
      } else {
        metrics.bh1750FailCount++;
        readSuccess = false;
      }
    }
    
    // Lê Sensor de Umidade do Solo
    soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN);
    soilMoisturePercent = map(soilMoistureRaw, SOIL_DRY_VALUE, SOIL_WET_VALUE, 0, 100);
    if (soilMoisturePercent < 0) soilMoisturePercent = 0;
    if (soilMoisturePercent > 100) soilMoisturePercent = 100;
    metrics.soilReadCount++;
    
    // Lê Nível de Sinal WiFi (RSSI)
    if (WiFi.status() == WL_CONNECTED) {
      wifiRSSI = WiFi.RSSI();
      metrics.wifiReadCount++;
    }
    
    unsigned long readTime = micros() - readStartTime;
    
    if (readSuccess) {
      metrics.successfulReadings++;
      metrics.totalReadTime += readTime;
      
      if (readTime < metrics.minReadTime) metrics.minReadTime = readTime;
      if (readTime > metrics.maxReadTime) metrics.maxReadTime = readTime;
    } else {
      metrics.failedReadings++;
    }
    
    // Envia para Blynk e mede latência
    if (Blynk.connected()) {
      unsigned long blynkStartTime = micros();
      
      Blynk.virtualWrite(V0, temperature);
      Blynk.virtualWrite(V1, humidity);
      Blynk.virtualWrite(V2, lightLevel);
      Blynk.virtualWrite(V3, soilMoisturePercent);
      Blynk.virtualWrite(V4, wifiRSSI);
      
      unsigned long blynkLatency = micros() - blynkStartTime;
      
      metrics.blynkSendCount++;
      metrics.totalBlynkLatency += blynkLatency;
      
      if (blynkLatency < metrics.minBlynkLatency) metrics.minBlynkLatency = blynkLatency;
      if (blynkLatency > metrics.maxBlynkLatency) metrics.maxBlynkLatency = blynkLatency;
    } else {
      metrics.blynkFailCount++;
    }
  }
  
  // Atualiza métricas de memória
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < metrics.minFreeHeap) metrics.minFreeHeap = freeHeap;
  if (freeHeap > metrics.maxFreeHeap) metrics.maxFreeHeap = freeHeap;
  
  // Imprime métricas a cada 1 segundo
  if (currentTime - lastConnectionCheck >= METRIC_INTERVAL_MS) {
    lastConnectionCheck = currentTime;
    printMetrics();
  }
  
  delay(10);
}

