// LIBRERIAS PARA BUS I2C, PANTALLA LCD Y PANTALLA OLED
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OBJETO DE PANTALLA LCD 16X2 EN DIRECCION 0X27
LiquidCrystal_I2C lcd(0x27, 16, 2);

// OBJETO DE PANTALLA OLED 128X64 EN DIRECCION 0X3C
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// DIRECCION I2C DE ESTE ARDUINO MAESTRO
const byte MI_DIR = 1;

// PIN DEL BOTON FISICO DE REINICIO Y PIN DEL BUZZER
const byte PIN_BOTON_RESET = 2;
const byte PIN_BUZZER = 3;

// PINES DEL DISPLAY DE 7 SEGMENTOS DE VIDAS
const byte SEG[7] = {8, 7, 6, 5, 4, 10, 9};

// TABLA DE DIGITOS PARA DISPLAY DE CATODO COMUN
const byte DIG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// TIEMPO TOTAL POR PARTIDA EN SEGUNDOS
const int TIEMPO_MAXIMO = 90;

// COMANDO RECIBIDO DESDE EL ARDUINO ESCLAVO
volatile char eventoRecibido = 0;

// ESTADOS: 0 NUEVA, 1 JUGANDO, 2 EN TRAMPA, 3 GANADO, 4 PERDIDO
byte estadoJuego = 0;

// VARIABLES DE PARTIDA ACTUAL
int vidas = 3;
int tiempoRestante = TIEMPO_MAXIMO;
unsigned long ultimoSegundo = 0;

// VARIABLES DE RECORD HISTORICO PARA LA PANTALLA OLED
bool existeRecord = false;
int mejorTiempoUsado = 999;
int mejorVidas = 0;
int victoriasTotales = 0;
int partidasJugadas = 0;

// VARIABLES PARA EL ANTIRREBOTE DEL BOTON DE REINICIO
unsigned long tBotonReset = 0;
bool resetAnterior = false;

// FUNCION QUE MUESTRA LAS VIDAS EN EL DISPLAY DE 7 SEGMENTOS
void mostrarDisplay(int n) {
  for (int i = 0; i < 7; i++) {
    digitalWrite(SEG[i], bitRead(DIG[n], i));
  }
}

// INTERRUPCION QUE GUARDA EL CARACTER ENVIADO POR EL ESCLAVO
void alRecibirI2C(int bytes) {
  while (Wire.available()) {
    eventoRecibido = Wire.read();
  }
}

// TONO CORTO DE INICIO O REANUDACION
void sonidoInicio() {
  tone(PIN_BUZZER, 1000, 150);
}

// TONO GRAVE DE CAIDA EN TRAMPA
void sonidoTrampa() {
  tone(PIN_BUZZER, 250, 400);
}

// TONO DE VICTORIA NORMAL
void sonidoVictoria() {
  tone(PIN_BUZZER, 1500, 500);
}

// TONO ESPECIAL CUANDO SE ROMPE UN RECORD
void sonidoNuevoRecord() {
  tone(PIN_BUZZER, 1800, 700);
}

// FUNCION QUE ACTUALIZA EL TABLERO DE RECORDS EN LA OLED
void actualizarOled(bool fueNuevoRecord) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // TITULO SUPERIOR EN LA PANTALLA OLED
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  if (fueNuevoRecord) {
    oled.println(F("!! NUEVO RECORD !!"));
  } else {
    oled.println(F("== RECORD ACTUAL =="));
  }

  // MOSTRAR MEJOR TIEMPO Y VIDAS SI YA EXISTE UN RECORD
  if (!existeRecord) {
    oled.setCursor(0, 18);
    oled.println(F("SIN RECORD REGISTRADO"));
    oled.setCursor(0, 32);
    oled.println(F("LLEGA A LA META!"));
  } else {
    oled.setTextSize(2);
    oled.setCursor(0, 14);
    oled.print(mejorTiempoUsado);
    oled.print(F("S "));
    oled.print(mejorVidas);
    oled.println(F("V"));

    oled.setTextSize(1);
    oled.setCursor(0, 36);
    oled.print(F("SOBRARON: "));
    oled.print(TIEMPO_MAXIMO - mejorTiempoUsado);
    oled.println(F(" SEG"));
  }

  // ESTADISTICAS DE LA SESION EN LA PARTE INFERIOR
  oled.setTextSize(1);
  oled.setCursor(0, 52);
  oled.print(F("JUEGOS:"));
  oled.print(partidasJugadas);
  oled.print(F("  GANADOS:"));
  oled.print(victoriasTotales);
  oled.display();
}

