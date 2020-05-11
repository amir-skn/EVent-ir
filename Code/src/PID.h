#include <Arduino.h>
class PID
{
private:
    float KI;
    float KP;
    float KD;
    float error=0;
    float errorPre=0;
    float integral=0;
    float derivative=0;
    float realPidVal=0;
    int   minOutput=0;
    int   maxOutput=255;
    float timeStep=5e-3; //Unit is in Seconds
public:
    PID(float KP, float KI, float KD);
    int   Calc(float desired, float pv);
    void  setTimeStep(float timeStep);
    float getTimeStep();
    float getError();
    float getPidRealVal();
    void  setOutputRange(int min, int max);
    float limitOutput(float input);
    void  resetParams();
};
