//www.elegoo.com
//2016.12.12

/************************
Exercise the motor using
the L293D chip
************************/

#define ENABLE 5
#define DIRA 18
#define DIRB 19
#define RELAY_PIN 23  // 重命名为RELAY_PIN，更直观体现继电器控制功能（原RE）

int i;
 
void setup() {
  //---set pin direction
  pinMode(ENABLE,OUTPUT);
  pinMode(DIRA,OUTPUT);
  pinMode(DIRB,OUTPUT);
  pinMode(RELAY_PIN,OUTPUT); 
  Serial.begin(9600);
}

void loop() {
   
    digitalWrite(RELAY_PIN,HIGH);
    Serial.println("Relay ON: L293D powered up"); 
    
//---back and forth example
    Serial.println("One way, then reverse");
    digitalWrite(ENABLE,HIGH); // enable on
    for (i=0;i<5;i++) {
    digitalWrite(DIRA,HIGH); //one way
    digitalWrite(DIRB,LOW);
    delay(750);
    digitalWrite(DIRA,LOW);  //reverse
    digitalWrite(DIRB,HIGH);
    delay(750);
  }
  digitalWrite(ENABLE,LOW); // disable
  delay(3000);
  
  // 再次执行电机正反转（L293D保持上电状态）
  for (i=0;i<5;i++) {
    digitalWrite(DIRA,HIGH); //one way
    digitalWrite(DIRB,LOW);
    delay(750);
    digitalWrite(DIRA,LOW);  //reverse
    digitalWrite(DIRB,HIGH);
    delay(750);
  }
  digitalWrite(ENABLE,LOW); // disable
  delay(3000);
  
  // 2. 继电器释放：切断L293D 16脚供电（L293D断电休眠）
  digitalWrite(RELAY_PIN,LOW);
  Serial.println("Relay OFF: L293D power cut off"); // 新增串口提示，便于调试
  delay(5000); // 可选：增加断电休眠时间，再进入下一轮循环
}