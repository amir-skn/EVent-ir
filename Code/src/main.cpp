// by Amir Sk , MHG, MHM, ASM
// Apr. 5rd 2020

#include <Arduino.h>
#include <configuration.h>
#include <avr/wdt.h>
#include <openGLCD.h>
#include <sysconfig.h>
#include <motor.h>
#include <motor_driver.h>
#include <buzzer.h>
#include <button.h>
#include <led.h>
#include <Potentiometer.h>
#include <LCD.h>
#include <timers.h>
#include <callBacks.h>
#include <PID.h>
#include <trajectory.h>
#include <string.h>
#include <Stream.h>
#include <math.h>
#include "DegreeTracker.h"

long testTimer = 0;

/* Global Objects */
SysConfig *Global_SysConfig;

Button *onButton;
Button *open_uSwitch;

LED *bLED;
LED *ardLED;
boolean a = false;
Buzzer *coolBuzz;
Motor_Driver *mot_Driver;

Potentiometer *respVolume;
Potentiometer *respCycle;
Potentiometer *IERatio;

PID *pid;

Trajectory *trajectory;

DegreeTracker *degreeTracker;

int table_RV[] = {200, 300, 400, 500, 600, 700, 800};
int table_RC[23];
int table_IE[] = {1, 2, 3, 4};
float calcedLeftTime[400];
float calcedLeftDegree[400];
//int MotorSpeedDesired[300];
int   MotorSpeedActual[400];
float MotorPwm[400];

int motorSpeed = 0;
int myCounter = 0;

int j = 0; int k = 0; 
volatile int timeStepValid = 0;

int motorStopped=0;
int motorMustStop=0;
int timerPinVal=LOW;
int comeAndGo=0;
int changeDir=0;

int ouSwithHitPC=0;
int stopPC=0;
 
/* ------------- Initial Check ------------*/
static void resetParams()
{
	Motor::getInstance()->resetEncPeriod();
	Motor::getInstance()->resetPC();
	pid->resetParams();
	degreeTracker->resetPosition();
}

static void onMotorStart()
{
	resetParams();
	Motor::getInstance()->motorStart();
	timeStepValid = 0;
	Timer4Start(round(15625 * pid->getTimeStep()) - 1);
}

static void onMotorStop()
{
	Motor::getInstance()->motorStop();
	resetParams();
	Timer4Stop();
}

static void motorSpeedCheck(){
	if(round(Motor::getInstance()->getEncRPM())>55)
	{
		Serial.println("check");
		onMotorStop();
		comeAndGo=0;
	}
}

void static initial_Check()
{
	if (open_uSwitch->get_Status() == BSTATE_HIGH)
	{
		Serial.println("Initial Setup");
		Motor::getInstance()->setSpeed(Global_SysConfig->motorInitPWM-2);
		Motor::getInstance()->setDirection(DIRECTION_OPEN);
		Motor::getInstance()->motorStart();
		while (open_uSwitch->get_Clicked() == false){
			delay(1);
			motorSpeedCheck();
		}
		open_uSwitch->set_Clicked(false);
		Motor::getInstance()->motorStop();
		Motor::getInstance()->setSpeed(Global_SysConfig->motorInitPWM);
	}
}

void setup()
{
	noInterrupts();
	
	for (size_t i = 8; i <= 30; i++)
		table_RC[i - 8] = i;

	Init_Timer1();
	Init_Timer3();
	Init_Timer4();
	Init_Timer5();

	Serial.begin(115200);

	Global_SysConfig = new SysConfig(2, 20, 0);
	Global_SysConfig->setParams(6e-1, 5e-3, 235);

	PinConfiguration::getInstance()->pinConfiguration();

	coolBuzz = new Buzzer(PinConfiguration::buzzerPin);

	//mot_Driver = new Motor_Driver(Motor::getInstance());

	onButton = new Button(PinConfiguration::onButton_pin);
	onButton->setPressCallback(onButton_callback);

	open_uSwitch = new Button(PinConfiguration::open_uSw_pin, INPUT, open_uSw_callback, LOW);

	bLED = new LED(PinConfiguration::bLED_pin);

	respVolume = new Potentiometer(PinConfiguration::Potentiometer_Volume, 7);
	respCycle  = new Potentiometer(PinConfiguration::Potentiometer_Cycle, 23);
	IERatio    = new Potentiometer(PinConfiguration::Potentiometer_IE, 4);

	respCycle->set_Range(table_RC, sizeof table_RC);
	respVolume->set_Range(table_RV, sizeof table_RV);
	IERatio->set_Range(table_IE, sizeof table_IE);

	pid = new PID(3, 48, 0.025, 10);
	pid->setTimeStep(Global_SysConfig->timeStep);
	pid->setOutputRange(0, 255);

	degreeTracker = new DegreeTracker((float)35, Global_SysConfig->duration, Global_SysConfig->timeStep);

	interrupts();

	Motor::getInstance()->setSpeed(Global_SysConfig->motorInitPWM);
	Motor::getInstance()->initEnc(PinConfiguration::motorEncoderPin, INPUT, enc_callback, RISING);
	initial_Check();
}

