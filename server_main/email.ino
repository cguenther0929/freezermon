byte eRcv() {
  byte respCode;
  byte thisByte;
  int loopCount = 0;

  while (!client.available()) {
    delay(1);
    loopCount++;
    // if nothing received for 10 seconds, timeout
    if (loopCount > 10000) {
      client.stop();
      Serial.println(F("\r\nTimeout"));
      return 0;
    }
  }

  respCode = client.peek();
  while (client.available())
  {
    thisByte = client.read();
    Serial.write(thisByte);
  }

  if (respCode >= '4')
  {
    return 0;
  }
  return 1;
}

bool WiFiConnect( const char * ssid, const char * password ) {
    int i = 0;
    int timeout = (int)WIFI_CONNECT_TIMEOUT_S/0.5;
    
    
    WiFi.begin(ssid, password);

    /**
     * Print diagnostic data
     * for WiFi if logging
     * is enabled
     */
    #if defined(ENABLE_LOGGING)
        Serial.println("");
        Serial.print("\tMy MAC address is: "); Serial.println(WiFi.macAddress());
        Serial.print("\tConnecting to SSID: "); Serial.println(ssid);
        Serial.print("\tSSID password: "); Serial.println(password);
    #endif

    
    // Wait for connection
    #if defined(ENABLE_LOGGING)
        Serial.print("\tWiFi Connecting\t");
    #endif
    
    while ((WiFi.status() != WL_CONNECTED) && i < timeout) {
        delay(500);
        i++;
        #if defined(ENABLE_LOGGING)
            Serial.print('.');
        #endif
    }
    
    #if defined(ENABLE_LOGGING)
        Serial.println("");
    #endif

    if(i == timeout){
    
        #if defined(ENABLE_LOGGING)
            Serial.println("\tWiFi Connection timeout!");
            return false;
        #endif
        return false;
    }
    else {
        #if defined(ENABLE_LOGGING)
            Serial.println("\tWiFi connected!");
            Serial.print("\tMy local IP: ");
            Serial.println(WiFi.localIP());
        #endif

    }

    return true;

  
}

