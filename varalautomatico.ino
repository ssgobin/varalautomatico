#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Stepper.h>

// Wifi network station credentials
#define WIFI_SSID "ssiddasuaredewifi"
#define WIFI_PASSWORD "senhadasuaredewifi"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "chavedoseubotdotelegram"

const char* accuweatherAPIKey = "chavedasuaapidoaccuweather";
String lastWeatherText = "";

const unsigned long BOT_MTBS = 1000; // mean time between scan messages


X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done

bool loud = true;
bool chovendo = false;
bool status = false;
const int interval = 10;
String city, lat, lon, codigoLocal, textoClima, textoTemp, textoIcon;
String verificaClima = "1";
const int pinoSensor = D7; //PINO DIGITAL UTILIZADO PELO SENSOR
int chuva;
int Porcento = 0;
const int pinoAberto = D5;
const int pinoChave2 = D6;
bool fechado;

bool manual = true;

int stepsPerRevolution = 64;  // change this to fit the number of steps per revolution

// ULN2003 Motor Driver Pins
#define IN1 D4
#define IN2 D3
#define IN3 D2
#define IN4 D1

Stepper motor(stepsPerRevolution, IN1, IN3, IN2, IN4);

void getAndSendWeather(){

  if (WiFi.status() == WL_CONNECTED) {
    Serial.begin(115200); 
    HTTPClient http;
    WiFiClient client;
    http.begin(client, "http://www.geoplugin.net/json.gp");
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      lat = doc["geoplugin_latitude"].as<String>();
      lon = doc["geoplugin_longitude"].as<String>();
    } else {
      Serial.println("Não foi possível obter a localização UM.");
    }
    http.end();

    if (!lat.isEmpty() && !lon.isEmpty()) {
      String locationAPIUrl = "http://dataservice.accuweather.com/locations/v1/cities/geoposition/search?apikey=" + String(accuweatherAPIKey) + "&q=" + lat + "%2C" + lon + "&language=pt-br";
      http.begin(client, locationAPIUrl);
      httpCode = http.GET();
      if (httpCode == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);

        String cidade = doc["LocalizedName"];
        String estado = doc["AdministrativeArea"]["LocalizedName"];
        String pais = doc["Country"]["LocalizedName"];

        cidade += "," + estado + "." + pais;
        codigoLocal = doc["Key"].as<String>();

        //Serial.println(codigoLocal);
        //Serial.print("Obtendo clima do local: ");
        //Serial.println(cidade);
      } else {
        //Serial.println("Não foi possível obter a localização DOIS.");
      }
      http.end();
    }

    if (!codigoLocal.isEmpty()) {
      String currentConditionsAPIUrl = "http://dataservice.accuweather.com/currentconditions/v1/" + codigoLocal + "?apikey=" + String(accuweatherAPIKey) + "&language=pt-br";
      http.begin(client, currentConditionsAPIUrl);
      httpCode = http.GET();
      if (httpCode == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);

        textoClima = doc[0]["WeatherText"].as<String>();
        int iconClima = doc[0]["WeatherIcon"];
        double temperatura = doc[0]["Temperature"]["Metric"]["Value"];

        //Serial.print("Clima no momento: ");
        //Serial.println(textoClima);
        //Serial.print("Icone: ");
        //Serial.println(iconClima);
        textoTemp = String(temperatura);
        textoIcon = String(iconClima);
      } else {
        //Serial.println("Não foi possível obter a localização.");
      }
      http.end();
    }

  }
}

void loop()
{
  Serial.begin(115200); 
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }

  if (!loud && verificaClima != textoIcon) {
    verificaClima = textoIcon;
    getAndSendWeather();
  }

  chuva = analogRead(pinoSensor);

  Porcento = map(chuva, 1023, 0, 0, 100);

  Serial.begin(115200);

  motor.setSpeed(200);

  if (digitalRead(pinoAberto) == LOW && Porcento >= 20){
      while (digitalRead(pinoChave2) == HIGH) {
        motor.step(-1);
        delay(10);
      }
    chovendo = true;
  }

  if (digitalRead(pinoChave2) == LOW && Porcento < 20 && manual == false){
    while(digitalRead(pinoAberto) == HIGH){
      motor.step(1);
      delay(10);
    }
    chovendo = false;
  }

  if (digitalRead(pinoChave2) == HIGH && digitalRead(pinoAberto) == HIGH) {
      while (digitalRead(pinoChave2) == HIGH) {
      motor.step(-1);
      delay(10);
    }
  }
}


