#include <SoftwareSerial.h>                                                                                                                //  Подключаем библиотеку для работы по программной шине UART
#include <Adafruit_Thermal.h>                                                                                                              //  Подключаем библиотеку для работы с принтером
SoftwareSerial  mySerial(5, 6);                                                                                                            //  Объявляем объект библиотеки SoftwareSerial, указывая задействованные выводы Arduino (RX=5-зелёный, TX=6-синий)
Adafruit_Thermal printer(&mySerial);                                                                                                       //  Объявляем объект библиотеки Adafruit_Thermal, указывая ссылку на созданный ранее объект библиотеки SoftwareSerial
#include "logo_iarduino.h"                                                                                                                 //  Подключаем файл с массивом для печати логотипа
//  Функция преобразования русских символов из формата UTF-8 в формат CP866                                                                //
char* RUS(char* str){                                                                                                                      //  Определяем функцию которая преобразует код русских символов из кодировки UTF-8 в кодировку CP866
    uint8_t i=0, j=0;                                                                                                                      //  Определяем переменные: i - счетчик входящих символов, j - счетчик исходящих символов
    while(str[i]){                                                                                                                         //  Проходим по всем символам строки str, пока не встретим символ конца строки (код 0)
        if(uint8_t(str[i]) == 0xD0 && uint8_t(str[i+1]) >= 0x90 && uint8_t(str[i+1]) <= 0xBF ){str[j] = (uint8_t) str[i+1]-0x10; i++;}else //  Символы «А-Я а-п» (код UTF-8: D090-D0AF D0B0-D0BF) сохраняем в кодировке CP866: код 80-9F A0-AF (символ занимал 2 байта, а стал занимать 1 байт)
        if(uint8_t(str[i]) == 0xD1 && uint8_t(str[i+1]) >= 0x80 && uint8_t(str[i+1]) <= 0x8F ){str[j] = (uint8_t) str[i+1]+0x60; i++;}else //  Символы «р-я»     (код UTF-8: D180-D18F)           сохраняем в кодировке CP866: код E0-EF       (символ занимал 2 байта, а стал занимать 1 байт)
        if(uint8_t(str[i]) == 0xD0 && uint8_t(str[i+1]) == 0x81                              ){str[j] = 0xF0;                    i++;}else //  Символ «Ё»        (код UTF-8: D081)                сохраняем в кодировке CP866: код F0          (символ занимал 2 байта, а стал занимать 1 байт)
        if(uint8_t(str[i]) == 0xD1 && uint8_t(str[i+1]) == 0x91                              ){str[j] = 0xF1;                    i++;}else //  Символ «ё»        (код UTF-8: D191)                сохраняем в кодировке CP866: код F1          (символ занимал 2 байта, а стал занимать 1 байт)
                                                                                              {str[j] = (uint8_t) str[i];}  j++; i++;      //  Остальные символы оставляем как есть, без преобразования, но их место в строке могло сдвинуться, если до них были встречены русские символы
    }   while(j<i){str[j]=0; j++;} return str;                                                                                             //  Так как место занимаемое символами в строке могло уменьшиться, заполняем оставщиеся байты символами конца строки (код 0)
}                                                                                                                                          //
                                                                                                                                           //