bool send_email_message() {
   
    #if defined(ENABLE_LOGGING)
        Serial.println("Sending Email...");
    #endif
    
                
    WiFi.forceSleepWake();
    delay(1);                       // There are claims that a non-zero delay is required after calling the wake function

    if(!WiFiConnect(ssid, password)) {
        #if defined(ENABLE_LOGGING)
            Serial.println("Deep sleep.");
        #endif
    }

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    #if defined(ENABLE_LOGGING)
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    #endif

    /**
     * @brief Make a client connection
     */
    #if defined(ENABLE_LOGGING)
        Serial.println(F("Making client connection"));
    #endif
    if (client.connect(email_server, 2525) == 1) {
        #if defined(ENABLE_LOGGING)
            Serial.println(F("connected"));
        #endif
    } else {
        #if defined(ENABLE_LOGGING)
            Serial.println(F("connection failed"));
        #endif
        return false;
    }
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.println(F("\t*** Error connecting to the client"));
        #endif
        return true;
    }


    /**
     * @brief Sending EHLO command
     */
    #if defined(ENABLE_LOGGING)
        Serial.println(F("Sending EHLO"));
    #endif
    client.println("EHLO www.example.com");
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.println(F("\t*** Error sending EHLO command."));
        #endif
        return false;
    }


    /**
     * @brief Sending auth login command
     */
    #if defined(ENABLE_LOGGING)
        Serial.println(F("Sending auth login"));
    #endif

    client.println("auth login");
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.println(F("\t*** Error sending AUTH LOGIN command."));
        #endif
        return false;
    }


    /**
     * @brief Send SMTP2GO User Account Credentials
     * We should passing in the BASE64 encoded
     * username for the SMTP2GO account
     */
    #if defined(ENABLE_LOGGING)
        Serial.print(F("Sending SMTP2GO B64 User Account Name: "));
        Serial.println(buf_hyg_smtp2go_account);
    #endif

    client.println(buf_hyg_smtp2go_account); 
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.print(F("\t*** Error sending SMTP2GO Username: "));
            Serial.println(buf_hyg_smtp2go_account);
        #endif
        return false;
    }


    /**
     * @brief Send SMTP2GO Password
     * 
     * THis should be the BASE64 password 
     * for you SMTP2GO account
     */
    #if defined(ENABLE_LOGGING)
        Serial.print(F("Sending B64 SMTP2GO Password: "));
        Serial.println(buf_hyg_smtp2go_password);
    #endif

    client.println(buf_hyg_smtp2go_password);  
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.println(F("\t*** Error sending SMTP2GO password"));
        #endif
        return false;       
    }


    /**
     * @brief Command for MAIL From:
     * i.e.  --> client.println(F("MAIL From: clinton.debug@gmail.com"));
     * 
     */
    memset(buf_temp, 0, TEMP_BUF_SIZE);
    strcpy(buf_temp, "MAIL From: ");
    strcat(buf_temp, "clinton.debug@gmail.com");
    client.println(buf_temp);
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.print(F("\t*** Error on command: "));
            Serial.println(buf_temp);
        #endif
        return false;
    }


    /**
     * @brief Enter recipient address
     * First, fill temp buffer with null characters
     * i.e.  -->  client.println(F("RCPT To: clinton.guenther@gmail.com"));
     * 
     */
    #if defined(ENABLE_LOGGING)
        Serial.print(F("Sending To: "));
        Serial.println(buf_recipient_email_addr);
    #endif

    memset(buf_temp, 0, TEMP_BUF_SIZE);
    strcpy(buf_temp, "RCPT To: ");
    strcat(buf_temp, buf_recipient_email_addr);

    client.println(buf_temp);
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.print(F("\t*** Error on command: "));
            Serial.println(buf_temp);
        #endif
        return false;
    }


    /**
     * @brief Send DATA command
     */
    #if defined(ENABLE_LOGGING)
        Serial.println(F("Sending DATA"));
    #endif

    client.println(F("DATA"));
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.println(F("\t*** Error on command \"DATA\"."));
        #endif
        return false;
    }


    /**
     * @brief Sending To: command
     * i.e.  --> client.println(F("To: clinton.guenther@gmail.com"));
     */
    #if defined(ENABLE_LOGGING)
        Serial.println(F("Sending email"));
    #endif
    memset(buf_temp, 0, TEMP_BUF_SIZE);
    strcpy(buf_temp, "To: ");
    strcat(buf_temp, buf_recipient_email_addr);
    client.println(buf_temp);


    /**
     * @brief Sending From: command
     * i.e. -->  client.println(F("From: clinton.debug@gmail.com"));
     */
    // client.println(F("From: clinton.debug@gmail.com"));
    memset(buf_temp, 0, TEMP_BUF_SIZE);
    strcpy(buf_temp, "From: ");
    strcat(buf_temp, buf_sender_email_addr);
    client.println(buf_temp);


    /**
     * @brief Send the subject
     */
    client.println(F("Subject: Freezer Alarm Status\r\n"));
    client.println(email_body);
    client.println(F("."));
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.println(F("\t*** Error sending DOT to complete transaction"));
        #endif
        return false;
    }


    /**
     * @brief Sending QUIT
     * 
     */
    #if defined(ENABLE_LOGGING)
        Serial.println(F("Sending QUIT"));
    #endif
                
    client.println(F("QUIT"));
    if (!eRcv()){
        #if defined(ENABLE_LOGGING)
            Serial.println(F("\t*** Error sending \"QUIT\"."));
        #endif
        return false;
    }


    /**
     * @brief Disconnecting 
     */
    client.stop();

    #if defined(ENABLE_LOGGING)
    Serial.println(F("disconnected"));
    #endif

    return true;
} 