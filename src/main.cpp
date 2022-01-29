#include <main.h>

void setup()
{

  Serial1.begin(57600, SERIAL_8N1, RX_NEX, TX_NEX);
  Serial.begin(115200);
  mySerial.begin(9600);
  printer.begin(80);

  // send("rest");
  // send(incStr);

  // WiFi.softAP(ssid, password);
  // WiFi.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  WiFi.setHostname("Stend");
  int counter_WiFi = 0;
  while (WiFi.status() != WL_CONNECTED && counter_WiFi < 6)
  {
    delay(1000);
    counter_WiFi++;
  }

  IPAddress ip_address = WiFi.localIP();
  Serial.print("\nAP IP address: ");
  Serial.println(ip_address);
  Serial.printf("Mac Address:\t");
  Serial.println(WiFi.macAddress());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "Stend_Kompressor | " + incStr); });

  AsyncElegantOTA.begin(&server); // Start ElegantOTA

#ifdef USE_WEB_SERIAL
  WebSerial.begin(&server);
  WebSerial.msgCallback(recvMsg);
#endif
  server.begin();

  xTaskCreatePinnedToCore(
      updatepressure,        /* Снятие показаний датчиков давления*/
      "Task_updatePressure", /* Название задачи */
      10000,                 /* Размер стека задачи */
      NULL,                  /* Параметр задачи */
      1,                     /* Приоритет задачи */
      &Task_updatePressure,  /* Идентификатор задачи, чтобы ее можно было отслеживать */
      0);                    /* Ядро для выполнения задачи (0) */

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);

  //Characterize ADC at particular atten
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, ESP_ADC_CAL_VAL_DEFAULT_VREF, adc_chars);

  pressuremeter0 = Inputanalog(ADC1_CHANNEL_7, MAXPRESSURE); // GPIO34
  pressuremeter1 = Inputanalog(ADC1_CHANNEL_6, MAXPRESSURE); // GPIO35

  output0 = Out(OUTPUT0, 0);
  output1 = Out(OUTPUT1, &pressuremeter1, 1);
  output2 = Out(OUTPUT2, 2);
  flash.begin("eeprom", false);

  int k = flash.getInt("press0_0", 0); //0 бар датчика 1 из eerom
  pressuremeter0.set0(k);
  k = flash.getInt("press0_200", 4); //200 бар датчика 1 из eerom
  pressuremeter0.set200(k);
  k = flash.getInt("press1_0", 8); //0 бар датчика 2 из eerom
  pressuremeter1.set0(k);
  k = flash.getInt("press1_200", 12); //200 бар датчика 2 из eerom
  pressuremeter1.set200(k);
  // Serial.println("Begun...");
  // Serial.println(incStr);

  incStr = "ESP32 ver: " + String(soft.MODEL_CODE, DEC) + "." + String(soft.FIRMWARE_VERSION, DEC) + "  " + String(soft.DAY_OF_RELEASE + 1, DEC) + "." + String(soft.MONTH_OF_RELEASE + 1, DEC) + "." + String(soft.YEAR_OF_RELEASE, DEC);

  printchek("On", 0);

  for (int i = 0; i < COUNTERGASPRESSURE; i++)
    timeControl += periodTimeArray[i];
  timeControl += COUNTERGASPRESSURE * 60000;

  sensors.begin();

  if (!oneWire.search(kompressorThermometer))
  {
    error.no_found_temp_kompressor = 1;
    Serial.println("Unable to find address for kompressorThermometer");
  }
  else
  {
    Serial.print("Device 0 Address: ");
    Serial.println(kompressorThermometer[0], HEX);
    error.no_found_temp_kompressor = 0;
    error.to_hight_temp_kompressor = 0;
    tickergetkompressorTemperature.attach_ms(100, getTemperature);
    sensors.setResolution(kompressorThermometer, 10);
  }
  // tickergetkompressorTemperature.attach_ms(1000, getTemperature);

  tickerprintDebugLog.attach_ms(1000, printDebugLog);

  xTaskCreatePinnedToCore(
      showNextion,        /* Вывод информации на дисплей Nextion*/
      "Task_showNextion", /* Название задачи */
      10000,              /* Размер стека задачи */
      NULL,               /* Параметр задачи */
      2,                  /* Приоритет задачи */
      &Task_showNextion,  /* Идентификатор задачи, чтобы ее можно было отслеживать */
      0);                 /* Ядро для выполнения задачи (0) */

  xTaskCreatePinnedToCore(
      readNextion,        /* Чтение информации с дисплея Nextion*/
      "Task_readNextion", /* Название задачи */
      10000,              /* Размер стека задачи */
      NULL,               /* Параметр задачи */
      3,                  /* Приоритет задачи */
      &Task_readNextion,  /* Идентификатор задачи, чтобы ее можно было отслеживать */
      0);                 /* Ядро для выполнения задачи (0) */

  pinMode(GPIO_NUM_2, OUTPUT);
}

