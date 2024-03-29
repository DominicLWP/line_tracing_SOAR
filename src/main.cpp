#include <AFMotor.h>
#include <Arduino.h>
#include <Adafruit_I2CDevice.h>
#include <SPI.h>
#include <TracerPID.h>
#include <vectors_line.h>
#include <EDifferential.h>
//JHELLO
enum MOVESTRAT
{
  PCONTROL_L=1,
  PCONTROL_R=-1,
  BESTACCEL=0,
  PIVOTL=4,
  PIVOTR=-4,
  BESTDECEL=-99
};

long lastMillis;

void motor_move(AF_DCMotor motor, int power)
{
  if(power==0)
  {
    motor.setSpeed(0);
    motor.run(RELEASE);
  }
  else if (power> 0)
  {
    motor.setSpeed(abs(power));
    motor.run(FORWARD);
  }
  else if (power< 0)
  {
    motor.setSpeed(abs(power));
    motor.run(BACKWARD);
  }
}

class sensor_buff
{
  public:
    sensor_buff(){};
    //int read;
    int indexPos;
    int pin;  
    //int bias;
    int thresholdBlack=500;
    bool outofline;
    virtual int readSensor()
    {
      //read = analogRead(pin);
      if(analogRead(pin)<thresholdBlack)
      {  
          return 1; //tune something here
      }
      else
      {
          return 0; //tune something here
      }
    }  
  
};

class linetrackingSensor
{
  public:
    double frameRead(sensor_buff* sensearray)
    {
      auto senseResult = pollLayer(sensearray);
      if(senseResult == 0b00000100)
      {
        return 0; //In this system it means hold your rate
      }
      else
      {
        return  0.01* (-(senseResult>>3) + ((senseResult>>1 | senseResult<<1) & 0b00000011));
      }
      return 0;
    }
    
    
  private:
    MOVESTRAT pStrat;
    uint32_t stratSwitch=0;
    byte pollLayer(sensor_buff* sensearray)
    {
      byte poll = 0b00000000;
      for(int i =0;i<5;i++)
      {
        poll |= (sensearray[i].readSensor()<<(i));
      }
      poll &= 0b00011111;
      return poll;
    }
};

sensor_buff sensorArray[5];
linetrackingSensor ltsA;

AF_DCMotor motor_left(1);
AF_DCMotor motor_right(4);
//WTF are the pins? IN1234, 2345, PWM 6,9

#define PGAIN 1.2
#define IGAIN 0.1
#define DGAIN 0.1

PIDClass softCon(PGAIN,IGAIN,DGAIN);

EDifferential eDif;

char* debugmessage;

//IR_1 is on the left
const int IR_1 = A0;
const int IR_2 = A1;
const int IR_3 = A2;
const int IR_4 = A3;
const int IR_5 = A4;

//#define MAX_STRAIGHTSPEED 200
//#define MAX_ACCEL 1;

int threshold = 400;
int currentSpeedL = 200;
int currentSpeedR = 200;
bool isLocked = true;

void setup() 
{
  pinMode(IR_1,INPUT);
  pinMode(IR_2,INPUT);
  pinMode(IR_3,INPUT);
  pinMode(IR_4,INPUT);
  pinMode(IR_5,INPUT);
  Serial.begin(9600);

  sensorArray[0].pin = IR_1;
  sensorArray[1].pin = IR_2;
  sensorArray[2].pin = IR_3;
  sensorArray[3].pin = IR_4;
  sensorArray[4].pin = IR_5;
  
  
  // sensorArray[0].bias = -2;
  // sensorArray[1].bias = -1;
  // sensorArray[2].bias = 0;
  // sensorArray[2].bias = 1;
  // sensorArray[3].bias = 2;

  //sensorArray[2].outofline = false;
  lastMillis = millis();
}

#define LOOP_MINTIME 12L

void loop() 
{
  #pragma region  LEGACY
  // //Serial.println(sensorArray[2].readSensor());

  // int totalBias =0;
  // if(sensorArray[2].readSensor()!=0)
  // {
  //   for(int i= 0;i<5;i++)
  //   {
  //     if(i!=2)
  //     {
  //       totalBias+= sensorArray[i].readSensor();
  //     }  
  //   }
  //   //Serial.println("");
  //   //need a curve function; speed hold?
  //   currentSpeedL-=totalBias;
  //   currentSpeedR+=totalBias;
  //   if(currentSpeedL<100)
  //   {
  //     currentSpeedL =100;
  //     currentSpeedR +=totalBias;
  //   }
  //   if(currentSpeedR<100)
  //   {
  //     currentSpeedR =100;
  //     currentSpeedL -=totalBias;
  //   }
  //   if(currentSpeedL>255)
  //   {
  //     currentSpeedL =255;
  //     currentSpeedR -=totalBias;
  //   }
  //   if(currentSpeedR>255)
  //   {
  //     currentSpeedR =255;
  //     currentSpeedL +=totalBias;
  //   }
  // }
  // else
  // {
  //   // Serial.print(abs(currentSpeedL-currentSpeedR));
  //   // Serial.println(" Straight");
    
  //   if(abs(currentSpeedL-currentSpeedR)>10)
  //   {
  //     bool isLhigher = (currentSpeedL>currentSpeedR);
  //     if(isLhigher)
  //     {
  //       currentSpeedL-=10;
  //     }
  //     else
  //     {
  //       currentSpeedR-=10;
  //     }
  //   }
  //   else
  //   {
  //     currentSpeedL+=MAX_ACCEL;
  //     currentSpeedR+=MAX_ACCEL;
  //     if(currentSpeedL>MAX_STRAIGHTSPEED)
  //     {
  //       currentSpeedR = MAX_STRAIGHTSPEED;
  //       currentSpeedL = MAX_STRAIGHTSPEED;
  //     }
  //   }
  // }
  // Serial.println(currentSpeedL);
  // Serial.print("L: ");
  // Serial.print(currentSpeedL);
  // Serial.print("R: ");
  // Serial.println(currentSpeedR);
  
  // // if(Serial.available()>0)
  // // {
  // //   if(Serial.read()=='u')
  // //   {
  // //     isLocked = false;
  // //   }
  // // }
  // if(isLocked)
  // {  
  //   motor_move(motor_left,currentSpeedL);
  //   motor_move(motor_right,currentSpeedR);
  // }
  
#pragma endregion
  auto startTime = millis();
  auto deltaTime = startTime-lastMillis;
  auto line_errors = false;
  if(deltaTime>LOOP_MINTIME)
  {
    //Instantiate a PID Class pls
    auto fRead = ltsA.frameRead(sensorArray);
    // Pivot Control
    
    auto dRead = softCon.getCorrectionTerm(deltaTime, fRead);// Store this into an array next time for more processing
    eDif.motorEControlLoop(dRead);
    lastMillis = millis()-startTime;

    motor_left.setSpeed(eDif.pMotorCommand.LMotor);
    motor_right.setSpeed(eDif.pMotorCommand.RMotor);
    motor_left.run(FORWARD);
    motor_right.run(FORWARD);
  }
  else
  {
    if(line_errors)
    {
      Serial.println(debugmessage);
    }
  }
}

