#include <Arduino.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"
class Inputanalog
{
private:
#define ON HIGH
#define OFF LOW
    adc1_channel_t _canal;
    int _val, _error, _median, _newest, _recent, _oldest, _maxpress, _press0 = 0, _press200 = 4095, _i = 0;
    static const int COUNTARRAY = 32;
    int _arraymedian[COUNTARRAY];

    // float _k = 0;
    float _k = 0.4; //FIRMWARE_VERSION = 0x15;

    int median_of_3(int a, int b, int c) //медианное усреднение
    {
        int _the_max = max(max(a, b), c);
        int _the_min = min(min(a, b), c);
        int _the_median = _the_max ^ _the_min ^ a ^ b ^ c;
        return _the_median;
    }
    //ошибки
    void error()
    {
        if (getmedian10() < 10)
            _error = 1; // обрыв датчика
        else if (get() > 4000)
            _error = 2; // показания датчика выше нормы (замыкание на питание)
        else
            _error = 0;
    }

public:
    Inputanalog() {}
    Inputanalog(adc1_channel_t canal, int maxpress) : _canal(canal), _maxpress(maxpress) //конструктор для каналов ADC
    {
        adc1_config_width(ADC_WIDTH_12Bit);
        adc1_config_channel_atten(_canal, ADC_ATTEN_11db);
    }
    //возращение значения
    int get()
    {
        return _val;
    }
    //возвращение медианного значения
    int getmedian()
    {
        return _median;
    }
    //возвращение усредненного медианного значения
    int getmedian10()
    {
        int _mp = 0;
        for (int i = 0; i < COUNTARRAY; i++)
        {
            _mp += _arraymedian[i];
            // Serial.print(_arraymedian[i]);
            // Serial.print(" ");
        }
        // Serial.println();
        return _mp >> 5;
    }
    //возвращение усредненного значения в барах
    int getmap()
    {
        // return constrain(map(majority(), _press0, _press200, 1, 200), 1, _maxpress);
        return constrain(map(getmedian10(), _press0, _press200, 1, 200), 1, _maxpress);
    }
    //возвращение пикового значения в барах
    int getmap_max()
    {
        // return constrain(map(majority(), _press0, _press200, 1, 200), 1, _maxpress);
        return constrain(map(getmedian(), _press0, _press200, 1, 200), 1, _maxpress);
    }

    int majority() //поиск наиболее часто встречающегося значения
    {
        int confidence = 0;
        int candidate = 0;
        for (int i = 0; i < COUNTARRAY; i++)
        {
            if (confidence == 0)
            {
                candidate = _arraymedian[i];
                confidence++;
            }
            else if (candidate == _arraymedian[i])
                confidence++;
            else
                confidence--;
        }
        return confidence? candidate: getmedian10();
    }

    //возвращение кода ощибки
    int geterror()
    {
        return _error;
    }
    void setV(int v)
    {
        _val = v;
    }
    //калибровка датчика - 1 бар
    void set0(int press)
    {
        _press0 = press;
    }
    int get0()
    {
        return _press0;
    }
    //калибровка датчика - 200 бар
    void set200(int press)
    {
        _press200 = press;
    }
    int get200()
    {
        return _press200;
    }
    //обновление показаний
    void update()
    {
        _val = adc1_get_raw(_canal);
        _oldest = _recent;
        _recent = _newest;
        _newest = _val;
        _median = _median * _k + (1 - _k) * median_of_3(_oldest, _recent, _newest);
        //_median = median_of_3(_oldest, _recent, _newest);
        _arraymedian[_i] = _median;
        _i == COUNTARRAY ? _i = 0 : _i++;
        error();
    }
};