void handleNewMessages(int numNewMessages)
{
  Serial.begin(115200); 
  String msg;

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";


    if (text == "/start") {
        msg = "Bem - vindo, " + from_name + ".\n";
        msg += "Segue lista de comandos:\n\n";
        msg += "/ligar: Liga o acionamento com base na API\n";
        msg += "/desligar : Desliga o acionamento com base na API\n";
        msg += "/statusapi : Mostra a situação do acionamento da API (ligado ou desligado)\n";
        msg += "/abrir : Abre o varal\n";
        msg += "/fechar : Fecha o varal\n";
        msg += "/status : Mostra o status do varal (aberto ou fechado)\n";
        msg += "/manualon : Liga a função de abrir o varal manualmente\n";
        msg += "/manualoff : Desliga a função de abrir o varal manualmente\n";
        msg += "/statusmanual : Mostra o status da abertura manual (ligado ou desligado)\n";
        msg += "/opcoes : Mostra as opções\n";
        String keyboardJson = "[[\"/ligar\", \"/desligar\"], [\"/statusapi\", \"/abrir\"], [\"/fechar\", \"/status\"], [\"/manualon\", \"/manualoff\"], [\"/statusmanual\"]]";
        //bot.sendMessage(chat_id, msg, "Markdown");
        bot.sendMessageWithReplyKeyboard(chat_id, msg, "", keyboardJson, true);
    }


    else if (text == "/ligar") {

      if (status) {
            msg = "";
            msg += "\n Acionamento já está ligado"; 
            bot.sendMessage(chat_id, msg, "");
         } else {


          
            msg = "";
            msg += "\n Acionamento com base na previsão ligado!";
            bot.sendMessage(chat_id, msg, "");
          
            loud = false;
            status = true;
                    
            if(textoIcon == "7"){
              Serial.println("Chuva! Recolhendo varal!");
            } else {
              Serial.println("Sol! Voltando varal!");
            }
  }
}


    else if (text == "/desligar") {

      if (!status) {
            msg = "";
            msg += "\n Acionamento já está desligado"; 
            bot.sendMessage(chat_id, msg, "");
        } else {
            loud = true;
            status = false;
            msg = "";
            msg += "\n Acionamento com base na previsão desligado! "; 
            bot.sendMessage(chat_id, msg, "");
        }
    }

    else if (text == "/statusapi") {

      if (status)
      {
        msg = "";
          msg += "\n Acionamento com base na previsão está ligado!"; 
          bot.sendMessage(chat_id, msg, "");
      }
      else
      {
        msg = "";
        msg += "\n Acionamento com base na previsão está desligado!"; 
          bot.sendMessage(chat_id, msg, "");
      }
    }

      else if (text == "/status") {

      if (fechado)
      {
        msg = "";
          msg += "\n Varal está fechado!"; 
          bot.sendMessage(chat_id, msg, "");
      }
      else
      {
        msg = "";
        msg += "\n Varal está aberto!"; 
          bot.sendMessage(chat_id, msg, "");
      }
    }

    else if (text == "/abrir") {

      motor.setSpeed(200);

      if (digitalRead(pinoAberto) == LOW) {
        motor.setSpeed(1);
        msg = "";
        msg += "\n Varal já está aberto"; 
        bot.sendMessage(chat_id, msg, "");
      } else if (Porcento >= 20) {
        motor.setSpeed(1);
        msg = "";
        msg += "\n Chovendo! Impossível abrir o varal!"; 
        bot.sendMessage(chat_id, msg, "");
      } else if (digitalRead(pinoAberto) == HIGH){
          while (digitalRead(pinoAberto) == HIGH) {
            motor.step(1);
            delay(10);
        }
        msg = "";
        msg += "\n Varal aberto ✅"; 
        bot.sendMessage(chat_id, msg, "");
        fechado = false;
      }
    }

    else if (text == "/fechar") {

      motor.setSpeed(200);

      if (digitalRead(pinoChave2) == LOW) {
        motor.setSpeed(1);
        msg = "";
        msg += "\n Varal já está fechado"; 
        bot.sendMessage(chat_id, msg, "");
      } 
      else if (digitalRead(pinoAberto) == LOW){
        while (digitalRead(pinoChave2) == HIGH) {
          motor.step(-1);
          delay(10);
        }
          msg = "";
          msg += "\n Varal fechado ✅"; 
          bot.sendMessage(chat_id, msg, "");
          fechado = true;
      }
    }

    else if (text == "/manualon") {
      if (manual) {
        manual = true;
        msg = "";
        msg += "\n Abrir varal manualmente já está ligado!";
        bot.sendMessage(chat_id, msg, "");
      } else {
        manual = true;
        msg = "";
        msg += "\n Abrir varal manualmente ligado!";
        bot.sendMessage(chat_id, msg, "");
      }
    }

    else if (text == "/manualoff") {
      if (!manual) {
      msg = "";
      msg += "\n Abrir varal manualmente já está desligado!";
      bot.sendMessage(chat_id, msg, "");
      } else {
      manual = false;
      msg = "";
      msg += "\n Abrir varal manualmente desligado!";
      bot.sendMessage(chat_id, msg, "");
      }
    }
    else if (text == "/statusmanual") {

      if (manual)
      {
        msg = "";
          msg += "\n Abrir varal manualmente está ligado!"; 
          bot.sendMessage(chat_id, msg, "");
      }
      else
      {
        msg = "";
        msg += "\n Abrir varal manualmente está desligado!"; 
          bot.sendMessage(chat_id, msg, "");
      }
    }
  
}
}



void setup()
{
  Serial.begin(115200); 

  // attempt to connect to Wifi network:
  configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP
  secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Check NTP/Time, usually it is instantaneous and you can delete the code below.
  time_t now = time(nullptr);

  pinMode(pinoAberto, INPUT_PULLUP);
  pinMode(pinoChave2, INPUT_PULLUP);
  motor.setSpeed(800);
  // set target position
}
