// LIBRERIAS PARA I2C Y SERVOMOTORES
#include <Wire.h>
#include <Servo.h>

// DIRECCIONES DEL BUS I2C
const byte MI_DIR = 2;
const byte DIR_MAESTRO = 1;

// PINES DE LOS SERVOMOTORES
const byte PIN_SERVO_X = 3;
const byte PIN_SERVO_Y = 11;

// PINES DEL JOYSTICK ANALOGICO
const byte PIN_JOY_X = A0;
const byte PIN_JOY_Y = A1;
const byte PIN_BOTON_JOY = 2;

// PINES DE LOS 3 SENSORES INFRARROJOS FC-51
const byte PIN_IR_INICIO = 12;
const byte PIN_IR_TRAMPA = A2;
const byte PIN_IR_META = A3;

// PINES DEL DISPLAY DE 7 SEGMENTOS DE A2
const byte SEG[7] = {4, 5, 6, 7, 8, 9, 10};
const byte DIG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// OBJETOS PARA CONTROLAR LOS DOS SERVOS
Servo servoX;
Servo servoY;

// ANGULOS CENTRALES Y RANGO INICIAL DE INCLINACION
const int CENTRO_X = 90;
const int CENTRO_Y = 90;
int rangoMaximo = 12;
int modoSensibilidad = 1;

// VARIABLES PARA EL SUAVIZADO EXPONENCIAL DE LOS SERVOS
float anguloActualX = 90.0;
float anguloActualY = 90.0;

// VARIABLES DE TIEMPO Y ESTADO ANTERIOR DE SENSORES
unsigned long tAnteriorServos = 0;
unsigned long tDebounceSensores = 0;
unsigned long tPresionBoton = 0;
bool botonAnterior = false;
bool irInicioAnt = HIGH;
bool irTrampaAnt = HIGH;
bool irMetaAnt = HIGH;

// FUNCION QUE MUESTRA LA VELOCIDAD ACTUAL EN EL DISPLAY DE A2
void mostrarDisplay(int n) {
  for (int i = 0; i < 7; i++) {
    digitalWrite(SEG[i], bitRead(DIG[n], i));
  }
}

// FUNCION QUE ENVIA EL EVENTO DEL SENSOR AL ARDUINO MAESTRO
void enviarEvento(char c) {
  Wire.beginTransmission(DIR_MAESTRO);
  Wire.write(c);
  Wire.endTransmission();
}

void setup() {
  // INICIAR BUS I2C CON DIRECCION 2
  Wire.begin(MI_DIR);

  // CONFIGURAR PINES DEL DISPLAY COMO SALIDAS
  for (int i = 0; i < 7; i++) {
    pinMode(SEG[i], OUTPUT);
  }

  // CONFIGURAR BOTON DEL JOYSTICK Y SENSORES IR CON PULLUP
  pinMode(PIN_BOTON_JOY, INPUT_PULLUP);
  pinMode(PIN_IR_INICIO, INPUT_PULLUP);
  pinMode(PIN_IR_TRAMPA, INPUT_PULLUP);
  pinMode(PIN_IR_META, INPUT_PULLUP);

  // CONECTAR Y CENTRAR SERVOS A 90 GRADOS
  servoX.attach(PIN_SERVO_X);
  servoY.attach(PIN_SERVO_Y);
  servoX.write(CENTRO_X);
  servoY.write(CENTRO_Y);

  // MOSTRAR VELOCIDAD 1 AL ARRANCAR
  mostrarDisplay(modoSensibilidad);
}

void loop() {
  // 1. ACTUALIZAR SERVOMOTORES CADA 20 MILISEGUNDOS
  if (millis() - tAnteriorServos >= 20) {
    tAnteriorServos = millis();

    // LEER POSICION X E Y DEL JOYSTICK
    int lecturaX = analogRead(PIN_JOY_X);
    int lecturaY = analogRead(PIN_JOY_Y);

    // MAPEAR LECTURA AL RANGO DE GRADOS SELECCIONADO
    int metaX = map(lecturaX, 0, 1023, CENTRO_X - rangoMaximo, CENTRO_X + rangoMaximo);
    int metaY = map(lecturaY, 0, 1023, CENTRO_Y - rangoMaximo, CENTRO_Y + rangoMaximo);

    // SUAVIZAR EL MOVIMIENTO PARA EVITAR LATIGAZOS
    anguloActualX = (anguloActualX * 0.80) + (metaX * 0.20);
    anguloActualY = (anguloActualY * 0.80) + (metaY * 0.20);

    // MOVER SERVOS A LA POSICION FILTRADA
    servoX.write((int)anguloActualX);
    servoY.write((int)anguloActualY);
  }

  // 2. CAMBIAR VELOCIDAD CON EL BOTON DEL JOYSTICK SIN REINICIAR PARTIDA
  bool botonPresionado = (digitalRead(PIN_BOTON_JOY) == LOW);
  if (botonPresionado && !botonAnterior && millis() - tPresionBoton > 250) {
    tPresionBoton = millis();

    // ALTERNAR ENTRE VELOCIDAD 1 (12 GRADOS) Y VELOCIDAD 2 (22 GRADOS)
    if (modoSensibilidad == 1) {
      modoSensibilidad = 2;
      rangoMaximo = 22;
    } else {
      modoSensibilidad = 1;
      rangoMaximo = 12;
    }
    mostrarDisplay(modoSensibilidad);
  }
  botonAnterior = botonPresionado;

  // 3. MONITOREAR LOS 3 SENSORES INFRARROJOS
  if (millis() - tDebounceSensores >= 150) {
    bool actInicio = digitalRead(PIN_IR_INICIO);
    bool actTrampa = digitalRead(PIN_IR_TRAMPA);
    bool actMeta = digitalRead(PIN_IR_META);

    // AVISAR SI EL BALIN PASO POR INICIO
    if (actInicio == LOW && irInicioAnt == HIGH) {
      enviarEvento('I');
      tDebounceSensores = millis();
    }

    // AVISAR SI EL BALIN CAYO EN TRAMPA
    if (actTrampa == LOW && irTrampaAnt == HIGH) {
      enviarEvento('T');
      tDebounceSensores = millis();
    }

    // AVISAR SI EL BALIN LLEGO A LA META
    if (actMeta == LOW && irMetaAnt == HIGH) {
      enviarEvento('M');
      tDebounceSensores = millis();
    }

    irInicioAnt = actInicio;
    irTrampaAnt = actTrampa;
    irMetaAnt = actMeta;
  }
}