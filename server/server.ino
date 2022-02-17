#include <SPI.h>
#include <Ethernet.h>
#include "DHT.h"
#include <LiquidCrystal.h>
#include <IRremote.h>


//Константы
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal lcd(8, 9, 4, 5, 6, 7 );

byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // Мак адрес
byte ip[] = { 
  192, 168, 0, 2 }; // IP адрес (изменить в title)
EthernetServer server(80);

//Переменные
String readString = String(30);  // Парсим запрос к серверу (ардуино)
long unsigned int lowIn;         // Время, в которое был принят сигнал отсутствия движения(LOW)    
long unsigned int pause = 5000;  // Пауза, после которой движение считается оконченным
boolean lockLow = true;          // Флаг. false = значит движение уже обнаружено, true - уже известно, что движения нет
boolean takeLowTime;             // Флаг. Сигнализирует о необходимости запомнить время начала отсутствия движения
boolean PIR = false;             // Вкл./Выкл. датчик движения

IRrecv irrecv(19);               // Указываем пин, к которому подключен приемник
decode_results results;


void setup()
{
  //Пины
  pinMode(3, OUTPUT);            // Реле-1
  pinMode(14, OUTPUT);           // Подсветка LCD 
  pinMode(18, INPUT);            // Датчик движения

  //Значения
  digitalWrite(3, HIGH);         // Отключаем реле-1

  //Старт
  Serial.begin(9600);
  Serial.println("FREE RAM: ");
  Serial.println(freeRam());
  irrecv.enableIRIn();
  Ethernet.begin(mac, ip);
  server.begin();
  //Serial.println("Server is run");
  dht.begin();
  //Serial.println("DHT is run");
  //Serial.print("Calibrating PIR sensor");
  lcd.begin(16, 2);
  display("Calibrating PIR","sensor - 10 sec.");
  for(int i = 0; i < 10; i++)
  {
    //Serial.print(".");
    delay(1000);
  }
  //Serial.println(" done");
  //Serial.println("Sensor active");
  digitalWrite(14, LOW);

  //Приветствие LCD
  display("Arduino Server","Status: is RUN");

}

void loop()
{
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  EthernetClient client = server.available();
  if (client)
  {
    // Проверяем подключен ли клиент к серверу
    while (client.connected())
    {
      char c = client.read();
      if (readString.length() < 30) {
        readString += c;
      }
      if (c == '\n') { 
        if(readString.indexOf("pir=1") > 0) {
          PIR = true;
          //Serial.println("Motion sensor ON");
          display("MOTION SENSOR","Status: ON");
        }
        if(readString.indexOf("pir=0") > 0) {
          PIR = false;
          lockLow = true; 
          digitalWrite(3, HIGH);
          //Serial.println("Motion sensor OFF");
          display("MOTION SENSOR","Status: OFF");
        }

        // Выводим HTML страницу, на которой пользователь может включить или выключить нужные ему реле
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();
        client.println("<!DOCTYPE html>");
        client.println("<html lang=\"ru\">");
        client.println("<head>");
        client.println("<meta charset=\"UTF-8\">");
        client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">");
        client.println("<link rel=\"stylesheet\" href=\"http://home.telwik.ru/style_server_ard/style.css\">");
        client.println("<title>Arduino Home Server</title>");
        client.println("</head>");
        client.println("<body>");
        client.println("<div class=\"wapper\">");
        client.println("<div class=\"header\">");
        client.println("<h1>Arduino Home Server</h1>");
        client.println("<h2>Технологии в каждый дом</h2>");
        client.println("</div>");
        client.println("<div class=\"content\">");
        client.println("<div class=\"title\">Температура</div>");
        client.println("<div class=\"text\">Состояние: ");
        client.println(t);
        client.println(" *С </div>");
        client.println("</div>");
        client.println("<div class=\"content\">");
        client.println("<div class=\"title\">Влажность</div>");
        client.println("<div class=\"text\">Состояние: ");
        client.println(h);
        client.println(" %\t </div>");
        client.println("</div>");
        client.println("<div class=\"content\">");
        client.println("<div class=\"title\">Датчик движения</div>");
        client.println("<div class=\"text\">Состояние: ");
        if (lockLow == false) {
          client.println("движение обнаружено!");
        } 
        else {
          client.println("движения нет.");
        }
        client.println("<br/>");
        if (PIR==true) {    
          client.println("<a href=\"?pir=0\" class=\"submit\">Выключить</a>");
        } 
        else {      
          client.println("<a href=\"?pir=1\" class=\"submit\">Включить</a>");
        }
        client.println("</div>");
        client.println("</div>");
        client.println("</div>");
        client.println("</body>");
        client.println("</html>");
        readString="";
        client.stop(); 
      }
    }
  } 
  else {
    if (PIR==true) {
      //Если обнаружено движение
      if(digitalRead(18) == HIGH) {
        //Если еще не вывели информацию об обнаружении
        if(lockLow) {
          lockLow = false;     
          //Serial.println("Motion detected");
          digitalWrite(3, LOW);
          display("MOTION FINISH","RELE-1 is ON");
        }        
        takeLowTime = true;
      }

      //Еcли движения нет
      if(digitalRead(18) == LOW) {      
        //Если время окончания движения еще не записано
        if(takeLowTime) {
          lowIn = millis();          //Сохраним время окончания движения
          takeLowTime = false;       //Изменим значения флага, чтобы больше не брать время, пока не будет нового движения
        }
        //Если время без движение превышает паузу => движение окончено
        if(!lockLow && millis() - lowIn > pause) { 
          //Изменяем значение флага, чтобы эта часть кода исполнилась лишь раз, до нового движения
          lockLow = true;               
          //Serial.println("Motion finished");
          digitalWrite(3, HIGH);
          //Serial.println("PIN3 LOW");
          display("MOTION FINISH","RELE-1 is OFF");
        }
      }
    }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Temperature:");
    lcd.print(t);
    lcd.setCursor(0,2);
    lcd.print("Humidity:  ");
    lcd.print(h);
    if (irrecv.decode(&results)) {
      if (results.value == 16712445) { // ОК
        digitalWrite(14, HIGH);
        delay(1500);
        digitalWrite(14, LOW);
      } 
      else if (results.value == 16728765) { // *
        PIR = true;
        display("MOTION SENSOR","Status: ON");
      }
      else if (results.value == 16732845) {
        PIR = false;
        lockLow = true; 
        digitalWrite(3, HIGH);
        display("MOTION SENSOR","Status: OFF");
      }
      irrecv.resume(); 
    }  
  }
}
void display(String title1, String title2) {
  lcd.clear();
  lcd.setCursor(0,0);
  digitalWrite(14, HIGH);
  lcd.print(title1);
  lcd.setCursor(0,2);
  lcd.print(title2);
  delay(1500);
  digitalWrite(14, LOW);
}
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}







