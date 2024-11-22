
#define DEBUG 1

// Uno = A4 (SDA), A5 (SCL)
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

#include "MAX6675.h"
#include <DallasTemperature.h>

byte temp[8] = {B01110, B01010, B01110, B00000, B00000, B00000, B00000, B00000};

// MAX6675
const int dataPin = 8;   // SO
const int clockPin = 6;  // SCK
const int selectPin = 7; // CS
const int DS = 5;        // DS18B20
const int boton = 3;     // BUTTON

OneWire oneWireObjeto(DS);
DallasTemperature sensorDS18B20(&oneWireObjeto);
MAX6675 thermoCouple(selectPin, dataPin, clockPin);

byte modo = 0;
float calibAmp = 43.3; // SCT-50A
const float calibBat = 0.03;
const float batMax = 4.10; // Depende de la calidad o el estado de la bateria.
const float batMin = 3.3;  // Depende del punto de corte del modulo y calidad de la bateria.

//---------------------------
void setup()
{
  #if DEBUG
    Serial.begin(9600);
  #endif
  pinMode(boton, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.createChar(0, temp);

  SPI.begin();
  thermoCouple.begin();
  thermoCouple.setSPIspeed(4000000);
  sensorDS18B20.begin();
}

void loop()
{
  static unsigned long tiempAnt = 0;

  if (digitalRead(boton))
  {
    modo = modo + 1;
    modo = (modo > 3) ? 0 : modo;
    delay(100);

    while (digitalRead(boton))
    {
      delay(100);
    }
    lcd.clear();
    tiempAnt = 0;
  }

  if (millis() - tiempAnt > 3000)
  {
    switch (modo)
    {
    case 0:
      bateria();
      break;

    case 1: // Sensor Ambiente
      DSTemp();
      break;

    case 2: // Sensor Termocupla
      lcd.clear();
      MAXTemp();
      break;

    case 3: // Sensor Amperimetro
      SCT50A();
      break;
    }
    tiempAnt = millis();
  }
}

void bateria()
{
  float v = voltaje();
  int p = porcentaje(v);
  String txt = "Bat:" + String(p) + "%   " + String(v) + "V"; // Bat: 000% 0.0V
  #if DEBUG
    Serial.println(v);
  #endif

  lcd.setCursor(0, 0);
  lcd.print("MONITOR TEMP. AC");
  lcd.setCursor(0, 1);
  lcd.print(txt);
}

void DSTemp()
{
  if (sensorDS18B20.getDeviceCount() != 0)
  {
    sensorDS18B20.requestTemperatures();

    float temp1 = sensorDS18B20.getTempCByIndex(0);
    temp1 = temp1 > 0.0 ? temp1 : 0.0;
    lcd.setCursor(0, 0);
    lcd.print("AMBIENT:");

    lcd.setCursor(10, 0);
    lcd.print(temp1, 1);
    lcd.write(byte(0));
    lcd.print("C");

    float temp2 = sensorDS18B20.getTempCByIndex(1);
    temp2 = temp2 > -50.0 ? temp2 : 0.0;
    lcd.setCursor(0, 1);
    lcd.print("LIQUIDO:");
    temp2 < 0.0 ? lcd.setCursor(8, 1) : lcd.setCursor(10, 1);

    lcd.print(temp2, 1);
    lcd.write(byte(0));
    lcd.print("C");

  #if DEBUG
      Serial.print("Ambiente: ");
      Serial.print(temp1);
      Serial.print("°C");
      Serial.print(" - Liquido: ");
      Serial.print(temp2);
      Serial.println("°C");
  #endif
  }
  else
  {
    lcd.setCursor(1, 0);
    lcd.print("AMBIENT. LIQUID.");
    lcd.setCursor(6, 1);
    lcd.print("Error");

    Serial.print("AMBIENTE Y SUMERGIBLE:");
    Serial.println("Error");
  }
}

void MAXTemp()
{
  int status = thermoCouple.read();
  float temp = thermoCouple.getTemperature();
  String dato;
  // thermoCouple.setOffset(-10.0);

  lcd.setCursor(3, 0);
  lcd.print("TERMOCUPLA");
  lcd.setCursor(4, 1);
  #if DEBUG
    Serial.print("TERMOCUPLA: ");
  #endif

  if (status == 0)
  {
    lcd.print(temp);
    lcd.write(byte(0));
    lcd.print("C");
  #if DEBUG
      Serial.print(temp);
      Serial.println("C");
  #endif
  }
  else if (status == 4)
  {
    lcd.setCursor(6, 1);
    lcd.print("Error");
  #if DEBUG
      Serial.println("Error");
  #endif
  }
  else if (status == 129)
  {
    lcd.setCursor(2, 1);
    lcd.print("Sin comunic.");
  #if DEBUG
      Serial.println("Sin comunic.");
  #endif
  }
}

void SCT50A()
{
  // float I = senal_escalada(1);
  float Irms = get_corriente(calibAmp);
  float esc = get_corriente(1);

  lcd.setCursor(2, 0);
  lcd.print("AMPERIMETRO");
  lcd.setCursor(0, 1);

  if (Irms < 50)
  {
    lcd.print("AC:");
    lcd.print(Irms);
    lcd.print("A");

    // lcd.setCursor(12, 1);
    // lcd.print(esc);
    lcd.setCursor(10, 1);
    lcd.print(Irms * 229);
    lcd.print("W");
  }
  else
  {
    lcd.print("AC: Error");
  }

#if DEBUG
  Serial.print("AMPERIMETRO: ");
  Serial.print(Irms);
  Serial.print("A");
#endif
}

//---------------------------
float get_corriente(float calib)
{
  float corriente = 0;
  float sumatoria = 0;
  float Irms = 0;
  long tiempo = millis();
  int N = 0;

  while (millis() - tiempo < 500)
  { // Duracion 0.5 seg (30 ciclos de 60Hz)
    corriente = senal_escalada(calib);
    sumatoria = sumatoria + sq(corriente);
    N = N + 1;
    delay(1);
  }
  Irms = sqrt(sumatoria / N);
  return (Irms);
}

float senal_escalada(float calib)
{
  float senal = voltaje_promedio(10) * (5.0 / 1023.0) - 2.5;
  float senal_scale = senal * calib; // Poner en 1 para obtener el valor escalado ese dato se debe usar para dividir al resultado del amperimetro.
  return (senal_scale);
}

int voltaje_promedio(int n)
{
  long suma = 0;
  for (int i = 0; i < n; i++)
  {
    suma = suma + analogRead(A0);
  }
  return (suma / n);
}

//---------------------------
float voltaje()
{
  int pinValor = 0;

  for (int i = 0; i < 15; i++)
  {
    pinValor += analogRead(A3);
    ;
  }
  pinValor = pinValor / 15;

  float vol = ((((pinValor * 5.0) / 1023) * 2.0) + calibBat);
  return vol;
}

int porcentaje(float vol)
{
  int bat = ((vol - batMin) / (batMax - batMin)) * 100;

  if (bat >= 100)
  {
    bat = 100;
  }
  if (bat <= 0)
  {
    bat = 1;
  }
  return bat;
}
