#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <settings.h>

// pin16 = d0 on node mcu
// pin5 = d1 on node mcu
// pin4 = d2 on node mcu
#define TELEX_POWER_PIN 4
#define TELEX_WRITE_PIN 5
#define TELEX_READ_PIN 16

#define BELL 0x0B
#define ALPHABET_1 0x1F
#define ALPHABET_2 0x1B

const uint8_t alphabet1[32]={'*','e',0x0A,'a',' ','s', 'i','u',0x0D,'d','r','j','n','f','c','k','t','z','l','w','h','y','p','q','o','b','g','~','m','x','v','^'};
const uint8_t alphabet2[32]={'*','3',0x0A,'-',' ','\'','8','7',0x0D,'$','4','%',',','!',':','(','5','+',')','2','#','6','0','1','9','?','&','~','.','/','=','^'};


uint8_t currentWriteAlphabet=0;
uint8_t currentReadAlphabet=0;

uint8_t telexPower=0;
unsigned long telexPowerTimeout;
uint8_t telexLocalEcho=0;

uint8_t telexSendString[80];

uint8_t baudotGetAlphabet(uint8_t *data)
{
  for (uint8_t zz=0;zz<32;zz++)
  {  // first search in current alphabet to prevent unnecesarry switching
     if (currentWriteAlphabet==1) 
     {
       if (alphabet1[zz]==data[0])
         return 1;
       if (alphabet2[zz]==data[0])
         return 2;
     }
     else
     {
       if (alphabet2[zz]==data[0])
         return 2;
       if (alphabet1[zz]==data[0])
         return 1;
     }
  }
  return 0;
}

uint8_t baudotEncodeChar(uint8_t *data)
{
  // transform string so it is suitable to send to telex
  // function returns alphabet to be used
  uint8_t alphabet;
  
  // set to lowercase
  if ((data[0] >= 65)&&(data[0] <= 90))
    data[0]+=32;
  // replace all quote type to single quote (') character
  if ((data[0]=='"')||(data[0]=='`'))
    data[0]='\'';
  // replace '*','~','^' characters with '+'
  // '~' character is used to encode null, char/digit switch and digi/char switch 
  if ((data[0]=='*')||(data[0]=='~')||(data[0]=='^'))
    data[0]='+';	
  alphabet=baudotGetAlphabet(data);
  if (!alphabet) // character not in alphabet, so replace with '+' character
  {
    data[0]='+';
    alphabet=2;	
  }

  for (uint8_t zz=0;zz<32;zz++)
  {  
    if (alphabet==1)
    {
      if (alphabet1[zz]==data[0])
      {
      	data[0]=zz;
      	zz=32;
      }
    } 
    else
    {
      if (alphabet2[zz]==data[0])
      {
        data[0]=zz;
        zz=32;
      }
    }
  }
  return alphabet;
}

uint8_t baudotDecodeChar(uint8_t data)
{
  if (data==0x1F)
    currentReadAlphabet=1;
  if (data==0x1B)
    currentReadAlphabet=2;
  if (currentReadAlphabet==2)
    return alphabet2[data];    	    
  else // assume alphabet is 1 if alphabet is not set (0)
    return alphabet1[data];    	    
}

/*
  // hardcoded character
  digitalWrite(TELEX_WRITE_PIN, LOW); // stopbit
  delay(20); 
  digitalWrite(TELEX_WRITE_PIN, HIGH); //lsb
  delay(20); 
  digitalWrite(TELEX_WRITE_PIN, HIGH); 
  delay(20); 
  digitalWrite(TELEX_WRITE_PIN, LOW); 
  delay(20); 
  digitalWrite(TELEX_WRITE_PIN, HIGH); 
  delay(20); 
  digitalWrite(TELEX_WRITE_PIN, HIGH); // msb
  delay(20); 
  digitalWrite(TELEX_WRITE_PIN, HIGH); // startbit
  delay(100); 
*/

void sendChar(uint8_t data)
{
  digitalWrite(TELEX_WRITE_PIN, LOW); // startbit
  delay(20); 
  for (uint8_t zz=0;zz<5;zz++)
  { 
    if (data&0x01)
      digitalWrite(TELEX_WRITE_PIN, HIGH); 
    else
      digitalWrite(TELEX_WRITE_PIN, LOW); 
    data>>=1;
    delay(20);
    
  }
  digitalWrite(TELEX_WRITE_PIN, HIGH); // stopbit
  delay(20);
}

