// LIBRERIAS PARA I2C Y PANTALLA LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// CONFIGURACION DE LA PANTALLA LCD EN DIRECCION 0X27
LiquidCrystal_I2C lcd(0x27, 16, 2);

// DIRECCION I2C DE ESTE ARDUINO MAESTRO
const byte MI_DIR = 1;

// PIN DEL BUZZER PASIVO
const byte PIN_BUZZER = 3;

// PINES DEL DISPLAY DE 7 SEGMENTOS DE A1
const byte SEG[7] = {8, 7, 6, 5, 4, 10, 9};

// TABLA DE NUMEROS DEL 0 AL 9 PARA CATODO COMUN
const byte DIG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// VARIABLE QUE GUARDA EL COMANDO RECIBIDO POR I2C
volatile char eventoRecibido = 0;

// ESTADOS DEL JUEGO: 0 ESPERA, 1 JUGANDO, 2 GANADO, 3 PERDIDO
byte estadoJuego = 0;

// CONTADOR DE VIDAS Y TIEMPO RESTANTE
int vidas = 3;
int tiempoRestante = 60;

// CONTROL DE TIEMPO CON MILLIS
unsigned long ultimoSegundo = 0;

// FUNCION QUE DIBUJA UN NUMERO EN EL DISPLAY DE 7 SEGMENTOS
void mostrarDisplay(int n) {
  for (int i = 0; i < 7; i++) {
    digitalWrite(SEG[i], bitRead(DIG[n], i));
  }
}

// FUNCION QUE RECIBE LOS EVENTOS DE LOS SENSORES DESDE A2
void alRecibirI2C(int bytes) {
  while (Wire.available()) {
    eventoRecibido = Wire.read();
  }
}

// SONIDO CORTO AL INICIAR O CAMBIAR ESTADO
void sonidoInicio() {
  tone(PIN_BUZZER, 1000, 150);
}

// SONIDO GRAVE AL CAER EN TRAMPA
void sonidoTrampa() {
  tone(PIN_BUZZER, 250, 400);
}

// SONIDO AGUDO AL LLEGAR A LA META
void sonidoVictoria() {
  tone(PIN_BUZZER, 1500, 500);
}

// PANTALLA INICIAL DE ESPERA
void pantallaEspera() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("COLOCA EL BALIN");
  lcd.setCursor(0, 1);
  lcd.print("VIDAS: ");
  lcd.print(vidas);
}

void setup() {
  // INICIAR BUS I2C CON DIRECCION 1
  Wire.begin(MI_DIR);
  Wire.onReceive(alRecibirI2C);

  // CONFIGURAR PINES DEL DISPLAY Y BUZZER COMO SALIDAS
  for (int i = 0; i < 7; i++) {
    pinMode(SEG[i], OUTPUT);
  }
  pinMode(PIN_BUZZER, OUTPUT);

  // INICIAR PANTALLA LCD
  lcd.init();
  lcd.backlight();

  // MOSTRAR VIDAS INICIALES Y MENSAJE DE BIENVENIDA
  mostrarDisplay(vidas);
  pantallaEspera();
}

void loop() {
  // REVISAR SI LLEGO ALGUN COMANDO DESDE EL ARDUINO 2
  if (eventoRecibido != 0) {
    char cmd = eventoRecibido;
    eventoRecibido = 0;

    // COMANDO I: SENSOR DE INICIO DETECTO EL BALIN
    if (cmd == 'I') {
      if (estadoJuego != 1) {
        if (vidas <= 0) {
          vidas = 3;
        }
        tiempoRestante = 60;
        estadoJuego = 1;
        ultimoSegundo = millis();
        mostrarDisplay(vidas);
        sonidoInicio();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("JUGANDO! V:");
        lcd.print(vidas);
      }
    }

    // COMANDO T: SENSOR DE TRAMPA DETECTO CAIDA
    if (cmd == 'T' && estadoJuego == 1) {
      vidas--;
      mostrarDisplay(vidas);
      sonidoTrampa();

      // SI QUEDAN VIDAS SE PAUSA, SI NO TERMINA EL JUEGO
      if (vidas > 0) {
        estadoJuego = 0;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CAISTE EN TRAMPA");
        lcd.setCursor(0, 1);
        lcd.print("PON BALIN EN INI");
      } else {
        estadoJuego = 3;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("FIN DEL JUEGO!");
        lcd.setCursor(0, 1);
        lcd.print("SIN VIDAS");
      }
    }

    // COMANDO M: SENSOR DE META DETECTO VICTORIA
    if (cmd == 'M' && estadoJuego == 1) {
      estadoJuego = 2;
      sonidoVictoria();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("META ALCANZADA!");
      lcd.setCursor(0, 1);
      lcd.print("SOBRARON: ");
      lcd.print(tiempoRestante);
      lcd.print("S");
    }

    // COMANDO R: BOTON DEL JOYSTICK REINICIA PARTIDA
    if (cmd == 'R') {
      vidas = 3;
      tiempoRestante = 60;
      estadoJuego = 0;
      mostrarDisplay(vidas);
      sonidoInicio();
      pantallaEspera();
    }
  }

  // DESCONTAR CRONOMETRO CADA 1000 MILISEGUNDOS SI ESTA JUGANDO
  if (estadoJuego == 1 && millis() - ultimoSegundo >= 1000) {
    ultimoSegundo = millis();
    tiempoRestante--;

    // ACTUALIZAR SEGUNDOS EN LA SEGUNDA LINEA DE LA LCD
    lcd.setCursor(0, 1);
    lcd.print("TIEMPO: ");
    if (tiempoRestante < 10) {
      lcd.print("0");
    }
    lcd.print(tiempoRestante);
    lcd.print(" SEG  ");

    // PITIDO DE ALERTA EN LOS ULTIMOS 5 SEGUNDOS
    if (tiempoRestante <= 5 && tiempoRestante > 0) {
      tone(PIN_BUZZER, 800, 80);
    }

    // SI EL TIEMPO LLEGA A CERO PIERDE LA PARTIDA
    if (tiempoRestante <= 0) {
      estadoJuego = 3;
      vidas = 0;
      mostrarDisplay(0);
      sonidoTrampa();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TIEMPO AGOTADO!");
      lcd.setCursor(0, 1);
      lcd.print("GAME OVER");
    }
  }
}