/* GrillUp - remote grill temperature monitor
   uses Bluetooth 4.0 to communicate with Android app
   https://hackaday.io/project/2346
   Arduino Code
*/
#include <Servo.h> 
#include <Serial.h>

Servo servo1;
Servo servo2;

// pin configuration
byte Sensors[] = {A1, A2, A3, A0, A7, A6}; // analog pins that read temperature probe resistance
byte Probe_check[] = {2, 3, 4, 7, 6, 5}; // to check if temperature probe is connected
byte servo1_pin= 9; 
byte servo2_pin= 10;
byte pump_pin = 8;
byte laser_pin = 13;

// data arrays
int Coord[][2] = {{100,20},{100,20},{100,20},{100,20},{100,20},{100,20}}; // coordinates for each probe
int coord_temp[] = {0,100,80}; // temporary coordinates
int Temp_pairs[][2]={{0, 250},{7, 200},{10, 190},{25, 151},{51, 132},{69, 124},{150, 100},{208, 90},{250, 82},{328, 63},{420, 55},{445, 52},{555, 43},{570, 41},{616, 39},{671, 34},{774, 31},{950, 25},{973, 21},{1023, 1}};
byte temperature[6]; // temperature in Celcius degres
int Auto[]={0, 0, 0, 0, 0, 0}; // temperature limits for auto-cooling feature
int zone[]={55,145,70,120}; // vertical motor: from [0]-->[1], horizontal motor: [2]-->[3]

// available serial commands
char command;
char parameter0;
char parameter1;
char cool = 'A';
char coolXY = 'B';
char servo = 'C';
char auto_cooling = 'D';
char laser= 'E';

// counter variables
int interval = 1000; // time between temperature updates (miliseconds)
unsigned long start_time=0;
int duration=0; // actual millis() minus start time

void setup()
{
  Serial.begin(9600);
  // pin configuration
  pinMode(pump_pin, OUTPUT); 
  digitalWrite(pump_pin, LOW);
  pinMode(laser_pin, OUTPUT); 
  digitalWrite(laser_pin, LOW);

  for (int x = 0; x < 6; x++)  {
    pinMode(Sensors[x], INPUT);  
    pinMode(Probe_check[x], INPUT);  
  }  
  
  servo1.attach(servo1_pin);
  servo2.attach(servo2_pin);
  start_time=millis();
}

void loop(){
  
  duration=millis()-start_time;
  if(duration>interval){ // check temperature with intervals
    for(int x=0;x<6;x++){ 
      if(digitalRead(Probe_check[x])==LOW){ // checks whether the probe is connected
        int val= analogRead(Sensors[x]);
        temperature[x]=convertToCelcius(val);
      }else{
        temperature[x]=0x00;
      }
    }
    sendReadings(temperature);
    start_time=millis();
  }
  
  autoCooling(); // sprays probe if temperature exceeds treshold
  
  if(Serial.available() >= 3) {
    command = Serial.read();
    
    //dump the buffer if the first character was not a letter
    if(isalpha(command)){
    }
    if (!isalpha(command)){
      Serial.read(); // throw out the letter
      if ( Serial.available() < 2 ) {
        return;
      }
    }
  
  parameter0 = Serial.read(); // parameter 0
  parameter1 = Serial.read(); // parameter 1

  
  if (command == cool)  
  {
    // general cooling
    coolAll(zone);
  }
  else if (command == coolXY)
  {
    int target_int= parameter0 - '0'; // converting parameter 1 into int
    if(target_int<=5)
    {
      coolProbe(target_int);
    }
  }
  else if (command == servo)
  {
      int index= parameter0 - '0';
      if (index <= 5){
        if(index!=coord_temp[0]){
          coord_temp[0]=index;
          coord_temp[1]=Coord[index][0];
          coord_temp[2]=Coord[index][1];
        }
        switch(parameter1){
          case 'H':
            coord_temp[1]=coord_temp[1]+5; // add 5 degree to x-axis angle
            coord_temp[1]=constrain(coord_temp[1],0,180);
            break;
          case 'J':
            coord_temp[1]=coord_temp[1]-5; // subtract 5 degree from x-axis angle
            coord_temp[1]=constrain(coord_temp[1],0,180);
            break;
          case 'K':
            coord_temp[2]=coord_temp[2]+5; // add 5 degree to y-axis angle
            coord_temp[2]=constrain(coord_temp[2],0,180);
            break;
          case 'L':
            coord_temp[2]=coord_temp[2]-5; // subtract 5 degree from y-axis angle
            coord_temp[2]=constrain(coord_temp[2],0,180);
            break;
          case 'S':
            Coord[coord_temp[0]][0]=coord_temp[1]; // save coordinates
            Coord[coord_temp[0]][1]=coord_temp[2];
            laserControl(LOW); // turns off laser
            break;
        }
        setServo(coord_temp[1],coord_temp[2]); // set servos
      }
  }
  else if (command == laser)
  {
    switch(parameter0){
      case '0':
        laserControl(LOW);
        break;
      case '1':
        laserControl(HIGH);
        break;
    }
  }
  else if (command == auto_cooling)
  {
    // auto-cooling routine
    int index= parameter0 - '0';
    if (index <= 5){
      int val=(int)parameter1;
      Auto[index]=val;
    }
  } 
  command=0;
  parameter0=0;
  parameter1=0;
}
}

