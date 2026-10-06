//www.elegoo.com
//2016.12.08

const int ledPin = 2;//the led attach to

void setup()
{ 
  pinMode(ledPin,OUTPUT);//initialize the ledPin as an output
  pinMode(5,INPUT);

} 

void loop() 
{ 
  int digitalVal = digitalRead(5); //read pin 5
  if(HIGH == digitalVal)
  {
    digitalWrite(ledPin,HIGH);//turn the led off
  }
  else
  {
    digitalWrite(ledPin,LOW);//turn the led on 
  }
}