uint8_t receiveChar(void)
{
  uint8_t data=0;

  delay(10); // wait until we are half way into the start bit 
  if (telexLocalEcho)
     digitalWrite(TELEX_WRITE_PIN, LOW); 
  delay(20); 
  for (uint8_t zz=0;zz<5;zz++)
  {
    if (!digitalRead(TELEX_READ_PIN))
    {
      if (telexLocalEcho)
        digitalWrite(TELEX_WRITE_PIN, HIGH); 
      data|=0x20;
    }
    else
    {
      if (telexLocalEcho)
        digitalWrite(TELEX_WRITE_PIN, LOW); 
    }
    data>>=1;
    delay(20);  
  }
   digitalWrite(TELEX_WRITE_PIN, HIGH); 
   delay(11); // wait until we are finished with the last bit to avoid detecting false startbit
    
  telexPowerTimeout=millis();
  return data;
}

void sendAlphabetSwitch(uint8_t alphabet)
{
  if (alphabet==1)
    sendChar(ALPHABET_1);
  else
    sendChar(ALPHABET_2);
  delay(80);
}

void sendString(uint8_t *data)
{
  uint16_t zz=0;
  uint8_t alphabet=0;
  
  while(data[zz])
  {
    alphabet=baudotEncodeChar(data+zz);
    if (alphabet!=currentWriteAlphabet)
    {
      sendAlphabetSwitch(alphabet);
      currentWriteAlphabet=alphabet;
    }
    sendChar(data[zz]);
    zz++;
  }
}



// ******************* String form to sent to the client-browser ************************************

String preStr=
"Beste ...\n\r\n\r"
"Bla bla bla\n\r"
"bla bla!\n\r\n\r"
"Met vriendelijke groet,\n\r"
".....\n\r";

String form =
  "<p><center><form name='form' action='msg'><input type='hidden' name='formAction'>"
  "<p>Enter telex message<br>" 
  "<input type='text' name='telexMsg' size=50><input type='button' value='Send' onclick='document.form.formAction.value=\"msg\";document.form.submit();'>"
  "</p>"
  "<p>Incomming message settings<br>" 
  "<input type='button' value='Toggle power' onclick='document.form.formAction.value=\"pwr\";document.form.submit();'><br>"
  "<input type='button' value='Toggle local echo' onclick='document.form.formAction.value=\"echo\";document.form.submit();'><br>"
  "<input type='button' value='Preformatted text' onclick='document.form.formAction.value=\"preformatted\";document.form.submit();'><br>"
  "</p>"
  "</form>"
  "</center></p>";

ESP8266WebServer server(80);                             // HTTP server will listen at port 80

  // Restore special characters that are misformed to %char by the client browser
String htmlDecode(String &str)
{
  // str.replace("+", " ");      
  str.replace("%20", " ");  
  str.replace("%21", "!");  
  str.replace("%22", "\"");  
  str.replace("%23", "#");
  str.replace("%24", "$");
  str.replace("%25", "%");  
  str.replace("%26", "&");
  str.replace("%27", "'");  
  str.replace("%28", "(");
  str.replace("%29", ")");
  str.replace("%2A", "*");
  str.replace("%2B", "+");  
  str.replace("%2C", ",");  
  str.replace("%2D", "-");  
  str.replace("%2E", ".");  
  str.replace("%2F", "/");   
  str.replace("%3A", ":");    
  str.replace("%3B", ";");  
  str.replace("%3C", "<");  
  str.replace("%3D", "=");  
  str.replace("%3E", ">");
  str.replace("%3F", "?");  
  str.replace("%40", "@"); 
  str.replace("%5B", "["); 
  str.replace("%5C", "\\"); 
  str.replace("%5D", "]"); 
  str.replace("%5E", "^"); 
  str.replace("%5F", "_"); 
  str.replace("%60", "`"); 
  str.replace("%7B", "{"); 
  str.replace("%7C", "|"); 
  str.replace("%7D", "}"); 
  str.replace("%7E", "~"); 
  return str;
}

