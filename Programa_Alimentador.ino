//Inclusão de bibliotecas//

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <time.h>
#include <sys/time.h>
#include <WiFi.h>
#include <NTPClient.h> 
#include <EEPROM.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

//Definição de parâmetros de controle//

#define PinWPS 4
#define rele 17
#define qtparam 4 
#define maxval  5 //Definição do número máximo de períodos de acionamento (pode ser alterado, se necessário, variando entre 1 e 9)
#define maxpin  1 
#define EEPROM_SIZE 100

TFT_eSPI tft = TFT_eSPI();

bool pinos_out_status[maxpin] = {false}; 
bool pinos_out_manual[maxpin] = {false}; 
const byte pinos_out[maxpin] = {17}; 
byte saida = 0;
byte tempos_acionamento[maxpin]; 

//Configurações de Wi-fi//

const char* ssid     = "SSID (Nome da rede Wi-Fi)";
const char* password = "Senha da rede Wi-Fi";
WiFiServer server(80);

//Configurações do relógio//

WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000);
time_t tt;

String hora;      
String header;
String RelayState = "Desligado";
                
char data_formatada[64]; 

struct tm data;
struct temporizacao {
  byte hora ;
  byte minuto;
  byte dias ;
  byte durar;
};

temporizacao tempos[maxpin][maxval];

//Configuração e Salvamento de dados na memória EEPROM//

void load() 
{
  int i, j;
  
  for (j = 0 ; j < maxpin; j++)
  
    for (i = 0 ; i < maxval; i++) 
    {
      tempos[j][i].hora = EEPROM.read(i * qtparam + 0 + (j *  maxval * qtparam));
      tempos[j][i].minuto = EEPROM.read(i * qtparam + 1 + (j * maxval * qtparam));
      tempos[j][i].durar = EEPROM.read(i * qtparam + 2 + (j * maxval * qtparam));
      tempos[j][i].dias = EEPROM.read(i * qtparam + 3 + (j * maxval * qtparam));
    }

  for (i = 0 ; i < maxpin; i++) 
  {
    tempos_acionamento[i] = EEPROM.read(maxpin * qtparam * maxval + 1 + i);
    if (tempos_acionamento[i] > maxval) tempos_acionamento[i] = 0; // travamento p primeiro start
  }
}

void save() 
{
  int i, j;

  for (j = 0 ; j < maxpin; j++)
  
    for (i = 0 ; i < maxval; i++)
    {
      EEPROM.write(i * qtparam + 0 + (j *  maxval * qtparam), tempos[j][i].hora);
      EEPROM.write(i * qtparam + 1 + (j *  maxval * qtparam), tempos[j][i].minuto);
      EEPROM.write(i * qtparam + 2 + (j *  maxval * qtparam), tempos[j][i].durar);
      EEPROM.write(i * qtparam + 3 + (j *  maxval * qtparam), tempos[j][i].dias);
    }
    
  for (i = 0 ; i < maxpin; i++)
  
    EEPROM.write(maxpin * maxval * qtparam + 1 + i, tempos_acionamento[i]);

  EEPROM.commit(); 

  for (i = 0 ; i < EEPROM_SIZE; i++)
  { 
  } 
}

void setup()
{
  
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(1);
  tft.setTextSize(2);
  
  int i;
  
  EEPROM.begin(EEPROM_SIZE); 
  load(); 
  
  for (i = 0 ; i < maxpin; i++)
  {
    pinMode(pinos_out[i], OUTPUT);  
  }
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
  }

  server.begin(); 

  xTaskCreatePinnedToCore
  (coreTaskZero, "coreTaskZero", 10000, NULL, 1, NULL, 0);         
}

void coreTaskZero( void * pvParameters ) 
{
  timeval tv;
  ntp.begin();           
  ntp.forceUpdate();   
  hora = ntp.getFormattedTime(); 
  tv.tv_sec = ntp.getEpochTime(); 
  settimeofday(&tv, NULL);
 
  int j, i;
  
  while (1) 
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    tt = time(NULL);
    data = *gmtime(&tt);
    strftime(data_formatada, 64, "%d/%m/%Y %H:%M:%S", &data);
    
    int hora = data.tm_hour, minutos = data.tm_min, segundos = data.tm_sec ; 
    int agora = hora * 60  + minutos; 

 //Setor responsável pelo acionamento automático//
    
    bool ligauto = false; 
    
    for (j = 0 ; j < maxpin; j++) 
    { 
      for (i = 0; i < tempos_acionamento[j] ; i++) 
      { 
        int acionamento_inicial = ((tempos[j][i].hora * 60) + tempos[saida][i].minuto); 
        int acionamento_final = ((tempos[j][i].hora * 60) + tempos[saida][i].minuto + tempos[saida][i].durar); 
        
        if ((acionamento_inicial <= agora) && (agora < acionamento_final))
        {
          ligauto = true; 
        }
        
      if ((ligauto) || (pinos_out_manual[j])) 
      {
        digitalWrite (pinos_out[j], HIGH); 
        pinos_out_status[j] = true; 
        RelayState = "Ligado";
      }
      
      else 
      {
        digitalWrite (pinos_out[j], LOW); // liga saida
        pinos_out_status[j] = false;
        RelayState = "Desligado";// atualiza status no array
      }
    }
    
//Setor responsável pelo controle das informações do display//

    //Relógio//
    
    tft.setTextSize(2);
    tft.setCursor(0 , 0);
    tft.fillScreen(TFT_BLACK);

    if(hora<10)
    {
      tft.print("0");
    }
    
    tft.print(hora);
    tft.print(":");

    if(minutos<10)
    {
      tft.print("0");
    }

    tft.print(minutos);
    tft.print(":");
 
    if(segundos<10)
    {
      tft.print("0");
    }

    tft.print(segundos);

    //Estado do motor e IP do servidor Web//
    
    tft.setCursor(30, 60);
    
    if(RelayState == "Ligado")
    {
      tft.setCursor(50, 60);
    }
    
    tft.print("Motor " + RelayState);

    tft.setCursor(0, 127);
    tft.setTextSize(1);
    
    tft.print("IP:");
    tft.print(WiFi.localIP());
  }
 }
}

