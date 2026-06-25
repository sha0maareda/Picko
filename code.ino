// =====================================================
//   Picko - Mobile Robot Arm
//   Motors  : L298N, 4x DC (2 parallel pairs)
//   Arm     : 4x MG90S servo (Base, Shoulder, Elbow, Gripper)
//   Nav     : 3x IR sensor (Left, Middle, Right)
//   Mode    : WiFi Access Point
//   Core    : ESP32 Arduino Core v3.x
// =====================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ----- WiFi -----
const char* ssid     = "Picko";
const char* password = "12345678";

// ----- Motor Pins -----
#define IN1  26
#define IN2  27
#define ENA  32
#define IN3  14
#define IN4  12
#define ENB  33

// ----- PWM -----
#define PWM_FREQ  1000
#define PWM_RES   8

// ----- Servo Pins -----
#define PIN_S1  13   // Base
#define PIN_S2  15   // Shoulder
#define PIN_S3  2    // Elbow
#define PIN_S4  4    // Gripper

// ----- IR Sensor Pins (input-only pins) -----
#define IR_L  34
#define IR_M  35
#define IR_R  36

// ----- Navigation Timing -----
#define TURN_TIME    600   // ms — increase if turn < 90deg, decrease if > 90deg
#define NAV_SPEED    180   // line following speed (0-255)
#define TURN_SPEED   160   // turning speed — keep lower than NAV_SPEED

// ----- Objects -----
Servo s1, s2, s3, s4;
WebServer server(80);

// ----- State -----
int motorSpeed = NAV_SPEED;
int angleS1 = 90, angleS2 = 90, angleS3 = 90, angleS4 = 90;

enum Station { HOME, WAREHOUSE, PRODUCTION, PACKAGING, NONE };
Station currentStation = HOME;
Station targetStation  = HOME;
bool    autoMode       = false;
bool    atStation      = false;

String stationName(Station s) {
  switch(s) {
    case HOME:       return "home";
    case WAREHOUSE:  return "warehouse";
    case PRODUCTION: return "production";
    case PACKAGING:  return "packaging";
    default:         return "unknown";
  }
}

Station nameToStation(String n) {
  if (n == "home")       return HOME;
  if (n == "warehouse")  return WAREHOUSE;
  if (n == "production") return PRODUCTION;
  if (n == "packaging")  return PACKAGING;
  return NONE;
}