// PANTALLA LCD EN ESTADO DE ESPERA INICIAL
void pantallaEspera() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("COLOCA EL BALIN"));
  lcd.setCursor(0, 1);
  lcd.print(F("VIDAS: "));
  lcd.print(vidas);
}

// PANTALLA LCD DURANTE EL JUEGO ACTIVO
void pantallaJugando() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("JUGANDO! V:"));
  lcd.print(vidas);
  lcd.setCursor(0, 1);
  lcd.print(F("TIEMPO: "));
  lcd.print(tiempoRestante);
  lcd.print(F(" SEG"));
}

void setup() {
  // INICIAR COMUNICACION I2C COMO MAESTRO DIRECCION 1
  Wire.begin(MI_DIR);
  Wire.onReceive(alRecibirI2C);

  // CONFIGURAR PINES DE SALIDA Y BOTON DE RESET
  for (int i = 0; i < 7; i++) {
    pinMode(SEG[i], OUTPUT);
  }
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BOTON_RESET, INPUT_PULLUP);

  // INICIAR PANTALLA LCD 16X2
  lcd.init();
  lcd.backlight();

  // INICIAR PANTALLA OLED 128X64 EN DIRECCION 0X3C
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // MOSTRAR VALORES INICIALES EN LAS TRES PANTALLAS
  mostrarDisplay(vidas);
  pantallaEspera();
  actualizarOled(false);
}

