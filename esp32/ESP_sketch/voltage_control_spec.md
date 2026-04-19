# Siren voltage correction 

The voltage_control.h file contains a structure supposed to adjust the PWM signal, which controls the voltage sent to the siren. 
Sirens all have a different inertia, this is why I implemented 4 intertial profiles. 
These profiles include a peak (maximum value, or 0 depending on if the siren goes up or down) since some sirens need it to start rotating (or stop). 
Though, it doesn't work well for now. 

Actually, I think it is going too fast, so the frequency tracking can't detect frequencies. 
Also, the voltage_control algorithm is poorly designed. 
Work on a solution to change this algorithm, so it can acutally reach the target frequency and keep adjusting around it (sirens response to voltage contains nonlinearity, it will move slightly anyway )