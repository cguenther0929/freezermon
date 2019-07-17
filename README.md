__DESCRIPTION:__
Contained within this repository are the source files for the freezer monitor.  The heart and soul of the freezer monitor is the ESP8266, more specifically, the development board named the ESP8266 Thing (developed by Sparkfun)

**Revisions:**

v1.0 : This version has been running on the freezer monitor for quite some time.  Email alerts are sent to notify health status, and of course, when temperature reaches a critical point.  There's a web interface that displays current freezer temp, which also allows for the temperature trip point to be updated.  It is desired that a SW filter be added to this version to prevent false alerts.  

v1.1 : Added filtering to measurements.  Added a button to send a test email, but that feature remains untested.  At the time of uploading these changes, the mail server was down, and emails were not sending. 

V1.2 : Corrected the issue with not being able to send emails by adding client.setInsecure() in Gsender.cpp.  The test email button present on the web interface remains untested and seemingly inoperable.  