byte convertToCelcius(int reading){
  // converts reading from analog input to celcius degree
  int value=constrain(reading,0,1024);
  int lower_index=0;
  int upper_index=0;
  int num_rows = sizeof(Temp_pairs)/sizeof(Temp_pairs[0]);
  for (int x=1; x<num_rows; x++){
    if(value>=Temp_pairs[x][0]){
      lower_index=x;
    } 
  }
  upper_index=lower_index+1;
  int progress=100*(value-Temp_pairs[lower_index][0])/(Temp_pairs[upper_index][0]-Temp_pairs[lower_index][0]); // percentrage of progress 
  float temp_val=Temp_pairs[lower_index][1]+progress*((float)(Temp_pairs[upper_index][1]-Temp_pairs[lower_index][1])/100);
  byte temp=(byte)temp_val;
  return temp;
}

void coolAll(int xy[]){
  int change=5;
  for(int a=0;a<4;a++){
    xy[a]=constrain(xy[a],0,180);
  }
  int rangeX=xy[1]-xy[0];
  int rangeY=xy[3]-xy[2];
  if (rangeX<change){
    xy[1]=xy[0]+change;
  }
  if (rangeY<change){
    xy[3]=xy[2]+change;
  }
  digitalWrite(pump_pin, HIGH);
  delay(100);
  for(int x=xy[0];x<xy[1];x=x+change){
    servo1.write(x); 
    for(int y=xy[2];y<xy[3];y=y+change){ 
      servo2.write(y);
      delay(30);
    }
  }
  digitalWrite(pump_pin, LOW);  
}

void sendReadings(byte temperature[]){
  // sends temperature
  Serial.write(temperature[0]);
  Serial.write(temperature[1]);
  Serial.write(temperature[2]);
  Serial.write(temperature[3]);
  Serial.write(temperature[4]);
  Serial.write(temperature[5]);
}

void coolProbe(int target_probe){
  // sets servos on XY position, and turns pump on for set ms
  setServo(Coord[target_probe][0],Coord[target_probe][1]);
  delay(700);
  digitalWrite(pump_pin, HIGH);
  delay(1000);
  digitalWrite(pump_pin, LOW);
}

void autoCooling(){
  // turns on cooling if temperature excels set threshold
  for(int x=0; x<6; x++){
    if(Auto[x]!=0){
      if(temperature[x]>Auto[x]){
        coolProbe(x);
      }
    }
  }
}

void setServo(int x, int y){
  // sets servo to x,y coordinates
  int x_val= constrain(x, 0, 180);
  int y_val= constrain(y, 0, 180);
  servo1.write(x_val); 
  delay(10);
  servo2.write(y_val);
  delay(10);
}

void laserControl(boolean state){
  // turns on/off laser pointer
  digitalWrite(laser_pin, state);
}