String tratamento(int indice) 
{
  int hora_t, minutos_t;
  char char_aux[6];
  
  hora_t = tempos[saida][indice].hora;
  minutos_t = tempos[saida][indice].minuto;
  
  sprintf(char_aux, "%02d:%02d", hora_t, minutos_t);
  
  return char_aux;
}

//Manutenção do servidor Web//

void loop() 
{
  String str_aux, str_aux1;
  char char_aux[6];
  int i, pos;
  
  WiFiClient client = server.available(); 
    
  if (client) 
  {                                     
    String currentLine = "";  
    
    while (client.connected()) 
    {            
      if (client.available()) 
      {             
        char c = client.read();            
                     
        header += c;
        
        if (c == '\n') 
        {                   
          if (currentLine.length() == 0) 
          {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            //Controle do botão de acionamento direto//
            
            if (header.indexOf("GET /2/on") >= 0) 
            {
              pinos_out_manual[saida] = true;
              RelayState = "Ligado";
            } 
            
            else if (header.indexOf("GET /2/off") >= 0) 
            {
              pinos_out_manual[saida] = false;
              RelayState = "Desligado";
             }

            //Setor responsável pelo sistema de agendamento de acionamentos//
           
            if (header.indexOf("AGENDA") > 0) 
            {
              for (i = 0; i < maxval ; i++) 
              {
                pos = header.indexOf("tempo" + String(i));
                
                if (pos > 0) 
                {
                  tempos[saida][i].hora = header.substring(pos + 7, pos + 9).toInt();
                  tempos[saida][i].minuto = header.substring(pos + 12, pos + 15).toInt();
                }
                
                pos = header.indexOf("durar" + String(i));
                
                if (pos > 0) 
                {
                  tempos[saida][i].durar = header.substring(pos + 7, pos + header.indexOf("&", pos)).toInt();
                }
              }
          
              save();
            }

            if (header.indexOf("ATUALIZAR") > 0)
            {
              for (i = 0; i < maxpin ; i++) 
              {
                pos = header.indexOf("saida" + String(i));
                
                if (pos > 0) 
                {
                  tempos_acionamento[saida] = header.substring(pos + 7, pos + 8).toInt();
                }
              }
            
              save();
            }
            
            pos = header.indexOf("OUTPUT");
            
            if (pos > 0) 
            {
              saida = header.substring(pos + 6, pos + 7).toInt();
            }

//Setor responsável pelos textos e botões da página Web//

            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta charset='utf-8' name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 25px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");

           
            str_aux = data_formatada; 

            client.println("<body><h1>Alimentador Automático</h1>");
         
            client.println("</p>");
            client.println("<body><h2>" + str_aux + "</h2>");
            client.println("<p>Motor " + RelayState + "</p>");

            if (RelayState == "Desligado") 
            {
              client.println("<p><a href=\"/2/on\"><button class=\"button\">ON</button></a></p>");
            } 
            
            else 
            {
              client.println("<p><a href=\"/2/off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            str_aux = tempos_acionamento[saida] ;
            
            client.println("<form method='get'>");
            client.println("  <b>Períodos de Acionamento:</b><input type='number' step ='1' min='0' max='" + String(maxval) + "' name='saida" + String(saida) + "' maxlength='3' value='" + str_aux + "'size='5'/>");
            client.println("  <input type='submit' name='bt' value='ATUALIZAR'/>");
            client.println("</form>");

            client.println("      <form method='get'>");
            client.println("         <table align='center'>");
            client.println("            <tr>");
            client.println("               <td>");
            client.println("                  <font size='4'><b><u>Agendamento de Acionamento Diário</u></b></font>");
            client.println("               </td>");
            
            for (i = 0 ; i < tempos_acionamento[saida] ; i++)
            {
              client.println("            <tr>");
              client.println("               <td>");
              client.println("                  <b>Início:</b> <input type='time' name='tempo" + String(i) + "' maxlength='2' size='5' value = '" + tratamento(i) + "'/> hora(s)");
              client.println("               </td>");
              client.println("            </tr>");
              client.println("            <tr>");
              client.println("               <td>");
              
              str_aux = tempos[saida][i].durar;
              
              client.println("                  <b>Duração:</b><input type='number' step ='1' min='0' max='3' name='durar" + String(i) + "' maxlength='3' value='" + str_aux + "'size='5'/> minuto(s)");
              client.println("               </td>");
              client.println("            </tr>");
              client.println("            <tr>");
              client.println("               <td>");
              client.println("               </td>");
              client.println("            </tr>");
            }
            
            client.println("            <tr>");
            client.println("               <td align='center'>");
            client.println("                  <input type='submit' name='btag' value='AGENDAR'/>");
            client.println("               </td>");
            client.println("            </tr>");
            client.println("         </table>");
            client.println("      </form>");
            client.println("</body></html>");
            client.println();
            
            break;
          } 
          
          else 
          {   
            currentLine = "";
          }
        } 
        
        else if (c != '\r') 
        {  
          currentLine += c;
        }
      }
    }
   
    header = "";
    
    client.stop();
  }
}