void loop() {
  // 1. REVISAR BOTON FISICO DE REINICIO EN PIN D2
  bool resetPresionado = (digitalRead(PIN_BOTON_RESET) == LOW);
  if (resetPresionado && !resetAnterior && millis() - tBotonReset > 250) {
    tBotonReset = millis();
    vidas = 3;
    tiempoRestante = TIEMPO_MAXIMO;
    estadoJuego = 0;
    mostrarDisplay(vidas);
    sonidoInicio();
    pantallaEspera();
    actualizarOled(false);
  }
  resetAnterior = resetPresionado;

  // 2. PROCESAR EVENTOS DE LOS SENSORES ENVIADOS POR EL ESCLAVO
  if (eventoRecibido != 0) {
    char cmd = eventoRecibido;
    eventoRecibido = 0;

    // EVENTO I: BALIN COLOCADO EN EL SENSOR DE INICIO
    if (cmd == 'I') {
      // SI VENIA DE CAER EN UNA TRAMPA, SOLO REANUDA SIN REINICIAR EL TIEMPO
      if (estadoJuego == 2) {
        estadoJuego = 1;
        sonidoInicio();
        pantallaJugando();
      }
      // SI ES PARTIDA NUEVA O YA HABIA GANADO O PERDIDO, INICIA DESDE CERO
      else if (estadoJuego == 0 || estadoJuego == 3 || estadoJuego == 4) {
        vidas = 3;
        tiempoRestante = TIEMPO_MAXIMO;
        partidasJugadas++;
        estadoJuego = 1;
        ultimoSegundo = millis();
        mostrarDisplay(vidas);
        sonidoInicio();
        pantallaJugando();
        actualizarOled(false);
      }
    }

    // EVENTO T: BALIN CAYO EN SENSOR DE TRAMPA
    if (cmd == 'T' && estadoJuego == 1) {
      vidas--;
      mostrarDisplay(vidas);
      sonidoTrampa();

      // SI LE QUEDAN VIDAS PASA A ESTADO 2 PERO EL TIEMPO SIGUE CORRIENDO
      if (vidas > 0) {
        estadoJuego = 2;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("CAISTE EN TRAMPA"));
        lcd.setCursor(0, 1);
        lcd.print(F("PON BALIN EN INI"));
      } else {
        // SI SE QUEDO SIN VIDAS TERMINA LA PARTIDA EN ESTADO 4
        estadoJuego = 4;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("FIN DEL JUEGO!"));
        lcd.setCursor(0, 1);
        lcd.print(F("SIN VIDAS"));
      }
    }

    // EVENTO M: BALIN LLEGO AL SENSOR DE META
    if (cmd == 'M' && (estadoJuego == 1 || estadoJuego == 2)) {
      estadoJuego = 3;
      victoriasTotales++;

      // CALCULAR SEGUNDOS EMPLEADOS EN SUPERAR EL LABERINTO
      int tiempoUsado = TIEMPO_MAXIMO - tiempoRestante;
      bool nuevoRecord = false;

      // EVALUAR SI SUPERA EL RECORD ANTERIOR EN TIEMPO O EN VIDAS
      if (!existeRecord || tiempoUsado < mejorTiempoUsado || (tiempoUsado == mejorTiempoUsado && vidas > mejorVidas)) {
        existeRecord = true;
        mejorTiempoUsado = tiempoUsado;
        mejorVidas = vidas;
        nuevoRecord = true;
      }

      // MOSTRAR MENSAJE DE VICTORIA O DE NUEVO RECORD EN LCD Y OLED
      lcd.clear();
      if (nuevoRecord) {
        sonidoNuevoRecord();
        lcd.setCursor(0, 0);
        lcd.print(F("!NUEVO RECORD!"));
        lcd.setCursor(0, 1);
        lcd.print(F("TIEMPO: "));
        lcd.print(tiempoUsado);
        lcd.print(F("S V:"));
        lcd.print(vidas);
      } else {
        sonidoVictoria();
        lcd.setCursor(0, 0);
        lcd.print(F("META ALCANZADA!"));
        lcd.setCursor(0, 1);
        lcd.print(F("SOBRO:"));
        lcd.print(tiempoRestante);
        lcd.print(F("S V:"));
        lcd.print(vidas);
      }
      actualizarOled(nuevoRecord);
    }
  }

  // 3. CRONOMETRO QUE CORRE TANTO JUGANDO (1) COMO RECUPERANDO DE TRAMPA (2)
  if ((estadoJuego == 1 || estadoJuego == 2) && millis() - ultimoSegundo >= 1000) {
    ultimoSegundo = millis();
    tiempoRestante--;

    // SI ESTA EN PANTALLA DE JUEGO ACTUALIZA LOS SEGUNDOS EN LA LCD
    if (estadoJuego == 1) {
      lcd.setCursor(0, 1);
      lcd.print(F("TIEMPO: "));
      if (tiempoRestante < 10) {
        lcd.print(F("0"));
      }
      lcd.print(tiempoRestante);
      lcd.print(F(" SEG  "));
    }

    // PITIDO DE ALERTA EN LOS ULTIMOS 5 SEGUNDOS
    if (tiempoRestante <= 5 && tiempoRestante > 0) {
      tone(PIN_BUZZER, 800, 80);
    }

    // SI EL RELOJ LLEGA A CERO PIERDE AUTOMATICAMENTE
    if (tiempoRestante <= 0) {
      estadoJuego = 4;
      vidas = 0;
      mostrarDisplay(0);
      sonidoTrampa();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("TIEMPO AGOTADO!"));
      lcd.setCursor(0, 1);
      lcd.print(F("FIN DEL JUEGO"));
    }
  }
}