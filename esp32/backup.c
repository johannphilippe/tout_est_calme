#include "driver/ledc.h"
#include "esp32-hal-ledc.h"
#include "WiFi.h"
#include "WiFiUdp.h"

/*
  TODO pour avril 2026 : 
  - Recevoir la fréquence en OSC 
  - Recevoir la fréquence désirée 
  - Adapter le PWM en fonction 
*/


/*
  PWM Config 
*/
constexpr uint16_t pow2_u16(uint16_t exponent)
{
  uint16_t base = 2;
  for(size_t i = 0; i < exponent; ++i)
    base *= 2; 
  return base; 
}

constexpr const int PWM = 12;
constexpr const int CHANNEL = 0;
constexpr uint16_t frequency = 18000;
constexpr uint8_t resolution = 12;
constexpr uint16_t range = pow2_u16(resolution);
int duty = 2048; 


/*
  Serial Config 
*/
uint8_t cnt = 0;
HardwareSerial daisy_rcv(2);
uint16_t freq = 0; 
uint8_t data_rcv[2];

/*
  Wifi config 
*/
struct wifi_config 
{

};
WiFiUDP udp;
constexpr uint16_t PORT = 8765; 
constexpr const char *SSID = "johannhotspot";
constexpr const char *PWD = "";
char packet_buffer[512];

struct osc_parser 
{
  osc_parser() = default;

  bool parse(char * data, uint16_t len)
  {
    uint16_t cnt = 0;
    uint8_t internal_cnt = 0;
    char cur = data[cnt];
    while(cur != '\0' && cnt < len)
    {
      cur = data[cnt];
      
      if(cur == '/' || cur == ' ') 
      {
        buf[internal_cnt] = '\0';
        Serial.printf("OSC address : %s \n", buf);
        internal_cnt = 0; 
      } else {
        buf[internal_cnt] = cur;
        internal_cnt++;
      }
      cnt++; 
    }

    Serial.printf("OSC msg : %s \n", buf);
    return true;
  }
  char buf[256]; 
};

osc_parser osc;


void network_task(void* params)
{

}
TaskHandle_t task_net; 

void setup() {
  ledcAttach(PWM, frequency, resolution);

  daisy_rcv.begin(9600, SERIAL_8N1, 16, 17);
  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PWD);
  Serial.print("\nConnecting ");
  while(WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println("\nConnected !");
  Serial.print("Local ESP32 IP : ");
  Serial.println(WiFi.localIP());
  WiFi.setSleep(false);
  udp.begin(PORT);


  /* 
    Start networking on core 2 
  */
  xTaskCreatePinnedToCore(
    network_task, 
    "Networking", 
    10000, 
    NULL, 
    0, 
    &task_net, 
    0
  );
  delay(1000);
}

void loop() {

  
  // Wifi 
  int packet_size = udp.parsePacket();
  if(packet_size) 
  {
    int len = udp.read(packet_buffer, 512); 
    //if(len > 0) packet_buffer[len - 1] = 0;
    //Serial.println(packet_buffer);
    for(size_t i = 0; i < len; ++i)
      Serial.print((char)packet_buffer[i]);
    Serial.println();
    if(!osc.parse(packet_buffer, len)) 
    {
      Serial.println("Failed to parse OSC");
    }
  }
  

  // Serial 
  /*
  while(daisy_rcv.available() > 0)
  {
    int incoming = daisy_rcv.read();
    if(incoming < 0) break; // invalid
    if(cnt < 2) {
      data_rcv[cnt] = (uint8_t)incoming; 
      cnt++;
    }
    if(cnt == 2) {
      uint16_t freq = (uint16_t(data_rcv[0]) << 8) | uint16_t(data_rcv[1]);
      Serial.printf("freq = %u \n", freq);
      cnt = 0;
    }
  }
  */

  //delay(100);


  // PWM working
  //duty = rand() % range;
  //ledcWrite(PWM, duty);
  //Serial.printf("duty = %d \n", duty);


  //delay(100);
}