void loop()
{
  errors();
  if (mode == "p0")
    modeMenu();
  if (mode == "p1")
    modeSetting_auto();
  if (mode == "p3")
    modeManual();
  if (mode == "p2")
    modeAuto();
  if (mode == "p2.1")
    modeAutoPause();
}

void updatepressure(void *pvParameters)
{
  while (true)
  {
    pressuremeter0.update();
    vTaskDelay(111 / portTICK_PERIOD_MS);
    // digitalWrite(GPIO_NUM_2, HIGH);
    pressuremeter1.update();
    vTaskDelay(111 / portTICK_PERIOD_MS);
    digitalWrite(GPIO_NUM_2, !digitalRead(GPIO_NUM_2));
  }
}

//  Режим Меню
void modeMenu()
{
  output0.off();
  output1.off();
  output2.off();
  savePressure = 1;
  counter = 0;
  error.pres_a_leak = 0;
  output1.resetTimeOpen();
  setting_press_manual = MAXPRESSURECOMPRESSOR;
}

//  Режим Ручной контроль
void modeManual()
{
  if (output0.get())
    if (pressuremeter0.getmap() > 2 + pressuremeter1.getmap())
      output1.on();
  // else if (pressuremeter0.getmap() < pressuremeter1.getmap())
  //   output1.off();

  // if (pressuremeter1.getmap_max() >= setting_press_manual + 2 || pressuremeter1.getmap() >= setting_press_manual) // окончание работы компрессора при предустановке давления
  if (pressuremeter1.getmap_max() > setting_press_manual) // окончание работы компрессора при предустановке давления //FIRMWARE_VERSION = 0x15;
  {
    stop_pump();
    setting_press_manual = MAXPRESSURECOMPRESSOR;
  }
  if (output2.get())
    savePressure = pressuremeter1.getmap();
  if ((!output1.get() || !output0.get()) && fsavePressure)
  {
    if (counPause > 1000)
    {
      savePressure = pressuremeter1.getmap();
      timePause = millis();
      fsavePressure = 0;
    }
    else
    {
      delay(10);
      counPause++;
    }
  }
  if (output1.get())
  {
    counPause = 0;
    fsavePressure = 1;
  }
}
//  Режим автоматический контроль
void modeAuto()
{
  if (pressuremeter1.getmap_max() < gasPressurArray[counter] + 2 && !pauseAutoMode) // выключение компрессора при достижении давления контроля.
  {
    output0.on();
    if (pressuremeter0.getmap_max() >= pressuremeter1.getmap_max())
      output1.on();
  }
  else
  {
    pauseAutoMode = true;
    output0.off();
    output1.off();
    if (!output2.get())
      timePauseStart = millis() + 20000;
    output2.on();

    if (millis() > timePauseStart)
    {
      output2.off();
      delay(100);
      output2.on();
      savePressure = pressuremeter1.getmap();
      timePause = millis();
      mode = "p2.1";
      // counPause = 0;
    }
    // else
    // {
    //   delay(10);
    //   counPause++;
    // }
  }
}

//  Режим автоматический контроль - пауза
void modeAutoPause()
{
  output2.on();
  // корректировка оставшегося времени опрессовки
  if (pauseAutoMode)
  {
    if (counter == 3)
      if ((timeControl - (millis() - timeStartControl)) < periodTimeArray[counter])
        timeControl += periodTimeArray[counter] - (timeControl - (millis() - timeStartControl)) + 10000;
  }
  // output0.off();
  // output1.off();
  // output2.on();
  pauseAutoMode = false;
  if (millis() - timePause > periodTimeArray[counter])
  {
    output2.off();
    if (savePressure - pressuremeter1.getmap() < MAXVOLUMEPRESSERROR) // проверка на отсутствие учетки по окончании паузы
    {
      counter++;
      mode = "p2";
      if (counter == COUNTERGASPRESSURE) // окончание опрессовки
      {
        modeAutoEnd();
        return;
      }
      while (pressuremeter0.getmap() < savePressure)
        output0.on();
      output1.setTimeOpen();
    }
    else
      error.pres_a_leak = 1;
  }
}

//  Режим автоматический контроль - окончание опрессовки
void modeAutoEnd()
{
  output0.off();
  printchek("Ok");
  mode = "p2.2";
  output1.on();
  output2.on();
}
void modeSetting_auto()
{
  send("work_auto.t7.txt", "--:--");
  send("waveform.va0.val", 0);
  send("waveform.va1.val", 0);
  while (pressuremeter1.getmap() > 10)
  {
    output1.on();
    output2.on();
  }
  output2.off();
  output1.off();
}

