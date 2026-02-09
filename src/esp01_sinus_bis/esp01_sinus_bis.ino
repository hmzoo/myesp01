/*
 * ESP-01 - Oscillation Sinusoïdale 2 Moteurs (AMPLITUDE VARIABLE)
 * 
 * Fonctionnalités:
 * - Point d'accès WiFi avec interface web
 * - 2 moteurs en opposition sinusoïdale
 * - Amplitude diminue avec la fréquence qui augmente
 * - f=0.5Hz: oscillation 10%-100%, f=10Hz: fixe 100%
 * - Graphique temps réel Canvas
 * 
 * FORMULES:
 * min(f) = 10% + 90% * (f - 0.5) / 9.5
 * max = 100% (constant)
 * centre = (max + min) / 2
 * amplitude = (max - min) / 2
 * M1 = centre + amplitude * sin(2πft)
 * M2 = centre - amplitude * sin(2πft)
 * 
 * CÂBLAGE IDENTIQUE À esp01_2moteurs (DRV8833 VCC unique):
 * GPIO0 → IN2 (AIN2, PWM M1), GPIO2 → IN4 (BIN2, PWM M2)
 * ESP VCC → IN1+IN3 (directions fixes), Condensateur 100-470µF sur VCC-GND
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Configuration WiFi
const char* ssid = "ESP01-Sinus-Bis";
const char* password = "12345678";

// Serveur web
ESP8266WebServer server(80);

// GPIO PWM (PWM inversé pour DRV8833)
const int MOTOR1_PWM_PIN = 0;  // GPIO0 → IN2
const int MOTOR2_PWM_PIN = 2;  // GPIO2 → IN4

// Paramètres oscillation
volatile bool isRunning = false;
volatile float frequency = 0.5;  // Hz (0.5 à 10 Hz)
const float FREQ_MIN = 0.5;
const float FREQ_MAX = 10.0;
const float PWM_MIN_AT_MIN_FREQ = 10.0;   // 10% à 0.5Hz
const float PWM_MAX_CONSTANT = 100.0;      // 100% constant

// Variables temps
unsigned long lastUpdateTime = 0;
const unsigned long UPDATE_INTERVAL = 20;  // 20ms = 50 FPS

// Position actuelle dans le cycle (0 à 2π)
float phase = 0.0;

// PWM actuel des moteurs (0-100%)
float motor1Percent = 100.0;
float motor2Percent = 0.0;

// Fréquence PWM
const int PWM_FREQ = 1000;

// Page HTML
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP-01 Sinus Bis</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 900px;
            width: 100%;
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 10px;
            font-size: 28px;
        }
        .subtitle {
            text-align: center;
            color: #666;
            margin-bottom: 25px;
            font-size: 14px;
        }
        .status {
            text-align: center;
            padding: 12px;
            border-radius: 10px;
            margin-bottom: 20px;
            font-weight: bold;
            font-size: 18px;
        }
        .status.running {
            background: #d4edda;
            color: #155724;
        }
        .status.stopped {
            background: #f8d7da;
            color: #721c24;
        }
        .controls {
            display: flex;
            gap: 15px;
            margin-bottom: 25px;
            justify-content: center;
        }
        button {
            padding: 15px 40px;
            border: none;
            border-radius: 10px;
            font-size: 18px;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.3s;
            text-transform: uppercase;
        }
        .start-btn {
            background: linear-gradient(135deg, #667eea, #764ba2);
            color: white;
        }
        .start-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(102, 126, 234, 0.4);
        }
        .stop-btn {
            background: linear-gradient(135deg, #f093fb, #f5576c);
            color: white;
        }
        .stop-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(245, 87, 108, 0.4);
        }
        .freq-control {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 25px;
        }
        .freq-label {
            display: flex;
            justify-content: space-between;
            margin-bottom: 10px;
            font-weight: bold;
            color: #333;
        }
        .freq-value {
            color: #667eea;
            font-size: 20px;
        }
        input[type="range"] {
            width: 100%;
            height: 8px;
            border-radius: 5px;
            background: #ddd;
            outline: none;
            -webkit-appearance: none;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 24px;
            height: 24px;
            border-radius: 50%;
            background: linear-gradient(135deg, #667eea, #764ba2);
            cursor: pointer;
            box-shadow: 0 2px 10px rgba(102, 126, 234, 0.5);
        }
        .amplitude-info {
            background: #fff3cd;
            padding: 12px;
            border-radius: 8px;
            margin-bottom: 20px;
            text-align: center;
            font-size: 14px;
            color: #856404;
        }
        .amplitude-info strong {
            color: #664d03;
        }
        .graph-container {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 10px;
            margin-bottom: 20px;
        }
        canvas {
            width: 100%;
            height: 250px;
            border-radius: 8px;
            background: white;
        }
        .motor-values {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        .motor-card {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 10px;
            text-align: center;
        }
        .motor-card h3 {
            color: #333;
            margin-bottom: 8px;
            font-size: 16px;
        }
        .motor-value {
            font-size: 32px;
            font-weight: bold;
            background: linear-gradient(135deg, #667eea, #764ba2);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .info-box {
            background: #e7f3ff;
            padding: 15px;
            border-radius: 10px;
            font-size: 13px;
            color: #004085;
            line-height: 1.6;
        }
        .info-box strong {
            color: #002752;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌊 Oscillation Sinusoïdale Bis</h1>
        <div class="subtitle">2 Moteurs • Amplitude Variable</div>
        
        <div id="status" class="status stopped">⏹ ARRÊTÉ</div>
        
        <div class="controls">
            <button class="start-btn" onclick="startOscillation()">▶ START</button>
            <button class="stop-btn" onclick="stopOscillation()">⏹ STOP</button>
        </div>
        
        <div class="freq-control">
            <div class="freq-label">
                <span>Fréquence</span>
                <span class="freq-value" id="freqValue">0.50 Hz</span>
            </div>
            <input type="range" id="freqSlider" min="50" max="1000" value="50" oninput="updateFrequency()">
            <div style="display: flex; justify-content: space-between; font-size: 12px; color: #666; margin-top: 5px;">
                <span>0.5 Hz (2s/cycle)</span>
                <span>10 Hz (0.1s/cycle)</span>
            </div>
        </div>
        
        <div class="amplitude-info" id="amplitudeInfo">
            📊 Amplitude: <strong id="ampValue">10% - 100%</strong> • 
            Centre: <strong id="centerValue">55%</strong>
        </div>
        
        <div class="graph-container">
            <canvas id="graph"></canvas>
        </div>
        
        <div class="motor-values">
            <div class="motor-card">
                <h3>🔵 Moteur 1</h3>
                <div class="motor-value" id="motor1Value">100%</div>
            </div>
            <div class="motor-card">
                <h3>🟣 Moteur 2</h3>
                <div class="motor-value" id="motor2Value">0%</div>
            </div>
        </div>
        
        <div class="info-box">
            <strong>💡 Principe:</strong> L'amplitude de l'oscillation diminue avec la fréquence.<br>
            <strong>Basse fréquence (0.5Hz):</strong> Oscillation large 10%-100%<br>
            <strong>Haute fréquence (10Hz):</strong> Les deux moteurs à 100% constant
        </div>
    </div>

    <script>
        const canvas = document.getElementById('graph');
        const ctx = canvas.getContext('2d');
        canvas.width = canvas.offsetWidth * 2;
        canvas.height = 500;
        
        let dataPoints1 = [];
        let dataPoints2 = [];
        const maxDataPoints = 200;
        
        function updateFrequency() {
            const slider = document.getElementById('freqSlider');
            const freq = slider.value / 100;
            document.getElementById('freqValue').textContent = freq.toFixed(2) + ' Hz';
            
            // Calcul amplitude variable
            const minPWM = 10 + 90 * (freq - 0.5) / 9.5;
            const maxPWM = 100;
            const center = (maxPWM + minPWM) / 2;
            const amplitude = (maxPWM - minPWM) / 2;
            
            document.getElementById('ampValue').textContent = 
                minPWM.toFixed(1) + '% - ' + maxPWM.toFixed(1) + '%';
            document.getElementById('centerValue').textContent = center.toFixed(1) + '%';
            
            fetch('/setFrequency?value=' + freq);
        }
        
        function startOscillation() {
            fetch('/start').then(() => {
                document.getElementById('status').className = 'status running';
                document.getElementById('status').textContent = '▶ EN COURS';
            });
        }
        
        function stopOscillation() {
            fetch('/stop').then(() => {
                document.getElementById('status').className = 'status stopped';
                document.getElementById('status').textContent = '⏹ ARRÊTÉ';
            });
        }
        
        function updateMotorValues() {
            fetch('/getMotors')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('motor1Value').textContent = data.motor1.toFixed(1) + '%';
                    document.getElementById('motor2Value').textContent = data.motor2.toFixed(1) + '%';
                    
                    // Mise à jour graphique
                    dataPoints1.push(data.motor1);
                    dataPoints2.push(data.motor2);
                    
                    if (dataPoints1.length > maxDataPoints) {
                        dataPoints1.shift();
                        dataPoints2.shift();
                    }
                    
                    drawGraph();
                });
        }
        
        function drawGraph() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            // Grille
            ctx.strokeStyle = '#e0e0e0';
            ctx.lineWidth = 1;
            for (let i = 0; i <= 10; i++) {
                const y = (canvas.height / 10) * i;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(canvas.width, y);
                ctx.stroke();
            }
            
            // Ligne 0% et 100%
            ctx.strokeStyle = '#ccc';
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(0, canvas.height);
            ctx.lineTo(canvas.width, canvas.height);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(0, 0);
            ctx.lineTo(canvas.width, 0);
            ctx.stroke();
            
            const pointWidth = canvas.width / maxDataPoints;
            
            // Moteur 1 (bleu)
            if (dataPoints1.length > 1) {
                ctx.strokeStyle = '#667eea';
                ctx.lineWidth = 3;
                ctx.beginPath();
                for (let i = 0; i < dataPoints1.length; i++) {
                    const x = i * pointWidth;
                    const y = canvas.height - (dataPoints1[i] / 100) * canvas.height;
                    if (i === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                }
                ctx.stroke();
            }
            
            // Moteur 2 (violet)
            if (dataPoints2.length > 1) {
                ctx.strokeStyle = '#764ba2';
                ctx.lineWidth = 3;
                ctx.beginPath();
                for (let i = 0; i < dataPoints2.length; i++) {
                    const x = i * pointWidth;
                    const y = canvas.height - (dataPoints2[i] / 100) * canvas.height;
                    if (i === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                }
                ctx.stroke();
            }
        }
        
        // Mise à jour toutes les 100ms
        setInterval(updateMotorValues, 100);
        updateFrequency();
    </script>
</body>
</html>
)rawliteral";

// Gestionnaires HTTP
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleStart() {
  isRunning = true;
  phase = 0.0;  // Redémarre depuis M1=100%, M2=0%
  motor1Percent = 100.0;
  motor2Percent = 0.0;
  Serial.println("▶ START - Oscillation démarrée");
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  isRunning = false;
  // Arrêt direct à 0%
  motor1Percent = 0.0;
  motor2Percent = 0.0;
  analogWrite(MOTOR1_PWM_PIN, 255);  // PWM inversé: 255 = 0%
  analogWrite(MOTOR2_PWM_PIN, 255);
  Serial.println("⏹ STOP - Oscillation arrêtée");
  server.send(200, "text/plain", "OK");
}

void handleSetFrequency() {
  if (server.hasArg("value")) {
    float newFreq = server.arg("value").toFloat();
    if (newFreq >= FREQ_MIN && newFreq <= FREQ_MAX) {
      frequency = newFreq;
      Serial.print("📊 Fréquence: ");
      Serial.print(frequency, 2);
      Serial.println(" Hz");
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleGetMotors() {
  String json = "{\"motor1\":";
  json += String(motor1Percent, 1);
  json += ",\"motor2\":";
  json += String(motor2Percent, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n=================================");
  Serial.println("🌊 ESP-01 Oscillation Sinus Bis");
  Serial.println("=================================\n");
  
  // Configuration PWM
  pinMode(MOTOR1_PWM_PIN, OUTPUT);
  pinMode(MOTOR2_PWM_PIN, OUTPUT);
  analogWriteFreq(PWM_FREQ);
  analogWrite(MOTOR1_PWM_PIN, 255);  // PWM inversé: 255 = 0%
  analogWrite(MOTOR2_PWM_PIN, 255);
  
  Serial.println("✓ GPIO0/GPIO2 configurés en PWM (1kHz)");
  Serial.println("✓ Moteurs initialisés à 0% (PWM inversé)");
  Serial.println("✓ Amplitude variable: 10%-100% @ 0.5Hz → 100%-100% @ 10Hz\n");
  
  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  Serial.println("✓ Point d'accès WiFi démarré");
  Serial.print("   SSID: ");
  Serial.println(ssid);
  Serial.print("   IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("\n=================================\n");
  
  // Routes
  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/setFrequency", handleSetFrequency);
  server.on("/getMotors", handleGetMotors);
  
  server.begin();
  Serial.println("✓ Serveur web démarré\n");
  
  lastUpdateTime = millis();
}

void loop() {
  server.handleClient();
  
  if (isRunning) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
      lastUpdateTime = currentTime;
      
      // Calcul amplitude variable en fonction de la fréquence
      // min(f) = 10% + 90% * (f - 0.5) / 9.5
      float minPWM = PWM_MIN_AT_MIN_FREQ + (PWM_MAX_CONSTANT - PWM_MIN_AT_MIN_FREQ) * (frequency - FREQ_MIN) / (FREQ_MAX - FREQ_MIN);
      float maxPWM = PWM_MAX_CONSTANT;
      float center = (maxPWM + minPWM) / 2.0;
      float amplitude = (maxPWM - minPWM) / 2.0;
      
      // Calcul sinusoïde
      float sinValue = sin(phase);
      motor1Percent = center + amplitude * sinValue;
      motor2Percent = center - amplitude * sinValue;
      
      // Clamp 0-100%
      motor1Percent = constrain(motor1Percent, 0.0, 100.0);
      motor2Percent = constrain(motor2Percent, 0.0, 100.0);
      
      // Conversion % → PWM (0-255)
      uint8_t pwm1 = (uint8_t)(motor1Percent * 2.55);
      uint8_t pwm2 = (uint8_t)(motor2Percent * 2.55);
      
      // PWM inversé (IN1/IN3=HIGH donc LOW=pleine vitesse)
      analogWrite(MOTOR1_PWM_PIN, 255 - pwm1);
      analogWrite(MOTOR2_PWM_PIN, 255 - pwm2);
      
      // Avance la phase (Δphase = 2π * freq * Δt)
      float deltaTime = UPDATE_INTERVAL / 1000.0;  // secondes
      phase += 2.0 * PI * frequency * deltaTime;
      
      // Boucle sur 2π
      if (phase >= 2.0 * PI) {
        phase -= 2.0 * PI;
      }
    }
  }
}
