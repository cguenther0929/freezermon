# DESCRIPTION:
Contained within this repository are the source files for the freezer monitor.  The heart and soul of the freezer monitor is the ESP8266, more specifically, the development board named the ESP8266 Thing (developed by Sparkfun)

The homes router shall be setup to perform port forwarding on port number 300. In addition to this, the **duckdns.org** service will be utilized so that an intelligent web address can be entered opposed to the WAN IP + port number.  (i.e. **http://cjgfreezer.duckdns.org:300** instead of **24.217.178.109:300**).  

## Compiling
The version of Arduino utilized at the time of recording this note is 1.8.15.  

It's necessary to select the *SparkFun ESP8266 Thing* board under *Tools > Board > ESP8266 (3.0.0) > SparkFUn ESP8266 Thing Board*.  

## Revisions:

v1.0 : This version has been running on the freezer monitor for quite some time.  Email alerts are sent to notify health status, and of course, when temperature reaches a critical point.  There's a web interface that displays current freezer temp, which also allows for the temperature trip point to be updated.  It is desired that a SW filter be added to this version to prevent false alerts.  

v1.1 : Added filtering to measurements.  Added a button to send a test email, but that feature remains untested.  At the time of uploading these changes, the mail server was down, and emails were not sending. 

V1.2 : Corrected the issue with not being able to send emails by adding client.setInsecure() in Gsender.cpp.  The test email button present on the web interface remains untested and seemingly inoperable.  

V1.3 : Remove ex-wife (Meghan Spindler) from freezer messages.  

V2.0 : Modified email sending algorithm such that it now leverages SMTP2GO to send email since Google dropped support for less secure apps. The Test Email button is now functional.