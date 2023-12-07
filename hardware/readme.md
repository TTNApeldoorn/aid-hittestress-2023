# Homerus Klimaat sensor 2023
Notes on hardware for Homerus Klimaatsensor. 

## Hardware




Ik heb de software nu draaiend, met deepsleep en interne GPS,  met  SPS30 en AM2315 op de i2c.
De AM2315 en SPS30 zitten parallel op de I2C bus als volgt:
 pin 21 SDA
 pin 22 SCL
 AM2315 power op 3V3
 SDS30 power nu op de 5v pin, deze zakt naar 4v als het board op de batterij draait, dus volgens de specs net niet genoeg. Hiervoor moet dus een step-up naar 5v bij geplaatst worden.

Het board draait nu 24 uur op de batterij, en Vbat is van 4,095V nu gezakt naar 3,898V. Volgens mij houdt die het wel een tijdje uit op de batterij.

De 5v pin op het TTGO board is een 5v output pin, en geen 5v input. Dus de externe 5v voeding moet aangesloten worden op de micro USB connector, anders werkt het laad circuit niet.