void loop()
{
	//mot_Driver->update_resp_rate(Global_SysConfig);
	//mot_Driver->check();
	//LCD::getInstance()->LCD_Menu(respVolume->Potentiometer_Read(), respCycle->Potentiometer_Read(), IERatio->Potentiometer_Read());
	onButton->check();
	
	if(motorStopped==0)
		motorSpeedCheck();
	
	if (onButton->get_Clicked() == true && onButton->get_On_Off() == BSTATE_ON)
	{
		Motor::getInstance()->setDirection(DIRECTION_CLOSE);
		comeAndGo=1;
		onMotorStart();
		onButton->set_Clicked(false);
	}

	else if (onButton->get_Clicked() == true && onButton->get_On_Off() == BSTATE_OFF)
	{
		motorMustStop=1;
		onButton->set_Clicked(false);				
	}
	if(k==2){
		k=0;
		if(motorMustStop==1){		
			onMotorStop();
			motorMustStop=0;		
			for (int i = 0; i < myCounter; i++)
			{
				Serial.print(i);
				Serial.print("\t");
				Serial.print(calcedLeftTime[i],3);
				Serial.print("\t");
				Serial.print(calcedLeftDegree[i],3);
				Serial.print("\t");
				Serial.print(MotorPwm[i]);
				Serial.print("\t");
				Serial.println(MotorSpeedActual[i]);
			}
			Serial.println(ouSwithHitPC);			
			j=0;	
			myCounter=0;			
			comeAndGo=0;
			Motor::getInstance()->setSpeed(Global_SysConfig->motorInitPWM);				
		}
	}

	if (open_uSwitch->get_Clicked() == true)
	{
		//TCNT5 = 0;
		//Motor::getInstance()->changeDirection();
		ouSwithHitPC=Motor::getInstance()->getPC();
		//bLED->switch_led();
		//pid->resetParams();
		open_uSwitch->set_Clicked(false);
	}

	if (Motor::getInstance()->getStatus() == MOTOR_IS_ON)	
	{		
		if (timeStepValid)
		{			
			timeStepValid = 0;	
	
			degreeTracker->updateTime();
			degreeTracker->updatePosition(Motor::getInstance()->getPC());			
			motorSpeed = pid->Calc(degreeTracker->updateDesiredRPM(), Motor::getInstance()->getEncRPM());
			Motor::getInstance()->setSpeed(motorSpeed);	

			calcedLeftTime[myCounter]    = degreeTracker->getLeftTime();
			calcedLeftDegree[myCounter]  = degreeTracker->getLeftDeltaDegree();
			MotorPwm[myCounter] 		 = motorSpeed;
			MotorSpeedActual[myCounter]  = round(Motor::getInstance()->getEncRPM());

			if(myCounter<399)
				myCounter++;
			
			if (degreeTracker->getLeftTime()<0.04){
				motorStopped = 1;
				//f[0]=Motor::getInstance()->getPC();
				//t[0]=degreeTracker->getLeftTime();
				Motor::getInstance()->motorStop();
				j++;
								
			}	
		}
	}
	else if (j==1){
		if (timeStepValid)
		{			
			timeStepValid = 0;			

			degreeTracker->updateTime();
			degreeTracker->updatePosition(Motor::getInstance()->getPC());	
			degreeTracker->updateDesiredRPM();
		
			calcedLeftTime[myCounter]    = degreeTracker->getLeftTime();
			calcedLeftDegree[myCounter]  = degreeTracker->getLeftDeltaDegree();
			MotorPwm[myCounter] 		 = motorSpeed;
			MotorSpeedActual[myCounter]  = round(Motor::getInstance()->getEncRPM());

			if(myCounter<399)
				myCounter++;
		}
	}

	if(degreeTracker->getLeftTime()<0){	
		k++;
		//j++;
		///if(j==2)
		j=0;
		motorStopped=0;	
		int currentPC=Motor::getInstance()->getPC();	
		Timer5Stop();		
		onMotorStop();
		changeDir=not(changeDir);
		if(changeDir){
			Motor::getInstance()->setDirection(DIRECTION_OPEN);
			degreeTracker->updateDesiredDelatTime(2*Global_SysConfig->duration);
		}
		else{
			Motor::getInstance()->setDirection(DIRECTION_CLOSE);
			//degreeTracker->updateDesiredDeltaDegree(35+(currentPC-ouSwithHitPC)*(float)360 / (float)MOTOR_PULSE_PER_TURN);
			degreeTracker->updateDesiredDelatTime(Global_SysConfig->duration);
		}
		Motor::getInstance()->setSpeed(Global_SysConfig->motorInitPWM);
		delay(50);
		onMotorStart();	
					
	}
		
	wdt_reset();
}