void setup(){                                                                                                                              //
    mySerial.begin(9600);                                                                                                                  //  Инициируем передачу данных по программной шине UART на скорости 9600. Функцию begin объекта mySerial нужно вызвать до вызова функции begin объекта printer!
    printer.begin();                                                                                                                       //  Инициируем работу с термопринтером. В качестве параметра можно указать время нагрева пикселей от 3 (0,03 мс) до 255 (2,55 мс), чем выше тем темнее пикселы. Значение по умолчанию = 120 (1,20 мс)
    printer.setCodePage(CODEPAGE_CP866);                                   printer.println(    "________________________________" );       //  Печатаем строку из нижних подчёркиваний, предварительно установив кодовую страницу CP866 с поддержкой кириллицы.
    printer.boldOn();  printer.setSize('L'); printer.justify('C');         printer.println(RUS("Термопринтер"                    ));       //  Печатаем текст установив полужирное начертание, крупный размер шрифта с выравниванием по центру
    printer.boldOff(); printer.setSize('S'); printer.justify('L');         printer.println(RUS("технология прямой термопечати на"));       //  Печатаем текст отключив полужирное начертнание и установив маленький размер шрифт с выравниванием по левому краю
                                                                           printer.println(RUS("чек. лентах из термальной бумаги"));       //  Печатаем текст, предыдущие настройки (нормальное начертание, маленький размер и выравнивание по левому краю) сохраняется
    printer.setCodePage(CODEPAGE_KATAKANA);                                printer.println(RUS("ББББББББББББББББББББББББББББББББ"));       //  Печатаем строку из символов «Б», но так как мы предварительно установили кодовую страницу KATAKANA, то вместо символа «Б» напечатается символ напоминающий жирное подчёркивание
    printer.setCodePage(CODEPAGE_CP866);                                   printer.println(RUS("Напряжение питания: 5-9В до 1,5А"));       //  Печатаем текст предварительно установив кодовую страницу CP866 с поддержкой кириллицы.
                                                                           printer.println(RUS("Интерфейс: TTL UART 9600 бит/сек"));       //  Печатаем текст
                                                                           printer.println(RUS("Скорость печати: до 80 мм/с"     ));       //  Печатаем текст
                                                                           printer.println(RUS("Размер пикселя: 1/8 мм"          ));       //  Печатаем текст
                                                                           printer.println(RUS("Разрешение: 203DPI (384 т/лин)"  ));       //  Печатаем текст
                                                                           printer.println(RUS("Ширина чековой ленты: 57 мм"     ));       //  Печатаем текст
                                                                           printer.println(RUS("Печать: текст, штрих-коды, растр"));       //  Печатаем текст
                                                                           printer.println(RUS("Таблицы: ASCII, набор GB2312-80" ));       //  Печатаем текст
                                                                           printer.println(RUS("Штрих-коды: 11 форматов"         ));       //  Печатаем текст
                                                                           printer.println(RUS("Растр: прямая печать изображений"));       //  Печатаем текст
    printer.setCodePage(CODEPAGE_KATAKANA);                                printer.println(RUS("ББББББББББББББББББББББББББББББББ"));       //  Печатаем строку из символов «Б», но так как мы предварительно установили кодовую страницу KATAKANA, то вместо символа «Б» напечатается символ напоминающий жирное подчёркивание
    printer.setCodePage(CODEPAGE_CP866);                                   printer.println(RUS("Таблица символов CODEPAGE_CP866:"));       //  Печатаем текст предварительно установив кодовую страницу CP866 с поддержкой кириллицы.
    printer.feed(); printer.tab(); printer.underlineOn(2);                 printer.println(RUS("    01234567 89ABCDEF"           ));       //  Печатаем текст предварительно прокрутив ленту на одну строку, установив табуляцию (отступ от левого края) и начертание текста с подчёриванием
    for(uint8_t i=0x2; i<=0xF; i++){printer.tab(); printer.underlineOff(); printer.print  (i,HEX); printer.print(" - ");                   //  Выполняем цикл от 2 до 15 включительно (для вывода строк с символами из установленной таблицы символов CP866)
    for(uint8_t j=0x0; j<=0xF; j++){printer.write(i*16+j); if(j==0x7){     printer.print  (" ");}} printer.feed();}  printer.feed();       //  Выполняем цикл от 0 до 15 включительно (для вывода самих символов в очередной строке)
    printer.setCodePage(CODEPAGE_KATAKANA);                                printer.println(RUS("ББББББББББББББББББББББББББББББББ"));       //  Печатаем строку из символов «Б», но так как мы предварительно установили кодовую страницу KATAKANA, то вместо символа «Б» напечатается символ напоминающий жирное подчёркивание
    printer.feed(); printer.begin(255);                                    printer.printBitmap(320,68, logo_print); printer.begin();       //  Печатаем изображение (логотипа iarduino) размером 32x20 пикселей из массива logo_print предварительно изменив время нагрева пикселей до 2,55 мс, а потом установив его обратно в значение по умолчанию
    printer.setCodePage(CODEPAGE_CP866);                                   printer.feed();                                                 //  Устанавливаем кодовую страницу CP866 с поддержкой кириллицы (так как она сбросилась после вызова функций begin) и прокручиваем чек на 1 строку
    printer.justify('C'); printer.doubleHeightOn();                        printer.println(RUS("СПАСИБО ЗА ПОКУПКУ!"             ));       //  Печатаем текст предварительно установив выравнивание по центру и двойную высоту шрифта
    printer.setDefault(); printer.feed(3);                                                                                                 //  Устанавливаем настройки принтера по умолчанию и прокручиваем ленту на 3 строки
}                                                                                                                                          //
                                                                                                                                           //
void loop(){}                                                                                                                              //