//  Ошибки
void errors()
{
  if (!error.no_found_temp_kompressor)
  {
    if (kompressor_temperature > MAX_TEMPERATURE_KOMPRESSOR)
    {
      if (mode == "p2" || mode == "p3")
        error.to_hight_temp_kompressor = 1;
    }
    if (kompressor_temperature < MIN_TEMPERATURE_KOMPRESSOR)
    {
      error.to_hight_temp_kompressor = 0;
      if (mode == "p2.8")
        mode = "p2";
      if (mode == "p3.8")
        mode = "p3";
    }
  }

  if (pressuremeter0.getmap_max() > MAXPRESSURECOMPRESSOR || pressuremeter1.getmap_max() > MAXPRESSURECOMPRESSOR)
  {
    output0.off();
    output1.off();
  }
  /* ----------------------------- ошибки по датчикам давления ---------------------------*/
  error.pres0_break = pressuremeter0.geterror();
  error.pres0_closure = pressuremeter0.geterror() >> 1;
  error.pres1_break = pressuremeter1.geterror();
  error.pres1_closure = pressuremeter1.geterror() >> 1;

  if (mode == "p2" || mode == "p2.1")
  {
    if (error.pres_a_leak)
    {
      mode = "p2.3";
      printchek("Error", counter);
    }
    if (error.pres0_break || error.pres0_closure || error.pres1_break || error.pres1_closure)
    {
      mode = "p2.5";
      output0.off();
      output1.off();
    }
    if (error.to_hight_temp_kompressor)
    {
      mode = "p2.8";
      output0.off();
      output1.on();
      output2.on();
    }
  }
  if (mode == "p3" || mode == "p4")
  {
    if (error.pres0_break || error.pres0_closure || error.pres1_break || error.pres1_closure)
    {
      if (mode == "p3")
        mode = "p3.5";
      else
        mode = "p4.5";
      output0.off();
    }
    if (error.to_hight_temp_kompressor)
    {
      mode = "p3.8";
      output0.off();
      output1.on();
      output2.on();
    }
  }
  if (mode == "p2" && counter < COUNTERGASPRESSURE)
  {
    if (output1.getTimeOpen() == 3000)
    {
      mode = "p2.7";
      output0.off();
    }
  }
}

void printDebugLog()
{
  // Serial.printf("Mem: %d\n", ESP.getFreeHeap());
  // Serial.printf("Mode - %s\n", mode);
  // Serial.printf("Errors - %d %d | %d %d || %d || %d\n", error.pres0_break, error.pres0_closure, error.pres1_break, error.pres1_closure, error.pres_a_leak, error.pres_off);
  // Serial.printf("Pres1med - %d\n", pressuremeter0.getmedian());
  // Serial.printf("Pres1majority - %d\n", pressuremeter0.majority());
  // Serial.printf("Pres1map - %d\n\n", pressuremeter0.getmap());
  // Serial.printf("Pres2med - %d\n", pressuremeter1.getmedian());
  // Serial.printf("Pres2majority - %d\n", pressuremeter1.majority());
  // Serial.printf("Pres2map - %d\n\n", pressuremeter1.getmap());
  // Serial.printf("VIN - %s\n\n", vin);
  // Serial.printf("DATE - %s\n\n", date);
  // Serial.printf("TIME - %s\n\n", timebegin);
#ifdef USE_WEB_SERIAL
  WebSerial.println("Press:");
  WebSerial.println(pressuremeter0.getmedian());
  WebSerial.println(pressuremeter1.getmedian());
#endif
}

