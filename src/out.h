#include <Arduino.h>

static int gasPressurArray[4] = {25, 50, 100, 200};

class Out
{
private:
#define ON HIGH
#define OFF LOW
    int _pin, _oldpress, _time_open = 100, _counter, _canalPWM;
    int8_t _state;
    unsigned long time = 0;
    Inputanalog *_pres;

public:
    Out() {}
    Out(int pin, Inputanalog *pres, int canalPWM) : _pin(pin), _canalPWM(canalPWM) //конструктор для выходов на плате + датчик давления
    {
        // pinMode(pin, OUTPUT);
        // digitalWrite(_pin, OFF);
        _pres = pres;
        ledcSetup(canalPWM, 200, 8);
        ledcAttachPin(_pin, _canalPWM);
    }
    Out(int pin, int canalPWM) : _pin(pin), _canalPWM(canalPWM) //конструктор для выходов на плате
    {
        // pinMode(pin, OUTPUT);
        // digitalWrite(_pin, OFF);
        ledcSetup(canalPWM, 200, 8);
        ledcAttachPin(_pin, _canalPWM);
    }
    //возращение статуса
    int get()
    {
        // return digitalRead(_pin);
        return _state;
    }
    // включение
    void on()
    {
        ledcWrite(_canalPWM, 256);
        delay(200);
        ledcWrite(_canalPWM, 192);
        _state = 1;

        // digitalWrite(_pin, ON);
    }
    //выключение
    void off()
    {
        ledcWrite(_canalPWM, 0);
        _state = 0;
        // digitalWrite(_pin, OFF);
    }
    void resetTimeOpen()
    {
        _time_open = 100;
        _counter = 0;
    }
    void setTimeOpen()
    {
        _time_open += 200;
        _counter++;
    }
    int getTimeOpen()
    {
        return _time_open;
    }
    void onpulse(int _counter)
    {
        if (millis() > time)
        {
            if (get() == 0)
            {
                if (_pres->getmap() - _oldpress < 10)
                    _time_open += 100;
                else if ((_pres->getmap() - _oldpress > 12) || (_pres->getmap() > gasPressurArray[_counter] * 0.75))
                    _time_open > 200 ? _time_open -= 100 : _time_open;
                time = millis() + _time_open;
                _oldpress = _pres->getmap();
                on();
            }
            else
            {
                off();
                time = millis() + 1500;
            }
        }
    }
};