// =====================================================
//   HTML
// =====================================================
const char PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Picko</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: sans-serif; background: #1a1a2e; color: #eee; display: flex; flex-direction: column; align-items: center; padding: 24px 16px; gap: 28px; min-height: 100vh; }
    h1 { font-size: 22px; color: #00d4ff; letter-spacing: 2px; }
    h2 { font-size: 14px; color: #00d4ff; letter-spacing: 1px; text-transform: uppercase; }
    .status { font-size: 12px; color: #aaa; background: #16213e; padding: 5px 14px; border-radius: 20px; }
    .section { width: 100%; max-width: 340px; background: #16213e; border-radius: 16px; padding: 18px; display: flex; flex-direction: column; align-items: center; gap: 16px; }
    .page { display: none; flex-direction: column; align-items: center; gap: 28px; width: 100%; }
    .page.active { display: flex; }
    .mode-btns { display: flex; gap: 16px; margin-top: 8px; }
    .mode-btn { display: flex; flex-direction: column; align-items: center; gap: 8px; padding: 20px 24px; background: #0f3460; border: none; border-radius: 16px; color: #eee; cursor: pointer; font-size: 13px; transition: background 0.15s, transform 0.1s; min-width: 130px; }
    .mode-btn:active { transform: scale(0.95); }
    .mode-btn.manual { border: 1.5px solid #00d4ff; }
    .mode-btn.auto   { border: 1.5px solid #1D9E75; }
    .mode-btn .icon  { font-size: 36px; }
    .back-btn { background: none; border: 1px solid #ffffff33; color: #aaa; padding: 6px 16px; border-radius: 20px; cursor: pointer; font-size: 12px; align-self: flex-start; }
    .back-btn:hover { border-color: #00d4ff; color: #00d4ff; }
    .grid { display: grid; grid-template-columns: repeat(3, 72px); grid-template-rows: repeat(3, 72px); gap: 8px; }
    .btn { width: 72px; height: 72px; border: none; border-radius: 14px; font-size: 26px; cursor: pointer; background: #0f3460; color: #eee; touch-action: manipulation; user-select: none; transition: background 0.1s, transform 0.1s; }
    .btn:active { background: #00d4ff; color: #1a1a2e; transform: scale(0.92); }
    .btn.stop  { background: #c0392b; color: #fff; font-size: 13px; font-weight: bold; }
    .btn.stop:active { background: #e74c3c; }
    .btn.empty { visibility: hidden; }
    .servo-row { display: flex; align-items: center; gap: 10px; width: 100%; }
    .servo-label { font-size: 12px; color: #aaa; min-width: 80px; }
    .servo-val   { font-size: 12px; color: #00d4ff; min-width: 38px; text-align: right; }
    input[type=range] { flex: 1; }
    .s1 { accent-color: #1D9E75; }
    .s2 { accent-color: #378ADD; }
    .s3 { accent-color: #EF9F27; }
    .s4 { accent-color: #D4537E; }
    .divider { width: 100%; height: 1px; background: #ffffff18; }
    .map-wrap { width: 100%; max-width: 340px; background: #16213e; border-radius: 16px; padding: 18px; display: flex; flex-direction: column; align-items: center; gap: 14px; }
    .map-grid { position: relative; width: 260px; height: 220px; }
    .map-line { position: absolute; background: #ffffff18; }
    .map-line.h { height: 1px; left: 36px; right: 36px; }
    .map-line.v { width: 1px; top: 22px; bottom: 22px; }
    .map-line.top    { top: 22px; }
    .map-line.bottom { bottom: 22px; }
    .map-line.left   { left: 36px; }
    .map-line.right  { right: 36px; }
    .stn { position: absolute; width: 100px; height: 44px; border-radius: 10px; border: 1.5px solid #ffffff22; background: #0f3460; display: flex; align-items: center; justify-content: center; gap: 6px; font-size: 12px; font-weight: bold; color: #aaa; cursor: pointer; transition: all 0.2s; }
    .stn:hover { border-color: #ffffff55; }
    .stn.active { border-color: #1D9E75; color: #1D9E75; box-shadow: 0 0 10px #1D9E7544; }
    .stn.top-right { top: 0; right: 0; }
    .stn.top-left  { top: 0; left: 0; }
    .stn.bot-left  { bottom: 0; left: 0; }
    .stn.bot-right { bottom: 0; right: 0; }
    .picko-dot { position: absolute; width: 20px; height: 20px; border-radius: 50%; background: #1D9E75; display: flex; align-items: center; justify-content: center; font-size: 11px; color: #fff; z-index: 10; }
    .moving-badge { font-size: 11px; color: #EF9F27; display: none; align-items: center; gap: 4px; }
    .cur-loc-row { font-size: 13px; color: #aaa; display: flex; align-items: center; gap: 6px; }
    .cur-loc-val { color: #eee; font-weight: bold; }
    .auto-btns { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; width: 100%; }
    .auto-btn { padding: 12px 8px; border: none; border-radius: 12px; font-size: 12px; cursor: pointer; font-weight: bold; color: #fff; transition: transform 0.1s; display: flex; align-items: center; justify-content: center; gap: 6px; }
    .auto-btn:active { transform: scale(0.95); }
  </style>
</head>
<body>

  <h1>&#x1F916; PICKO</h1>
  <div class="status" id="status">Ready</div>

  <!-- Home -->
  <div class="page active" id="page-home">
    <div class="section" style="gap:20px; padding:28px 18px;">
      <div style="font-size:64px;">&#x1F3ED;</div>
      <div style="font-size:13px; color:#aaa; letter-spacing:1px;">Mobile Robot Arm</div>
      <div class="mode-btns">
        <button class="mode-btn manual" onclick="showPage('manual')">
          <span class="icon">&#x1F3AE;</span>Manual
        </button>
        <button class="mode-btn auto" onclick="showPage('auto')">
          <span class="icon">&#x1F3ED;</span>Auto
        </button>
      </div>
    </div>
  </div>

  <!-- Manual -->
  <div class="page" id="page-manual">
    <button class="back-btn" onclick="showPage('home')">&#x2190; Home</button>
    <div class="section">
      <h2>&#x1F3CE; Car Control</h2>
      <div class="grid">
        <div class="btn empty"></div>
        <button class="btn" ontouchstart="sendCmd('forward')" ontouchend="sendCmd('stop')" onmousedown="sendCmd('forward')" onmouseup="sendCmd('stop')">&#x2191;</button>
        <div class="btn empty"></div>
        <button class="btn" ontouchstart="sendCmd('left')" ontouchend="sendCmd('stop')" onmousedown="sendCmd('left')" onmouseup="sendCmd('stop')">&#x2190;</button>
        <button class="btn stop" onclick="sendCmd('stop')">STOP</button>
        <button class="btn" ontouchstart="sendCmd('right')" ontouchend="sendCmd('stop')" onmousedown="sendCmd('right')" onmouseup="sendCmd('stop')">&#x2192;</button>
        <div class="btn empty"></div>
        <button class="btn" ontouchstart="sendCmd('backward')" ontouchend="sendCmd('stop')" onmousedown="sendCmd('backward')" onmouseup="sendCmd('stop')">&#x2193;</button>
        <div class="btn empty"></div>
      </div>
    </div>
    <div class="section">
      <h2>&#x1F9BE; Arm Control</h2>
      <div class="servo-row">
        <span class="servo-label">S1 Base</span>
        <input class="s1" type="range" min="0" max="180" value="90" oninput="setServo(1,this.value)">
        <span class="servo-val" id="v1">90&#xB0;</span>
      </div>
      <div class="servo-row">
        <span class="servo-label">S2 Shoulder</span>
        <input class="s2" type="range" min="0" max="180" value="90" oninput="setServo(2,this.value)">
        <span class="servo-val" id="v2">90&#xB0;</span>
      </div>
      <div class="servo-row">
        <span class="servo-label">S3 Elbow</span>
        <input class="s3" type="range" min="0" max="180" value="90" oninput="setServo(3,this.value)">
        <span class="servo-val" id="v3">90&#xB0;</span>
      </div>
      <div class="servo-row">
        <span class="servo-label">S4 Gripper</span>
        <input class="s4" type="range" min="0" max="180" value="90" oninput="setServo(4,this.value)">
        <span class="servo-val" id="v4">90&#xB0;</span>
      </div>
      <div class="divider"></div>
      <div style="display:flex; gap:8px; flex-wrap:wrap; justify-content:center;">
        <button class="btn" style="width:auto;padding:0 14px;font-size:12px;" onclick="setPreset('home')">Home</button>
        <button class="btn" style="width:auto;padding:0 14px;font-size:12px;" onclick="setPreset('pick')">Pick</button>
        <button class="btn" style="width:auto;padding:0 14px;font-size:12px;" onclick="setPreset('drop')">Drop</button>
      </div>
    </div>
  </div>

  <!-- Auto -->
  <div class="page" id="page-auto">
    <button class="back-btn" onclick="showPage('home')">&#x2190; Home</button>
    <div class="map-wrap">
      <h2>&#x1F3ED; Picko Auto</h2>
      <div class="map-grid">
        <div class="map-line h top"></div>
        <div class="map-line h bottom"></div>
        <div class="map-line v left"></div>
        <div class="map-line v right"></div>
        <div class="stn top-left"  id="stn-production" onclick="goTo('production')">&#x2699; Production</div>
        <div class="stn top-right" id="stn-warehouse"  onclick="goTo('warehouse')">&#x1F4E6; Warehouse</div>
        <div class="stn bot-left"  id="stn-packaging"  onclick="goTo('packaging')">&#x1F4CB; Packaging</div>
        <div class="stn bot-right" id="stn-home"       onclick="goTo('home')">&#x1F3E0; Home</div>
        <div class="picko-dot" id="picko">&#x1F916;</div>
      </div>
      <div class="cur-loc-row">Location: <span class="cur-loc-val" id="cur-loc">Home</span></div>
      <div class="moving-badge" id="moving-badge">&#x27A1; Moving to <span id="moving-to"></span>...</div>
      <div class="divider"></div>
      <div class="auto-btns">
        <button class="auto-btn" style="background:#444;"    onclick="goTo('home')">&#x1F3E0; Home</button>
        <button class="auto-btn" style="background:#378ADD;" onclick="goTo('warehouse')">&#x1F4E6; Warehouse</button>
        <button class="auto-btn" style="background:#EF9F27;color:#1a1a2e;" onclick="goTo('production')">&#x2699; Production</button>
        <button class="auto-btn" style="background:#1D9E75;" onclick="goTo('packaging')">&#x1F4CB; Packaging</button>
      </div>
    </div>
  </div>

  <script>
    function showPage(name) {
      document.querySelectorAll('.page').forEach(function(p) { p.classList.remove('active'); });
      document.getElementById('page-' + name).classList.add('active');
      setStatus(name === 'home' ? 'Ready' : name === 'manual' ? 'Manual mode' : 'Auto mode');
    }
    function setStatus(msg) { document.getElementById('status').textContent = msg; }

    function sendCmd(cmd) {
      setStatus(cmd.toUpperCase());
      fetch('/cmd?action=' + cmd).catch(function() { setStatus('Lost connection'); });
    }

    var keyMap = { ArrowUp:'forward', ArrowDown:'backward', ArrowLeft:'left', ArrowRight:'right', ' ':'stop' };
    var held = {};
    document.addEventListener('keydown', function(e) {
      if (keyMap[e.key] && !held[e.key]) { held[e.key]=true; sendCmd(keyMap[e.key]); e.preventDefault(); }
    });
    document.addEventListener('keyup', function(e) {
      if (keyMap[e.key]) { held[e.key]=false; sendCmd('stop'); }
    });

    function setServo(n, val) {
      document.getElementById('v'+n).textContent = val+'°';
      fetch('/servo?n='+n+'&val='+val);
    }
    var presets = {
      home: [90, 90,  90,  90],
      pick: [90, 45,  135, 10],
      drop: [180,90,  90,  170]
    };
    function setPreset(name) {
      var p = presets[name];
      var sliders = document.querySelectorAll('#page-manual input[type=range]');
      for (var i=0; i<4; i++) { sliders[i].value=p[i]; setServo(i+1,p[i]); }
    }

    var stnPos = {
      home:       { right:'0px', bottom:'0px', left:'',   top:''   },
      warehouse:  { right:'0px', top:'0px',    bottom:'', left:''  },
      production: { left:'0px',  top:'0px',    bottom:'', right:'' },
      packaging:  { left:'0px',  bottom:'0px', top:'',    right:'' }
    };
    var stnNames = { home:'Home', warehouse:'Warehouse', production:'Production', packaging:'Packaging' };
    var curLoc = 'home';

    function updateMap(loc) {
      var stations = ['home','warehouse','production','packaging'];
      for (var i=0; i<stations.length; i++) {
        var el = document.getElementById('stn-'+stations[i]);
        if (stations[i] === loc) el.classList.add('active');
        else el.classList.remove('active');
      }
      var p = stnPos[loc];
      var dot = document.getElementById('picko');
      dot.style.right  = p.right;
      dot.style.left   = p.left;
      dot.style.top    = p.top;
      dot.style.bottom = p.bottom;
      document.getElementById('cur-loc').textContent = stnNames[loc];
    }

    function goTo(target) {
      if (target === curLoc) return;
      document.getElementById('moving-badge').style.display = 'flex';
      document.getElementById('moving-to').textContent = stnNames[target];
      setStatus('Moving to ' + stnNames[target] + '...');
      fetch('/go?target=' + target).catch(function() { setStatus('Lost connection'); });
      setTimeout(function() {
        curLoc = target;
        updateMap(target);
        document.getElementById('moving-badge').style.display = 'none';
        setStatus('At ' + stnNames[target]);
      }, 1200);
    }

    updateMap('home');

    setInterval(function() {
      fetch('/location')
        .then(function(r) { return r.json(); })
        .then(function(d) { if(d.loc && d.loc !== curLoc) { curLoc=d.loc; updateMap(d.loc); } })
        .catch(function() {});
    }, 2000);
  </script>

</body>
</html>
)rawhtml";

// =====================================================
//   Motor Functions
// =====================================================
void stopMotors() {
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,LOW);
  ledcWrite(ENA,0); ledcWrite(ENB,0);
}
void moveForward(int spd = -1) {
  if (spd < 0) spd = motorSpeed;
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
  ledcWrite(ENA,spd); ledcWrite(ENB,spd);
}
void moveBackward() {
  digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH);
  digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH);
  ledcWrite(ENA,motorSpeed); ledcWrite(ENB,motorSpeed);
}
void turnLeft(int spd = -1) {
  if (spd < 0) spd = motorSpeed;
  digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
  ledcWrite(ENA,spd); ledcWrite(ENB,spd);
}
void turnRight(int spd = -1) {
  if (spd < 0) spd = motorSpeed;
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH);
  ledcWrite(ENA,spd); ledcWrite(ENB,spd);
}

// =====================================================
//   IR Sensors
// =====================================================
void readIR(int &L, int &M, int &R) {
  L = digitalRead(IR_L);
  M = digitalRead(IR_M);
  R = digitalRead(IR_R);
}

Station detectStation() {
  int L, M, R;
  readIR(L, M, R);
  if (L==1 && M==1 && R==1) return PRODUCTION;
  if (L==0 && M==0 && R==0) return WAREHOUSE;
  if (L==1 && M==0 && R==1) return PACKAGING;
  if (L==0 && M==0 && R==1) return HOME;
  return NONE;
}

// =====================================================
//   Line Following
// =====================================================
void followLine() {
  int L, M, R;
  readIR(L, M, R);
  if      (M==1 && L==0 && R==0) moveForward(NAV_SPEED);
  else if (L==1 && M==0)          turnRight(TURN_SPEED);
  else if (R==1 && M==0)          turnLeft(TURN_SPEED);
  else                             moveForward(NAV_SPEED);
}

// =====================================================
//   Turn 90 Degrees Right
// =====================================================
void turnRight90() {
  turnRight(TURN_SPEED);
  delay(TURN_TIME);
  stopMotors();
  delay(200);
}

// =====================================================
//   Station Actions
// =====================================================
void doWarehouseAction() {
  Serial.println("Station: Warehouse");
  // Add arm pick sequence here
}

void doProductionAction() {
  Serial.println("Station: Production");
  // Add arm place/pick sequence here
}

void doPackagingAction() {
  Serial.println("Station: Packaging");
  // Add arm place sequence here
}

// =====================================================
//   Navigation
// =====================================================
void navigationLoop() {
  if (!autoMode) return;
  if (currentStation == targetStation) { stopMotors(); return; }

  Station detected = detectStation();

  if (detected != NONE && !atStation) {
    atStation = true;
    stopMotors();
    delay(300);
    currentStation = detected;
    Serial.println("Arrived at: " + stationName(currentStation));

    if      (currentStation == WAREHOUSE)  doWarehouseAction();
    else if (currentStation == PRODUCTION) doProductionAction();
    else if (currentStation == PACKAGING)  doPackagingAction();
    else if (currentStation == HOME)       { autoMode = false; return; }

    if (currentStation == targetStation) {
      autoMode = false;
      Serial.println("Target reached: " + stationName(targetStation));
      return;
    }

    delay(300);
    turnRight90();
    atStation = false;

  } else if (detected == NONE) {
    atStation = false;
    followLine();
  }
}

// =====================================================
//   Server Handlers
// =====================================================
void handleRoot() { server.send_P(200, "text/html", PAGE); }

void handleCmd() {
  if (!server.hasArg("action")) { server.send(400,"text/plain","Missing action"); return; }
  String a = server.arg("action");
  if      (a=="forward")  moveForward();
  else if (a=="backward") moveBackward();
  else if (a=="left")     turnLeft();
  else if (a=="right")    turnRight();
  else if (a=="stop")     stopMotors();
  server.send(200,"text/plain","OK");
}

void handleServo() {
  if (!server.hasArg("n")||!server.hasArg("val")) { server.send(400,"text/plain","Missing args"); return; }
  int n   = server.arg("n").toInt();
  int val = constrain(server.arg("val").toInt(), 0, 180);
  switch(n) {
    case 1: s1.write(val); angleS1=val; break;
    case 2: s2.write(val); angleS2=val; break;
    case 3: s3.write(val); angleS3=val; break;
    case 4: s4.write(val); angleS4=val; break;
  }
  server.send(200,"text/plain","OK");
}

void handleGo() {
  if (!server.hasArg("target")) { server.send(400,"text/plain","Missing target"); return; }
  Station t = nameToStation(server.arg("target"));
  if (t == NONE) { server.send(400,"text/plain","Unknown target"); return; }
  targetStation = t;
  autoMode  = true;
  atStation = false;
  Serial.println("Target set: " + stationName(t));
  server.send(200,"text/plain","OK");
}

void handleLocation() {
  String json = "{\"loc\":\"" + stationName(currentStation) + "\"}";
  server.send(200,"application/json", json);
}

// =====================================================
//   Setup
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  ledcAttach(ENA, PWM_FREQ, PWM_RES);
  ledcAttach(ENB, PWM_FREQ, PWM_RES);
  stopMotors();

  ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2); ESP32PWM::allocateTimer(3);
  s1.setPeriodHertz(50); s1.attach(PIN_S1,500,2400);
  s2.setPeriodHertz(50); s2.attach(PIN_S2,500,2400);
  s3.setPeriodHertz(50); s3.attach(PIN_S3,500,2400);
  s4.setPeriodHertz(50); s4.attach(PIN_S4,500,2400);
  s1.write(90); s2.write(90); s3.write(90); s4.write(90);

  pinMode(IR_L, INPUT);
  pinMode(IR_M, INPUT);
  pinMode(IR_R, INPUT);

  WiFi.softAP(ssid, password);
  Serial.println("IP: " + WiFi.softAPIP().toString());

  server.on("/",         handleRoot);
  server.on("/cmd",      handleCmd);
  server.on("/servo",    handleServo);
  server.on("/go",       handleGo);
  server.on("/location", handleLocation);
  server.onNotFound([]() { server.send(404,"text/plain","Not found"); });

  server.begin();
  Serial.println("Picko online.");
}

// =====================================================
//   Loop
// =====================================================
void loop() {
  server.handleClient();
  navigationLoop();
}