void showNextion(void *pvParameters)
{
  while (true)
  {
    if (mode == "p0")
      showPageNextionMenu(); //  Экран меню

    if (mode == "p2" || mode == "p2.1")
      showPageNextionAutoControl(); //  Экран автоматического контроля

    // вывод сообщения об удачном завершеним контроля
    if (mode == "p2.2")
    {
      //String incStr = "Проверка опрессовки закончена\\r\\r10 бар   - ОК! | 25 бар   - ОК!\\r50 бар   - ОК! | 100 бар - ОК!\\r200 бар - ОК!\\rТест пройден успешно!";
      String incStr = "Проверка опрессовки закончена\\r\\r25 бар   - ОК! | 50 бар   - ОК!\\r100 бар - ОК! | 200 бар - ОК!\\rТест пройден успешно!";
      showText(1032, &incStr);
      send("n1.val", pressuremeter1.getmap()); //FIRMWARE_VERSION = 0x15;
      send("n4.val", pressuremeter1.getmap()); //FIRMWARE_VERSION = 0x15;
    }

    // вывод сообщения об утечке газа
    if (mode == "p2.3")
    {
      String incStr = "Утечка газа в контролируемой системе\\r";
      incStr += "Контрольное давление - " + String(gasPressurArray[counter]) + " бар\\r\\r" + "Необходима повторная опрессовка системы";
      showText(32768, &incStr);
    }

    // вывод сообщения о ручной остановке проверки (вкл. клапана сброса)
    if (mode == "p2.4")
    {
      String incStr = "Проверка опрессовки прервана \\rНеобходимо заново повторить процедуру";
      showText(50712, &incStr);
    }

    // вывод сообщения об остановке при проблемах с датчиками давления
    if (mode == "p2.5")
    {
      String incStr = "Неисправность датчика давления \\rПосле ремонта необходимо повторить процедуру";
      showText(32768, &incStr);
    }

    // вывод сообщения об остановке при недостаточном давлении в рессивере
    if (mode == "p2.6")
    {
      String incStr = "Недостаточно воздуха в рессивере\\rНеобходим наполнить рессивер \\rи заново повторить процедуру";
      showText(32768, &incStr);
    }
    // вывод сообщения о невозможности достижения контрольной точки (большая утечка)
    if (mode == "p2.7")
    {
      String incStr = "Сильная утечка в контролируемой системе\\rНеобходим проверить соединения \\rи заново повторить процедуру";
      showText(32768, &incStr);
    }
    // вывод сообщения о перегреве компрессора
    if (mode == "p2.8")
    {
      String incStr = "Перегрев компрессора!\\rНеобходим перерыв \\rдля охлаждения компрессора";
      showText(32768, &incStr);
    }

    if (mode == "p3")
      showPageNextionManualControl(); //   Экран ручного контроля

    // вывод сообщения о неисправностях датчиков давления
    if (mode == "p3.5")
    {
      String incStr = "Неисправность датчиков давления\\rНеобходим ремонт";
      showText(32768, &incStr);
    }
    // вывод сообщения о перегрева компрессора
    if (mode == "p3.8")
    {
      String incStr = "Перегрев компрессора!\\rНеобходим перерыв \\rдля охлаждения компрессора";
      showText(32768, &incStr);
    }

    //   Экран графика
    if (mode == "p4")
    {
      send("waveform.va0.val", constrain(map(savePressure, 1, MAXPRESSURE, 0, 100), 0, 100));
      send("waveform.va1.val", constrain(map(pressuremeter1.getmap(), 1, MAXPRESSURE, 0, 100), 0, 100));
      send("waveform.t2.txt", String(savePressure) + " бар");
      send("waveform.t3.txt", String(pressuremeter1.getmap()) + " бар");
    }

    if (mode == "p4.5")
      send("page work_manual");

    //   Экран настройки
    if (mode == "p5")
    {
      send("n0.val", pressuremeter0.getmap());
      send("n1.val", pressuremeter1.getmap());
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// Экран меню
void showPageNextionMenu()
{
  String erpress, ercomp;
  if (pressuremeter0.geterror())
  {
    send("vis t2,1");
    send("vis g0,1");
    send("menu.p0.pic", 12);
    send("vis t4,0");
    send("menu.j0.val", 0);
    if (pressuremeter0.geterror() == 1)
      send("t2.txt", "обрыв");
    if (pressuremeter0.geterror() == 2)
      send("t2.txt", "замыкание");
    erpress = "необходим ремонт датчика давления ";
    // send("g0.txt", "необходим ремонт");
  }
  else
  {
    send("vis t4,1");
    send("menu.t4.txt", String(pressuremeter0.getmap()) + " бар");
    send("menu.j0.val", constrain(map(pressuremeter0.getmap(), 0, MAXPRESSURE, 20, 90), 20, 100));
    send("vis t2,0");
    send("menu.p0.pic", 11);
  }
  if (pressuremeter1.geterror())
  {
    send("vis t3,1");
    send("vis g0,1");
    send("menu.p1.pic", 12);
    if (pressuremeter1.geterror() == 1)
      send("t3.txt", "обрыв");
    if (pressuremeter1.geterror() == 2)
      send("t3.txt", "замыкание");
    erpress = "необходим ремонт датчика давления ";
  }
  else
  {
    send("vis t3,0");
    send("menu.p1.pic", 11);
  }

  if (kompressor_temperature > MIN_TEMPERATURE_KOMPRESSOR)
  {
    send("vis g0,1");
    send("menu.t0.pco", 32768);
    ercomp = " высокая температура компрессора";
  }

  if (kompressor_temperature < MIN_TEMPERATURE_KOMPRESSOR * 0.9)
    send("menu.t0.pco", 50712);
  if (error.no_found_temp_kompressor)
  {
    send("vis g0,1");
    ercomp = " неисправность датчика температуры";
    send("menu.t0.txt", "---");
  }
  else
    send("menu.t0.txt", String(kompressor_temperature, 1) + " C");
  if (pressuremeter0.geterror() || pressuremeter1.geterror() || error.to_hight_temp_kompressor || error.no_found_temp_kompressor || kompressor_temperature > MIN_TEMPERATURE_KOMPRESSOR * 1.2)
    send("g0.txt", erpress + ercomp);

  // if (pressuremeter0.getmap() < gasPressurArray[COUNTERGASPRESSURE - 1] + 5 && !pressuremeter0.geterror() && !pressuremeter1.geterror())
  // {
  //   send("menu.t4.pco", 32768);
  //   send("vis g0,1");
  //   send("g0.txt", "давление в рессивере < " + String(gasPressurArray[COUNTERGASPRESSURE - 1] + 5) + " бар, контроль опрессовки невозможен");
  // }
  // else
  // {
  // send("menu.t4.pco", 38495);

  // if (!(pressuremeter0.geterror() || pressuremeter1.geterror() || error.to_hight_temp_kompressor || error.no_found_temp_kompressor))
  else
    send("g0.txt", "");
  // }
}

//  Экран автоматического контроля
void showPageNextionAutoControl()
{
  if (!error.no_found_temp_kompressor)
  {
    // getTemperature();
    send("work_auto.t1.txt", String(kompressor_temperature, 1) + " C");
  }
  else
    send("work_auto.t1.txt", "---");
  send("j0.val", constrain(map(pressuremeter0.getmap(), 0, MAXPRESSURE, 20, 90), 20, 100));
  send("n0.val", gasPressurArray[counter]);
  send("n1.val", pressuremeter1.getmap());
  send("n4.val", pressuremeter1.getmap());
  send("n3.val", pressuremeter0.getmap());

  // mode == "p2" ? send("n2.val", pressuremeter1.getmap()) : send("n2.val", savePressure);
  send("n2.val", savePressure);

  if (output0.get())
  {
    send("work_auto.j0.bpic", 17);
    send("work_auto.j0.ppic", 18);
  }
  else
  {
    send("work_auto.j0.bpic", 7);
    send("work_auto.j0.ppic", 8);
  }

  output1.get() ? send("work_auto.p0.pic", 10) : send("work_auto.p0.pic", 9);
  output2.get() ? send("work_auto.p1.pic", 10) : send("work_auto.p1.pic", 9);
  send("waveform.va0.val", constrain(map(savePressure, 1, MAXPRESSURE, 1, 100), 1, 100));
  send("waveform.va1.val", constrain(map(pressuremeter1.getmap(), 1, MAXPRESSURE, 1, 100), 1, 100));

  int sekund = ((timeControl - (millis() - timeStartControl)) % 60000 / 1000);
  int min = (timeControl - (millis() - timeStartControl)) / 60000;
  if (millis() - timeStartControl > timeControl)
  {
    sekund = 0;
    min = 0;
  }
  send("j1.val", constrain(map(timeControl - (millis() - timeStartControl), 0, timeControl, 100, 0), 0, 100));
  String time = "";
  if (sekund > 9)
    time = String(min) + ":" + String(sekund);
  else
    time = String(min) + ":0" + String(sekund);
  send("work_auto.t7.txt", time);

  if (savePressure - pressuremeter1.getmap() >= MAXVOLUMEPRESSERROR)
  {
    send("n1.pco", 32768);
    send("work_auto.s0.pco1", 32768);
  }
  else
  {
    send("n1.pco", 1024);
    send("work_auto.s0.pco1", 1024);
  }
}

//  Экран ручного контроля
void showPageNextionManualControl()
{
  send("j0.val", constrain(map(pressuremeter0.getmap(), 0, MAXPRESSURE, 20, 90), 20, 100));
  send("n2.val", savePressure);
  send("n1.val", pressuremeter1.getmap());
  send("n3.val", pressuremeter0.getmap());
  send("n4.val", pressuremeter1.getmap());
  if (!error.no_found_temp_kompressor)
    send("work_manual.t1.txt", String(kompressor_temperature, 1) + " C");
  else
    send("---");

  if (output0.get() || startPump)
  {
    send("work_manual.j0.bpic", 17);
    send("work_manual.j0.ppic", 18);
  }
  else
  {
    send("work_manual.j0.bpic", 7);
    send("work_manual.j0.ppic", 8);
  }
  output1.get() ? send("work_manual.p0.pic", 10) : send("work_manual.p0.pic", 9);
  output2.get() ? send("work_manual.p1.pic", 10) : send("work_manual.p1.pic", 9);

  send("waveform.va0.val", constrain(map(savePressure, 1, MAXPRESSURE, 1, 100), 1, 100));
  send("waveform.va1.val", constrain(map(pressuremeter1.getmap(), 1, MAXPRESSURE, 1, 100), 1, 100));
}

// данные с Nextion
void readNextion(void *pvParameters)
{
  char buf;
  while (true)
  {
    if (Serial1.available())
    {
      buf = Serial1.read();
      if (buf == 0x23)
      {
        char arr[4];
        Serial1.readBytes(arr, 4);
        NextionRS232.id = arr[1] | arr[0] << 8;
        NextionRS232.val = arr[2] | arr[3] << 8;
        //  Serial.printf("id - %x\n", NextionRS232.id);
        //  Serial.printf("val - %x\n", NextionRS232.val);
        analyseDate(NextionRS232);
      }
      else if (buf == 0x24)
      {
        String incStr = "";
        incStr = Serial1.readStringUntil(0x0A);
        // Serial.print(incStr);
        analyseString(incStr);
      }
    }
    vTaskDelay(100);
  }
}

//парсинг полученых текстовых данных от дисплея Nextion
void analyseString(String &incStr)
{
  for (int i = 0; i < incStr.length(); i++)
  {
    if (incStr.substring(i).startsWith("vin")) // получение номера
      vin = incStr.substring(3, incStr.length());
    if (incStr.substring(i).startsWith("date")) // получение даты
      date = incStr.substring(4, incStr.length());
    if (incStr.substring(i).startsWith("time")) // получение времени начала контроля
      timebegin = incStr.substring(4, incStr.length());
  }
}

//парсинг полученых данных от дисплея Nextion
void analyseDate(NextRS232 &dataRS232)
{
  int k;
  switch (dataRS232.id)
  {
  case 0x0000: //  меню
    mode = "p0";
    send("wav0.en", 1);
    break;
  case 0x0100: // настройка автомат
    mode = "p1";
    send("wav0.en", 1);
    break;
  case 0x0200: // включение автоматического режима контроля
    mode = "p2";
    timeStartControl = millis();
    send("wav0.en", 1);
    break;
  case 0x0300: // включение ручного режима
    mode = "p3";
    send("wav0.en", 1);
    break;
  case 0x0400: // включение большого графика
    mode = "p4";
    send("wav0.en", 1);
    break;
  case 0x0500: // настройка
    mode = "p5";
    send("wav0.en", 1);
    break;

  case 0x0302: //  управление компрессором
    setting_press_manual = MAXPRESSURECOMPRESSOR;
    if (output0.get())
      stop_pump();
    else
    {
      manualpressuresetting(setting_press_manual);
    }
    break;

  case 0x030C: //  предустановка 25 бар
               // if (setting_press_manual == MAXPRESSURECOMPRESSOR || pressuremeter1.getmap() > 25)
    if (!output0.get())
    {
      manualpressuresetting(25);
    }
    else
      setting_press_manual = 25;
    delay(50);
    send("b0.val", 1);
    send("b1.val", 0);
    send("b2.val", 0);
    send("b3.val", 0);
    break;

  case 0x030D: //  предустановка 50 бар
    if (!output0.get())
    {
      manualpressuresetting(50);
    }
    else
      setting_press_manual = 50;
    delay(50);
    send("b0.val", 0);
    send("b1.val", 1);
    send("b2.val", 0);
    send("b3.val", 0);
    break;

  case 0x030E: //  предустановка 150 бар
    if (!output0.get())
    {
      manualpressuresetting(100);
    }
    else
      setting_press_manual = 100;
    delay(50);
    send("b0.val", 0);
    send("b1.val", 0);
    send("b2.val", 1);
    send("b3.val", 0);
    break;

  case 0x030F: //  предустановка 200 бар
    if (!output0.get())
    {
      manualpressuresetting(200);
    }
    else
      setting_press_manual = 200;
    delay(50);
    send("b0.val", 0);
    send("b1.val", 0);
    send("b2.val", 0);
    send("b3.val", 1);
    break;

  case 0x0304: //  управление основным клапаном
    output1.get() ? output1.off() : output1.on();
    send("wav0.en", 1);
    break;
  case 0x0305: // управление клапаном сброса
    if (output2.get())
    {
      output2.off();
    }
    else
    {
      output2.on();
    }
    send("wav0.en", 1);
    break;
  case 0x050A: // калибровка датчиков давления 1 бар
    flash.putInt("press0_0", pressuremeter0.getmedian10());
    k = flash.getInt("press0_0", 0);
    pressuremeter0.set0(k);
    flash.putInt("press1_0", pressuremeter1.getmedian10());
    k = flash.getInt("press1_0", 0);
    pressuremeter1.set0(k);
    send("wav0.en", 1);
    break;
  case 0x050B: // калибровка датчиков давления 200 бар
    flash.putInt("press0_200", pressuremeter0.getmedian10());
    k = flash.getInt("press0_200", 0);
    pressuremeter0.set200(k);
    flash.putInt("press1_200", pressuremeter1.getmedian10());
    k = flash.getInt("press1_200", 0);
    pressuremeter1.set200(k);
    send("wav0.en", 1);
    break;

  case 0x031B: //  управление компрессором при калибровки датчиков давления
    output0.get() ? output0.off() : start_pump();
    break;
  }
}
void showText(int color, String *incStr)
{
  send("tm0.en", 0);
  send("vis t0,1");
  send("t0.pco", color);
  send("t0.txt", *incStr);
  output1.get() ? send("work_auto.p0.pic", 10) : send("work_auto.p0.pic", 9);
  output2.get() ? send("work_auto.p1.pic", 10) : send("work_auto.p1.pic", 9);
}
// отправка на Nextion
void send(String dev)
{
  Serial1.print(dev);
  Serial1.write(0xff);
  Serial1.write(0xff);
  Serial1.write(0xff);
}
void send(String dev, double data)
{
  Serial1.print(dev + "=");
  Serial1.print(data, 0);
  Serial1.write(0xff);
  Serial1.write(0xff);
  Serial1.write(0xff);
}
void send(String dev, String data)
{
  Serial1.print(dev + "=\"" + data + "\"");
  Serial1.write(0xff);
  Serial1.write(0xff);
  Serial1.write(0xff);
}

void printchek(String inStr, int counter)
{
#ifndef PRINTER_RUS
  send("wav1.en", 1);
  if (!printer.hasPaper())
    return;
  printer.justify('C');
  if (inStr == "On")
  {
    printer.println();
    IPAddress ip_address = WiFi.localIP();
    if (ip_address.toString() != "0.0.0.0")
    {
      printer.println(ip_address);
      printer.println(WiFi.macAddress());
    }
    printer.println("www.italgas.ru");
  }

  if (inStr != "On")
  {
    printerPrintLine(30);
    printer.print(timebegin);
    printer.println("   ");
    printer.println(date);
    printer.setSize('L');
    printer.println(vin);
    printer.setSize('S');
    printer.justify('L');
    printerPrintLine(30);
  }
  if (inStr == "Ok")
  {
    printer.println(F("25 bar   - OK"));
    printer.println(F("50 bar   - OK"));
    printer.println(F("100 bar  - OK"));
    printer.println(F("200 bar  - OK"));
    printer.justify('C');
    printer.setSize('M');
    printer.println(F("**** TEST RESULT - OK ****"));
  }
  if (inStr == "Error")
  {
    switch (counter)
    {
    case 0:
      printer.println(F("25 bar  - ERROR!"));
      break;
    case 1:
      printer.println(F("25 bar  - OK"));
      printer.println(F("50 bar  - ERROR!"));
      break;
    case 2:
      printer.println(F("25 bar  - OK"));
      printer.println(F("50 bar  - OK"));
      printer.println(F("100 bar - ERROR!"));
      break;
    case 3:
      printer.println(F("25 bar  - OK"));
      printer.println(F("50 bar  - OK"));
      printer.println(F("100 bar - OK"));
      printer.println(F("200 bar - ERROR!"));
      break;
    }
    printer.justify('C');
    printer.setSize('M');
    printer.println(F("** REPEAT PRESSING **"));
  }
  printer.setSize('S');
  printer.justify('L');
  printerPrintLine(30);
  String str = "ESP32 ver: " + String(soft.MODEL_CODE, DEC) + "." + String(soft.FIRMWARE_VERSION, DEC) + "  " + String(soft.DAY_OF_RELEASE + 1, DEC) + "." + String(soft.MONTH_OF_RELEASE + 1, DEC) + "." + String(soft.YEAR_OF_RELEASE, DEC);
  printer.println(str);
  printer.setDefault();
  printer.justify('L');
  str = String(pressuremeter0.get0()) + " " + String(pressuremeter0.get200()) + " | " + String(pressuremeter1.get0()) + " " + String(pressuremeter1.get200());
  printer.println(str);
  printerPrintLine(30);
#endif

  // принтер с поддержкой русского языка и печати растровых картинок
#ifdef PRINTER_RUS
  send("wav1.en", 1);
  printer.wake();
  delay(1000);
  if (!printer.hasPaper())
    return;
  printer.justify('C');
  printer.setCodePage(CODEPAGE_ISO_8859_5);
  printer.setLineHeight(33);
  printer.setSize('M');
  printer.printBitmap(368, 72, canvas_logo_hor);
  printer.setSize('S');
  printer.println("www.italgas.ru");

  if (inStr != "On")
  {
    printer.justify('C');
    printerPrintLine(30);
    printer.print(timebegin + " | ");
    printer.println(date);
    printer.setSize('L');
    printer.print(vin);
    printer.feedRows(15);
    printer.setSize('S');
    printer.justify('L');

    printerPrintLine(30);
  }
  if (inStr == "Ok")
  {
    send("wav1.en", 1);
    printer.println(F("РЕЗУЛЬТАТ ТЕСТА:"));
    printer.feedRows(20);
    printer.println(F("25 бар   - успешно"));
    printer.println(F("50 бар   - успешно"));
    printer.println(F("100 бар  - успешно"));
    printer.println(F("200 бар  - успешно"));
    printer.justify('C');
    printer.setSize('M');
    printer.feedRows(10);
    printer.println(F("ТЕСТ ПРОЙДЕН УСПЕШНО"));
  }
  if (inStr == "Error")
  {
    printer.println(F("РЕЗУЛЬТАТ ТЕСТА:"));
    printer.feedRows(20);
    switch (counter)
    {
    case 0:
      printer.println(F("25 бар  - УТЕЧКА!"));
      break;
    case 1:
      printer.println(F("25 бар  - успешно"));
      printer.println(F("50 бар  - УТЕЧКА!"));
      break;
    case 2:
      printer.println(F("25 бар  - успешно"));
      printer.println(F("50 бар  - успешно"));
      printer.println(F("100 бар - УТЕЧКА!"));
      break;
    case 3:
      printer.println(F("25 бар  - успешно"));
      printer.println(F("50 бар  - успешно"));
      printer.println(F("100 бар - успешно"));
      printer.println(F("200 бар - УТЕЧКА!"));
      break;
    }
    printer.justify('C');
    printer.setSize('M');
    printer.feedRows(10);
    printer.println(F("НЕОБХОДИМА ПОВТОРНАЯ ОПРЕССОВКА"));
  }
  printer.setSize('S');
  printer.justify('L');
  printerPrintLine(30);
  String str = "ESP32 ver: " + String(soft.MODEL_CODE, DEC) + "." + String(soft.FIRMWARE_VERSION, DEC) + "  " + String(soft.DAY_OF_RELEASE + 1, DEC) + "." + String(soft.MONTH_OF_RELEASE + 1, DEC) + "." + String(soft.YEAR_OF_RELEASE, DEC);
  printer.println(str);
  printer.setDefault();
  printer.justify('L');
  str = String(pressuremeter0.get0()) + " " + String(pressuremeter0.get200()) + " | " + String(pressuremeter1.get0()) + " " + String(pressuremeter1.get200());
  printer.println(str);
  printerPrintLine(30);
  if (inStr != "On")
  {
    IPAddress ip_address = WiFi.softAPIP();
    printer.println(ip_address.toString());
  }
  printer.feed(3);
  printer.sleep();
#endif
}

void printerPrintLine(int n)
{
  // printer.setCodePage(CODEPAGE_CP866);
  for (int i = 0; i < n; i++)
    // printer.write(0xC4);
    printer.print("-");
  // printer.setCodePage(CODEPAGE_ISO_8859_5);
  printer.feed(1);
}

void manualpressuresetting(int _setting_press_manual)
{
  send("wav0.en", 1);
  send("b0.val", 0);
  send("b1.val", 0);
  send("b2.val", 0);
  send("b3.val", 0);
  output0.off();
  if (pressuremeter1.getmap_max() > _setting_press_manual)
  {
    while (pressuremeter1.getmap_max() > _setting_press_manual + 5)
    {
      output1.on();
      output2.on();
      delay(300);
      output1.off();
      output2.off();
      delay(2000);
    }
    setting_press_manual = MAXPRESSURECOMPRESSOR;
    return;
  }
  else
  {
    setting_press_manual = _setting_press_manual;
    start_pump();
    return;
  }
}

void start_pump()
{
  startPump = true;
  output1.off();
  while (pressuremeter0.getmap() > 2)
  {
    output2.on();
    yield();
    delay(200);
  }
  output0.on();
  startPump = false;
  output2.off();
}
void stop_pump()
{
  output0.off();
  output1.off();
  delay(1000);
  output2.on();
  send("wav0.en", 1);
  send("b0.val", 0);
  send("b1.val", 0);
  send("b2.val", 0);
  send("b3.val", 0);
}

void getTemperature()
{
  float tempC = sensors.getTempC(kompressorThermometer);
  Serial.println(tempC);
  sensors.requestTemperatures();
  if (tempC == DEVICE_DISCONNECTED_C)
  {
    counterErrorConnectTemper++;
    if (counterErrorConnectTemper > 30)
      error.no_found_temp_kompressor = 1;
    return;
  }
  counterErrorConnectTemper--;
  if (counterErrorConnectTemper < 20)
  {
    counterErrorConnectTemper = 0;
    error.no_found_temp_kompressor = 0;
  }
  kompressor_temperature = tempC;
}

#ifdef USE_WEB_SERIAL
void recvMsg(uint8_t *data, size_t len)
{
  WebSerial.println("Received Data...");
  String d = "";
  for (int i = 0; i < len; i++)
  {
    d += char(data[i]);
  }
}
#endif