void handle_msg() 
{
  server.send(200, "text/html", form);    // Send same page so they can send another msg
  String formAction=server.arg("formAction");
  if (formAction=="msg")
  {
    String msg=server.arg("telexMsg");
    strcpy((char *)telexSendString,htmlDecode(msg).c_str());
    int len=strlen((char *)telexSendString);
    telexSendString[len++]='\r';
    telexSendString[len++]='\n';
    telexSendString[len++]=0;

    Serial.print("Send telex msg: ");
    Serial.print((char *)telexSendString);  
  
    if (!telexPower)
    {
      Serial.println("Power up telex");
      digitalWrite(TELEX_POWER_PIN, HIGH); // power on
      Serial.println("Wait for motor to stabilise");
      delay(2000);
    }

    sendString(telexSendString);
   
    if (!telexPower)
    {
      delay(1000);
      digitalWrite(TELEX_POWER_PIN, LOW); // power off
      Serial.println("Power down telex");
    }
    telexPowerTimeout=millis();
  }		  
  else if (formAction=="pwr")
  {
    Serial.println("Toggle telex power");
    if (telexPower)
    {  
      digitalWrite(TELEX_POWER_PIN, LOW); // power off
      telexPower=0;
    }
    else
    {
      digitalWrite(TELEX_POWER_PIN, HIGH); // power on
      telexPower=1;
      telexPowerTimeout=millis();
    }
  }
  else if (formAction=="echo")
  {
    Serial.println("Toggle telex local echo");
    telexLocalEcho=!telexLocalEcho;
  }	
  else if (formAction=="preformatted")
  {
    if (!telexPower)
    {
      Serial.println("Power up telex");
      digitalWrite(TELEX_POWER_PIN, HIGH); // power on
      Serial.println("Wait for motor to stabilise");
      delay(2000);
    }

    sendString((uint8_t*)preStr.c_str());
   
    if (!telexPower)
    {
      delay(1000);
      digitalWrite(TELEX_POWER_PIN, LOW); // power off
      Serial.println("Power down telex");
    }
    telexPowerTimeout=millis();
  }
}


void setup()
{
//  ESP.wdtDisable();                               // used to debug, disable wachdog timer, 
//  while((!Serial)&&(millis()<10000)); 
  Serial.begin(115200);                           // full speed to monitor
  Serial.println("Boot ready!");

  pinMode(TELEX_POWER_PIN, OUTPUT);
  pinMode(TELEX_WRITE_PIN, OUTPUT);
  pinMode(TELEX_READ_PIN, INPUT);
  digitalWrite(TELEX_WRITE_PIN, HIGH); // idle
  digitalWrite(TELEX_POWER_PIN, LOW); // idle

  WiFi.begin(SSID, PASS);                         // Connect to WiFi network
  while (WiFi.status() != WL_CONNECTED) 
  {         					  // Wait for connection
    delay(500);
    Serial.print(".");
  }
  
  server.on("/", []() {
    server.send(200, "text/html", form);
  });
  server.on("/msg", handle_msg);                  // And as regular external functions:
  server.begin();                                 // Start the server 

  Serial.print("SSID : ");                        // prints SSID in monitor
  Serial.println(SSID);                           // to monitor             
  
  char result[16];
  sprintf(result, "%3d.%3d.%3d.%3d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  Serial.println();Serial.println(result);
  
  Serial.println("WebServer ready!   ");
  Serial.println(WiFi.localIP());                 // Serial monitor prints localIP

  telexPowerTimeout=millis();

}

uint8_t char_in=0;
void loop()
{
  server.handleClient();                        // checks for incoming messages
  delay(1);
  if (digitalRead(TELEX_READ_PIN)) // startbit detected  
  {
    char_in=receiveChar();
    Serial.print("Character received: ");
    Serial.println(char(baudotDecodeChar(char_in)));
  }
  
  if ((char_in==0x0D)&&(currentReadAlphabet==2))
  {
    Serial.println("Soft power down received ... powering down telex");
    digitalWrite(TELEX_POWER_PIN, LOW); // power off
    telexPower=0;
    char_in=0;
  }
  
  if ((telexPower)&&((millis()-telexPowerTimeout)>30000))
  {
    Serial.println("Keyboard not used for 30 seconds ... powering down telex");
    digitalWrite(TELEX_POWER_PIN, LOW); // power off
    telexPower=0;
  }
  
}
