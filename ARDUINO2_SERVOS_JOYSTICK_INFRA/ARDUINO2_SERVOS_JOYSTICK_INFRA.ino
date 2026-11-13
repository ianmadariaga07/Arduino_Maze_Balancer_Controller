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

// ANGULOS CENTRALES Y LIMITES DE INCLINACION EN GRADOS
const int CENTRO_X = 90;
const int CENTRO_Y = 90;
int rangoMaximo = 15;
int modoSensibilidad = 1;

// VARIABLES PARA EL FILTRO DE SUAVIZADO DE MOVIMIENTO
float anguloActualX = 90.0;
float anguloActualY = 90.0;

// VARIABLES DE TIEMPO Y ANTIRREBOTE
unsigned long tAnteriorServos = 0;
unsigned long tDebounceSensores = 0;
unsigned long tPresionBoton = 0;
bool botonAnterior = false;
bool irInicioAnt = HIGH;
bool irTrampaAnt = HIGH;
bool irMetaAnt = HIGH;

// FUNCION QUE MUESTRA EL MODO EN EL DISPLAY DE A2
void mostrarDisplay(int n) {
  for (int i = 0; i < 7; i++) {
    digitalWrite(SEG[i], bitRead(DIG[n], i));
  }
}

// FUNCION QUE ENVIA UN CARACTER POR I2C AL ARDUINO 1
void enviarEvento(char c) {
  Wire.beginTransmission(DIR_MAESTRO);
  Wire.write(c);
  Wire.endTransmission();
}

void setup() {
  // INICIAR I2C CON DIRECCION 2
  Wire.begin(MI_DIR);

  // CONFIGURAR DISPLAY DE 7 SEGMENTOS
  for (int i = 0; i < 7; i++) {
    pinMode(SEG[i], OUTPUT);
  }

  // CONFIGURAR BOTON Y SENSORES INFRARROJOS
  pinMode(PIN_BOTON_JOY, INPUT_PULLUP);
  pinMode(PIN_IR_INICIO, INPUT_PULLUP);
  pinMode(PIN_IR_TRAMPA, INPUT_PULLUP);
  pinMode(PIN_IR_META, INPUT_PULLUP);

  // CONECTAR SERVOS Y POSICIONARLOS A 90 GRADOS EXACTOS
  servoX.attach(PIN_SERVO_X);
  servoY.attach(PIN_SERVO_Y);
  servoX.write(CENTRO_X);
  servoY.write(CENTRO_Y);

  // MOSTRAR MODO DE SENSIBILIDAD 1 EN EL DISPLAY
  mostrarDisplay(modoSensibilidad);
}

void loop() {
  // 1. ACTUALIZAR POSICION DE SERVOS CADA 20 MILISEGUNDOS
  if (millis() - tAnteriorServos >= 20) {
    tAnteriorServos = millis();

    // LEER VALORES DEL JOYSTICK DE 0 A 1023
    int lecturaX = analogRead(PIN_JOY_X);
    int lecturaY = analogRead(PIN_JOY_Y);

    // CONVERTIR LECTURA A RANGO DE INCLINACION RESPECTO A 90 GRADOS
    int metaX = map(lecturaX, 0, 1023, CENTRO_X - rangoMaximo, CENTRO_X + rangoMaximo);
    int metaY = map(lecturaY, 0, 1023, CENTRO_Y - rangoMaximo, CENTRO_Y + rangoMaximo);

    // APLICAR FILTRO SUAVIZADO PARA EVITAR MOVIMIENTOS BRUSCOS
    anguloActualX = (anguloActualX * 0.80) + (metaX * 0.20);
    anguloActualY = (anguloActualY * 0.80) + (metaY * 0.20);

    // ESCRIBIR ANGULO SUAVIZADO EN LOS SERVOMOTORES
    servoX.write((int)anguloActualX);
    servoY.write((int)anguloActualY);
  }

  // 2. LEER BOTON DEL JOYSTICK PARA CAMBIAR VELOCIDAD O REINICIAR
  bool botonPresionado = (digitalRead(PIN_BOTON_JOY) == LOW);
  if (botonPresionado && !botonAnterior && millis() - tPresionBoton > 250) {
    tPresionBoton = millis();

    // ALTERNAR ENTRE MODO PRECISO (1: 12 GRADOS) Y MODO RAPIDO (2: 22 GRADOS)
    if (modoSensibilidad == 1) {
      modoSensibilidad = 2;
      rangoMaximo = 22;
    } else {
      modoSensibilidad = 1;
      rangoMaximo = 12;
    }
    mostrarDisplay(modoSensibilidad);

    // AVISAR A A1 PARA REINICIAR EL RELOJ SI ESTABA EN FIN DE JUEGO
    enviarEvento('R');
  }
  botonAnterior = botonPresionado;

  // 3. LEER LOS 3 SENSORES INFRARROJOS (ACTIVAN EN BAJO CUANDO PASA EL BALIN)
  if (millis() - tDebounceSensores >= 150) {
    bool actInicio = digitalRead(PIN_IR_INICIO);
    bool actTrampa = digitalRead(PIN_IR_TRAMPA);
    bool actMeta = digitalRead(PIN_IR_META);

    // DETECTAR PASO POR SENSOR DE INICIO
    if (actInicio == LOW && irInicioAnt == HIGH) {
      enviarEvento('I');
      tDebounceSensores = millis();
    }

    // DETECTAR PASO POR SENSOR DE TRAMPA
    if (actTrampa == LOW && irTrampaAnt == HIGH) {
      enviarEvento('T');
      tDebounceSensores = millis();
    }

    // DETECTAR PASO POR SENSOR DE META
    if (actMeta == LOW && irMetaAnt == HIGH) {
      enviarEvento('M');
      tDebounceSensores = millis();
    }

    irInicioAnt = actInicio;
    irTrampaAnt = actTrampa;
    irMetaAnt = actMeta;
  }
}