# Siren pitch correction 

This project aims to detect and adjust pitch of  8 old rotating sirens. 
The principle is simple : 
- A pickup is placed close to the engine of the siren 
- We track its frequency (algorithms are in csound_opcode/sirens.cpp)
- The tracked frequency needs a ratio multiplier (audible frequency is a multiple of tracked)
- We then send the target frequency (what we want) and the current tracked frequency to ESP32 
- The ESP32 drives a PWM signal controlling the voltage sent to the siren  



# notes 

- If no output frequency : 
    * Dropped too fast : PWM 0 
    * Frequency too far for a long time : reset frequency


# First TODO 

- Configure OSC modes : 
    - direct PWM : disable Frequency tracking 
    - target freq : enables 
    - shutdown : outputs 0