#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>

#define buzzer 4
#define m_step 25
#define m_dir 26
#define m_enable 33
#define flow_interrupt 35
#define manual_close 15
#define manual_open 2

const char* PARAM_INPUT_1 = "inputSP";
const char* PARAM_INPUT_2 = "absPos";
const char* PARAM_INPUT_3 = "relativePos";
const char* PARAM_INPUT_4 = "KpValue";
const char* PARAM_INPUT_5 = "KiValue";
const char* PARAM_INPUT_6 = "KdValue";
const char* PARAM_INPUT_7 = "UpdtInterval";
const char* PARAM_INPUT_8 = "AdvConfig";


bool controle = false; //0=Manual 1=Automatico
float valv_position = 0;
static float curso = 590;
static float Tolerancia_erro = 0.4; //Tolerancia para erro de posição em porcentagem

float tempo_de_amostragem = 500;
byte m_velocity = 5;
bool trava_motor = false;
float abs_pos = 0;
int setPoint = 0;
int Steap = 0;
int n_cicles = 0;
float frequency = 0;
float flow = 0;
double tempo_ultimo_pulso = 0;
double tempo_ultimo_calc = 0;
double tempo_ultimo_calc2 = 0;
double tempo_ultimo_calc3 = 0;
int tempo_morto = 1500;

bool SET_CMD_WEB = false;
String CMD_WEB = "";

class PID {
  public:
    double error;
    double sample;
    double lastSample;
    double kP, kI, kD;
    double P, I, D;
    double pid;
    double setPoint;
    long lastProcess;

    PID(double _kP, double _kI, double _kD) {
      kP = _kP;
      kI = _kI;
      kD = _kD;
    }

    void addNewSample(double _sample) {
      sample = _sample;
    }

    void setSetPoint(double _setPoint) {
      setPoint = _setPoint;
    }

    double process() {
      error = setPoint - sample;
      float deltaTime = (millis() - lastProcess) / 1000.0;
      lastProcess = millis();
      P = error * kP;
      I = I + (error * kI) * deltaTime;
      D = (lastSample - sample) * kD / deltaTime;
      lastSample = sample;
      if (P > 100) {
        P = 100;
      } if (P < 0) {
        P = 0;
      }
      if (I > 100) {
        I = 100;
      }
      if (I < 0) {
        I = 0;
      }
      pid = P + I + D;
      if (pid > 100) {
        pid = 100;
      } if (pid < 0) {
        pid = 0;
      }
      return pid;
    }
};

PID meuPid(0.03, 0.008, 0);

