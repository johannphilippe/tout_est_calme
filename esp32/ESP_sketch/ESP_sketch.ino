#pragma once 

#include "driver/ledc.h"
#include "esp32-hal-ledc.h"
#include "WiFi.h"
#include "WiFiUdp.h"

#include "osc_parser.h"

#include "voltage_control.h"
voltage_control cv(voltage_control::inertia_t::very_slow);
/*
  TODO pour avril 2026 : 
  - Recevoir la fréquence en OSC 
  - Recevoir la fréquence désirée 
  - Adapter le PWM en fonction 
*/

template<typename T>
struct shared_data
{
  shared_data(T init) 
  : data(init)
  {
    semaphore = xSemaphoreCreateMutex();
    if(semaphore == NULL) {

    } else {
      xSemaphoreGive(semaphore); 
    }
  }

  bool write(T x, size_t max_wait_ticks = TICKS) 
  {
    if(xSemaphoreTake(semaphore, max_wait_ticks) == pdFAIL) 
    {
      return false;
    } 
    // Acquired 
    data = x; 
    xSemaphoreGive(semaphore); 
    return true;
  }

  bool read(T& x, size_t max_wait_ticks = TICKS)
  {
    if(xSemaphoreTake(semaphore, max_wait_ticks) == pdFAIL) 
    {
      return false;
    } 
    // Acquired 
    x = data;
    xSemaphoreGive(semaphore);
    return true; 
  }

  constexpr static size_t TICKS = 100; 
  SemaphoreHandle_t semaphore;
  T data;
};

shared_data<float> target_freq(0.0f);
shared_data<float> current_pitch(0.0f); 
shared_data<int32_t> direct_pwm(0);
shared_data<bool> target_enable(false);
shared_data<bool> target_changed(false); 
shared_data<bool> shutdown(false); 

osc_parser osc;

/*
  PWM Config 
*/

constexpr const int PWM = 16;
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

//#define CELESTE
//#define GISELE
//#define GABRIELLE 
//#define SONORA12
#define PIAF 

#ifdef PIAF 
  constexpr uint8_t ip_postfix = 10;
#elif defined(GABRIELLE)
#elif defined(GISELE)
#elif defined(CELESTE)
#endif

struct wifi_config 
{
  wifi_config() 
  : local_ip(123,45,1,ip_postfix)
  , gateway(123,45,1,1)
  , subnet(255, 255, 255, 0)
  {}

  WiFiUDP udp;
  IPAddress local_ip, gateway, subnet;

  static constexpr uint16_t PORT = 8765; 
  static constexpr const char *SSID = "ulysse";
  static constexpr const char *PWD = "";
  static constexpr uint16_t BUFFER_SIZE = 512;
  char packet_buffer[BUFFER_SIZE];

  String ip_str;
};
wifi_config wifi;


void network_task(void* params)
{
  while(true) {
    // Wifi 
    int packet_size = wifi.udp.parsePacket();
    if(packet_size) 
    {
      int len = wifi.udp.read(wifi.packet_buffer, 512); 
      
      for(size_t i = 0; i < len; ++i)
      {
        Serial.print((char)wifi.packet_buffer[i]);
      }
      Serial.println();
      
      if(!osc.parse(wifi.packet_buffer, len)) 
      {
        Serial.println("Failed to parse OSC");
      } else { // Parsed successfully 
        if(osc.match("/siren/pitch")) 
        {
          float t = osc.read<float>(0);
          if(!current_pitch.write(t)) 
          {
            Serial.println("Failed to write current pitch");
          }

        } else if(osc.match("/siren/target")) 
        {
          float t = osc.read<float>(0);
          if(!target_freq.write(t)) 
          {
            Serial.println("Failed to write target freq");
          } else {
            target_enable.write(true);
            target_changed.write(true);
            //Serial.printf("Received Target %f, enabling target mode \n", t);
          }
        } else if(osc.match("/siren/shutdown"))
        {
          int32_t res = osc.read<int32_t>(0); 
          bool s = (res > 0) ? true : false;  
          shutdown.write(s);
          target_enable.write(false);
          //Serial.printf("Shutdown mode changed : %i \n", res);
        } else if(osc.match("/siren/inertia"))
        {

          int32_t res = osc.read<int32_t>(0); 
          //Serial.printf("Inertia : %i \n", res);
          cv.set_inertia(static_cast<voltage_control::inertia_t>(res));
        } else if(osc.match("/siren/pwm"))
        {
          target_enable.write(false);
          int32_t res = osc.read<int32_t>(0); 
          direct_pwm.write(res);
        }
      }
    }
    vTaskDelay(2);
  }
}

TaskHandle_t task_net; 

void setup() {

  ledcAttach(PWM, frequency, resolution);
  ledcWrite(PWM, 0);

  /* No daisy YET 
  daisy_rcv.begin(9600, SERIAL_8N1, 16, 17);
  */

  Serial.begin(9600);

  
  Serial.printf("CV Resolution & range %f %f \n", cv.resolution, cv.range);
  WiFi.mode(WIFI_STA);

  if(!WiFi.config(wifi.local_ip, wifi.gateway, wifi.subnet)) 
  {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(wifi.SSID, wifi.PWD);
  Serial.print("\nConnecting ");
  while(WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println("\nConnected !");
  Serial.print("Local ESP32 IP : ");
  Serial.println(WiFi.localIP());
  wifi.ip_str = WiFi.localIP().toString(); 

  WiFi.setSleep(false);
  if(!wifi.udp.begin(wifi.local_ip, wifi.PORT)) 
  {
    Serial.println("Failed to bind UDP to IP");
  }

  /* 
    Start networking on dedicated core
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
/*
  Runs on Core 1 
*/
float target_frequency_value = 0.0f; 
float current_pitch_value = 0.0f; 
float last_pitch = 0.0f;
bool target_changed_value = false;
bool shutdown_val = false;
int8_t direction = false;
bool target_enable_value = false;

void loop() {
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
  shutdown.read(shutdown_val);
  if(shutdown_val) {
    ledcWrite(PWM, 0);
    return;
  }

  target_enable.read(target_enable_value);
  if(!target_enable_value) {
    int32_t pwm_value = 0;
    direct_pwm.read(pwm_value);
    ledcWrite(PWM, pwm_value);
    return;
  }

  // First check if anything has changed in the target 
  if(target_changed.read(target_changed_value) && target_changed_value) 
  {
    float current = current_pitch_value;
    if(target_freq.read(target_frequency_value)) 
    {
      last_pitch = current;
    }
    target_changed_value = false; 
    target_changed.write(false); 
  }

  // Then read current pitch 
  current_pitch.read(current_pitch_value); 

  uint16_t pwm_value = cv(target_frequency_value, last_pitch, current_pitch_value);
  //Serial.printf("PWM value : %f \n", pwm_value);
  ledcWrite(PWM, pwm_value);

  vTaskDelay(1);
  // Now check how far we are from the target, and adjust the PWM accordingly
}