// Replace with your network credentials
const char* ssid = "Gabriel B (2.4Ghz)";
const char* password = "999514334";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 800;

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else {
    Serial.println("SPIFFS mounted successfully");
  }
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  pinMode(buzzer, OUTPUT);
  pinMode(m_step, OUTPUT);
  pinMode(m_dir, OUTPUT);
  pinMode(m_enable, OUTPUT);
  pinMode(flow_interrupt, INPUT);
  pinMode(manual_close, INPUT_PULLDOWN);
  pinMode(manual_open, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(flow_interrupt), treatPulse, FALLING);
  digitalWrite(m_step, LOW);
  digitalWrite(m_dir, LOW);
  if (trava_motor) {
    digitalWrite(m_enable, LOW);
  } else {
    digitalWrite(m_enable, HIGH);
  }
  //init_valv();
  //tone(buzzer, 800, 400);
  //Serial.println("INIT");
  initWiFi();
  initSPIFFS();
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });
  server.serveStatic("/", SPIFFS, "/");
  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String();
  });




  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      SET_CMD_WEB = true;
      CMD_WEB = "S" + inputMessage + "\n";
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      SET_CMD_WEB = true;
      CMD_WEB = "A" + inputMessage + "\n";
    }
    // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_3)) {
      inputMessage = request->getParam(PARAM_INPUT_3)->value();
      inputParam = PARAM_INPUT_3;
      SET_CMD_WEB = true;
      CMD_WEB = "R" + inputMessage + "\n";
    }
    else if (request->hasParam(PARAM_INPUT_8)) {
      inputMessage = request->getParam(PARAM_INPUT_8)->value();
      inputParam = PARAM_INPUT_8;
      SET_CMD_WEB = true;
      CMD_WEB = inputMessage + "\n";
    }

    else if (request->hasParam(PARAM_INPUT_4)) {
      inputMessage = request->getParam(PARAM_INPUT_4)->value();
      inputParam = PARAM_INPUT_4;
      SET_CMD_WEB = true;
      CMD_WEB = "P" + inputMessage + "\n";
    }
    else if (request->hasParam(PARAM_INPUT_5)) {
      inputMessage = request->getParam(PARAM_INPUT_5)->value();
      inputParam = PARAM_INPUT_5;
      SET_CMD_WEB = true;
      CMD_WEB = "I" + inputMessage + "\n";
    }
    else if (request->hasParam(PARAM_INPUT_6)) {
      inputMessage = request->getParam(PARAM_INPUT_6)->value();
      inputParam = PARAM_INPUT_6;
      SET_CMD_WEB = true;
      CMD_WEB = "D" + inputMessage + "\n";
    }


    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field ("
                  + inputParam + ") with value: " + inputMessage +
                  "<br><a href=\"/\">Return to Home Page</a>");
  });





  events.onConnect([](AsyncEventSourceClient * client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  // Start server

  server.begin();
}

String getSensorReadings() {
  readings["sensor1"] = abs_pos;
  readings["sensor2"] = (Re_map(valv_position, 0.0, curso, 0.0, 100.0));
  readings["sensor3"] = flow;
  readings["sensor4"] = meuPid.setPoint;
  readings["sensor5"] = meuPid.kP;
  readings["sensor6"] = meuPid.kI;
  readings["sensor7"] = meuPid.kD;
  readings["sensor8"] = meuPid.pid;;

  String jsonString = JSON.stringify(readings);
  return jsonString;
}

float Re_map(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void close_fn() {
  digitalWrite(m_enable, LOW);
  valv_position--;
  digitalWrite(m_dir, LOW);
  digitalWrite(m_step, HIGH);
  delay(m_velocity);
  digitalWrite(m_step, LOW);
  delay(m_velocity);
  digitalWrite(m_dir, LOW);
  if (trava_motor == false) {
    digitalWrite(m_enable, HIGH);
  }
  //Serial.print("Position: "); Serial.print(map(valv_position, 0, curso, 0, 100)); Serial.println(" %");
}

void open_fn () {
  digitalWrite(m_enable, LOW);
  valv_position++;
  digitalWrite(m_dir, HIGH);
  digitalWrite(m_step, HIGH);
  delay(m_velocity);
  digitalWrite(m_step, LOW);
  delay(m_velocity);
  digitalWrite(m_dir, LOW);
  if (trava_motor == false) {
    digitalWrite(m_enable, HIGH);
  }
  //Serial.print("Position: "); Serial.print(map(valv_position, 0, 1133, 0, 100)); Serial.println(" %");
}

int go_to_absolute_position (float future_pos_porcent) {
  if ((future_pos_porcent >= 0) && (future_pos_porcent <= 100)) {
    float future_pos = Re_map(future_pos_porcent, 0, 100.0, 0.0, curso);
    abs_pos = future_pos_porcent;
    if ((abs(valv_position - future_pos)) < Tolerancia_erro) {
      return 2;
    }
    if (valv_position > future_pos) {
      measurements();
      while (valv_position >= future_pos) {
        close_fn();
        measurements();
        if ((millis() - lastTime) > timerDelay) {
          events.send("ping", NULL, millis());
          events.send(getSensorReadings().c_str(), "new_readings" , millis());
          lastTime = millis();
        }
      }
      measurements();
      return 1;//OK
    } else if (valv_position < future_pos) {
      measurements();
      while (valv_position <= future_pos) {
        open_fn();
        measurements();
        if ((millis() - lastTime) > timerDelay) {
          events.send("ping", NULL, millis());
          events.send(getSensorReadings().c_str(), "new_readings" , millis());
          lastTime = millis();
        }
      }
      measurements();
      return 1;//OK
    } else {
      //Serial.println("A válvula já se encontra nessa posição!");
      return 2;//Já está nessa posição
    }
  } else {
    //Serial.println("A posição digitada é inválida escolha um intervalo entre 0 e 100%");
    return 0;//Não é uma posição válida
  }

}

int go_to_relative_position (float future_pos_porcent) {
  float actual_pos_porcent = Re_map(valv_position, 0.0, curso, 0.0, 100.0);
  if (future_pos_porcent + actual_pos_porcent > 100) {
    go_to_absolute_position(100);//Saturada em 100%
    return 3;
  } else if (future_pos_porcent + actual_pos_porcent < 0) {
    go_to_absolute_position(0);
    return 2;//Saturada em 0%
  } else {
    go_to_absolute_position(actual_pos_porcent + future_pos_porcent);
    return 1;//OK
  }
}

void calc_flow() {
  if ((millis() - tempo_ultimo_calc) >= tempo_de_amostragem) {
    //Serial.println("CALC");
    frequency = (n_cicles / (tempo_de_amostragem / 1000));
    //flow = ( (0.000181 * pow(frequency, 3)) - (0.0211 * pow(frequency, 2)) + (7.9 * frequency) - (0));
    flow = ((7.6724 * frequency) - 7.727);
    //Serial.print("FLOW: "); Serial.println(flow);
    if (flow < 0) {
      flow = 0;
    }
    n_cicles = 0;
    tempo_ultimo_calc = millis();
    if (controle) {
      if ((millis() - tempo_ultimo_calc3) >= tempo_morto) {
        meuPid.addNewSample(flow);
        tempo_ultimo_calc3 = millis();
      }
      //Adicionando uma nova leitura
    }
  }
}

void treatPulse() {
  n_cicles++;
  //tempo_ultimo_pulso = millis();
}

void serialEvent() {
  String inputString = "";
  bool stringComplete = false;
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
  if (stringComplete) {
    serialcommand(inputString);
  }
  inputString = "";
  stringComplete = false;
}
void serialcommand(String inputString) {
  String value;
  int i = 1;
  while (inputString[i] != '\n') {
    value += inputString[i];
    i++;
  }
  //Serial.println(inputString);
  switch (inputString[0]) {
    case 'S':
    case 's':
      //Controle automatico  - Setpoint
      controle = true;
      //Serial.print("SetPint "); Serial.println(value.toFloat());
      setPoint = value.toFloat();
      meuPid.setSetPoint(setPoint);
      meuPid.I = 0;
      meuPid.P = 0;
      meuPid.D = 0;
      break;
    case 'R':
    case 'r':
      //Controle Manual  - posição relativa %
      controle = false;
      //Serial.print("Posição relativa: "); Serial.println(value.toFloat());
      go_to_relative_position(value.toFloat());
      meuPid.I = 0;
      meuPid.P = 0;
      meuPid.D = 0;
      meuPid.setPoint = 0;
      break;
    case 'A':
    case 'a':
      //Controle Manual  - posição relativa %
      controle = false;
      meuPid.I = 0;
      meuPid.P = 0;
      meuPid.D = 0;
      go_to_absolute_position(value.toFloat());
      meuPid.setPoint = 0;
      break;
    case 'H':
    case 'h':
      controle = false;
      init_valv();
      meuPid.I = 0;
      meuPid.P = 0;
      meuPid.D = 0;
      go_to_absolute_position(value.toFloat());
      meuPid.setPoint = 0;
      break;
    case 'L':
    case 'l':
      controle = false;
      measurements();
      meuPid.I = 0;
      meuPid.P = 0;
      meuPid.D = 0;
      break;
    case 'P':
    case 'p':
      meuPid.kP = value.toFloat();
      measurements();
      meuPid.P = 0;
      break;
    case 'I':
    case 'i':
      meuPid.kI = value.toFloat();
      measurements();
      meuPid.I = 0;
      break;
    case 'D':
    case 'd':
      meuPid.kD = value.toFloat();
      measurements();
      meuPid.D = 0;
      break;
    case 'W':
    case 'w':
      timerDelay = value.toInt();
      if (timerDelay > 2500) {
        timerDelay = 800;
      } if (timerDelay < 300) {
        timerDelay = 800;
      }
      measurements();
      break;
    case 'M':
    case 'm':
      tempo_morto = value.toInt();
      if (timerDelay > 3500) {
        tempo_morto = 1500;
      }
      measurements();
      break;
      case 'V':
      case 'v':
      m_velocity = value.toInt();
      if (m_velocity > 15) {
        m_velocity = 15;
      } if (m_velocity < 2) {
        m_velocity = 2;
      }
      measurements();
      break;     
    default:
      Serial.println("Comando não reconhecido");
      break;
  }
  inputString = "";
}

void init_valv() {
  int i = 0;
  for (int i = 0; i < 600; i++) {
    digitalWrite(m_enable, LOW);
    digitalWrite(m_dir, LOW);
    digitalWrite(m_step, HIGH);
    delay(m_velocity);
    digitalWrite(m_step, LOW);
    delay(m_velocity);
    digitalWrite(m_dir, LOW);
    if (trava_motor == false) {
      digitalWrite(m_enable, HIGH);
    }
  }
  valv_position = 0;
  //Serial.print("Position: "); Serial.print(map(valv_position, 0, curso, 0, 100)); Serial.println(" %");
}


void measurements() {
  calc_flow();
  Serial.print("Controller: "); Serial.print(abs_pos);
  Serial.print("; Position: "); Serial.print((float)Re_map(valv_position, 0.0, curso, 0.0, 100.0));
  Serial.print(" ; Flow: "); Serial.print(flow);
  if (controle) {
    Serial.print(" ; SetPoint: "); Serial.print(meuPid.setPoint);
    Serial.print(" ; P: "); Serial.print(meuPid.P);
    Serial.print(" ; I: "); Serial.print(meuPid.I);
    Serial.print(" ; D: "); Serial.print(meuPid.D);
    Serial.print(" ; PID: "); Serial.print(meuPid.pid);
  }
  Serial.print("\n");
}

void loop() {
  if (SET_CMD_WEB) {
    serialcommand(CMD_WEB);
    SET_CMD_WEB = false;

  }
  calc_flow();
  if ((millis() - lastTime) > timerDelay) {
    events.send("ping", NULL, millis());
    events.send(getSensorReadings().c_str(), "new_readings" , millis());
    lastTime = millis();
  }
  if (controle) {
    measurements();
    if ((millis() - tempo_ultimo_calc2) >= tempo_morto) {
      go_to_absolute_position(meuPid.process());
      tempo_ultimo_calc2 = millis();
    }
  }
  if (digitalRead(manual_open)) {
    open_fn();
    measurements();
  }
  if (digitalRead(manual_close)) {
    close_fn();
    measurements();
  }
  if (valv_position > curso) {
    valv_position = curso;
  } else if (valv_position < 0) {
    valv_position = 0;
  }
}
