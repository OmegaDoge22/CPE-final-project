  #include <RTClib.h>
  #include <LiquidCrystal.h>
  #include <DHT.h>
  #include <Stepper.h>

  // UART Pointers
  volatile unsigned char *myUCSR0A  = (unsigned char *)0x00C0;
  volatile unsigned char *myUCSR0B  = (unsigned char *)0x00C1;
  volatile unsigned char *myUCSR0C  = (unsigned char *)0x00C2;
  volatile unsigned int  *myUBRR0   = (unsigned int *)0x00C4;
  volatile unsigned char *myUDR0    = (unsigned char *)0x00C6;

  //ADC Pointers
  volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
  volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
  volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
  volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

  //Pin Pointers
  unsigned char* pin_h = (unsigned char*)0x100;
  unsigned char* ddr_b = (unsigned char*) 0x24; 
  unsigned char* port_b = (unsigned char*) 0x25; 
  unsigned char* ddr_h = (unsigned char*) 0x101; 
  unsigned char* port_h = (unsigned char*) 0x102; 

  LiquidCrystal lcd(27, 26, 25, 24, 23, 22); //Creates the lcd board and the pins its connected to
  RTC_DS3231 rtc;
  DHT dht(3, DHT11);
  Stepper myStepper(200, 30, 32, 31, 33);
  #define RDA 0x80
  #define TBE 0x20  
  int state = 0;
  bool changed = false;
  bool ventCheck = false;

  char stateOff[25] = "State switched to off.";
  char stateIdle[25] = "State switched to idle."; 
  char stateError[25] = "State switched to error.";
  char stateRun[25] = "State switched to run";
  char stateVent[25] = "Vent has been moved";

  void setup() {
    U0Init(9600);
    adc_init();
    lcd.begin(16, 2);
    rtc.begin();
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    dht.begin();
    

    DDRE &= 0xEF; //Start button pin = 2
    DDRH &= 0xDF; //Stop button pin = 8
    DDRH &= 0xBF; //Reset button pin = 9

    PORTE |= 0x10; //Sets all buttons to be pull up
    PORTH |= 0x20;
    PORTH |= 0x40; 

    attachInterrupt(digitalPinToInterrupt(2), start, FALLING);

    DDRB |= 0x10; //Yellow LED pin = 10
    DDRB |= 0x20; //Green LED pin = 11
    DDRB |= 0x40; //Red LED pin = 12
    DDRB |= 0x80; //Blue LED pin = 13

    DDRH &= 0xF7; //Input for on/off in H bridge (pin 6)
  

    //Water Sensor Analog pin = 0
    //Potentiometer analog pin = 1
    //RTC SDA pin = 20
    //RTC SCL pin = 21
  }

  
  void loop() {
    
    if((*pin_h & 0b00100000) && state != 0){
    
      state = 0;
      lcd.clear();
      time();
      putChar(' ');
      for(int i = 0; i < 22; i++){
        putChar(stateOff[i]);
      }
      putChar('\n');
    }
    
    int turnVent = adc_read(1);
    if(turnVent < 188 && state != 0){
      //turn motor right
      if(ventCheck){
        ventCheck = false;
        time();
        putChar(' ');
        for(int i = 0; i < 19; i++){
          putChar(stateVent[i]);
        }
        putChar('\n');
      }

      while(turnVent < 188){
        myStepper.setSpeed(40);
        myStepper.step(5);
        turnVent = adc_read(1);
      }
    }
    else if(turnVent >= 188 && turnVent <= 628 && state != 0){
      //dont turn vent
      ventCheck = true;
    }
    else if(turnVent > 628 && state != 0){
      //turn motor left
      if(ventCheck){
        ventCheck = false;
        time();
        putChar(' ');
        for(int i = 0; i < 19; i++){
          putChar(stateVent[i]);
        }
        putChar('\n');
      }

      while(turnVent > 628){
        myStepper.setSpeed(40);
        myStepper.step(-5);
        turnVent = adc_read(1);
      }
    }

    if(state == 0){
      PORTB |= 0x10; //Turns all LEDs off besides yellow LED
      PORTB &= 0xDF;
      PORTB &= 0xBF;
      PORTB &= 0x7F;
      
      PORTH &= 0xF7; //Turns fan off
      
    }
    else if(state == 1){
      if(changed){
        changed = false;
        time();
        putChar(' ');
        for(int i = 0; i < 23; i++){
          putChar(stateIdle[i]);
        }
        putChar('\n');

        int waterLevel = adc_read(0);
        int temp = dht.readTemperature(true);
        lcd.clear();
        
        lcd.print("Water Level: ");
        lcd.print(waterLevel);
        lcd.setCursor(0, 1);
        lcd.print("Temperature: ");
        lcd.print(temp);
        
      }
      PORTB &= 0xEF; //Turns all LEDs off besides green LED
      PORTB |= 0x20;
      PORTB &= 0xBF;
      PORTB &= 0x7F;

      PORTH &= 0xF7; //Turns fan off

      unsigned long currentMillis = millis();
      int waterLevel = adc_read(0);
      int temp = dht.readTemperature(true);
      
      

      if(currentMillis % 60000 == 0){
        waterLevel = adc_read(0);
        temp = dht.readTemperature(true);;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Water Level: ");
        lcd.print(waterLevel);
        lcd.setCursor(0, 1);
        lcd.print("Temperature: ");
        lcd.print(temp);


      }

      if(waterLevel <= 40){
        state = 2;
        time();
        putChar(' ');
        for(int i = 0; i < 24; i++){
          putChar(stateError[i]);
        }
        putChar('\n');
        changed = true;
      }

      if(temp > 76){
        state = 3;
        time();
        putChar(' ');
        for(int i = 0; i < 21; i++){
          putChar(stateRun[i]);
        }
        putChar('\n');
        changed = true;
      }

    }
    else if(state == 2){
      PORTB &= 0xEF; //Turns all LEDs off besides red LED
      PORTB &= 0xDF;
      PORTB |= 0x40;
      PORTB &= 0x7F;

      PORTH &= 0xF7; //Turns fan off

      if(changed){
        changed = false;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ERROR: WATER TOO");
        lcd.setCursor(0, 1);
        lcd.print("LOW");
      }
      int waterLevel = adc_read(0);

      if(((*pin_h & 0b01000000)) && waterLevel > 40){
        state = 1;
        changed = true;
        time();
        putChar(' ');
        for(int i = 0; i < 23; i++){
          putChar(stateIdle[i]);
        }
        putChar('\n');
      }
    }  
    else if(state == 3){

      if(changed){
        changed = false;
        int waterLevel = adc_read(0);
        int temp = dht.readTemperature(true);
        lcd.clear();
        
        lcd.print("Water Level: ");
        lcd.print(waterLevel);
        lcd.setCursor(0, 1);
        lcd.print("Temperature: ");
        lcd.print(temp);
        
      }
      PORTB &= 0xEF; //Turns all LEDs off besides blue LED
      PORTB &= 0xDF;
      PORTB &= 0xBF;
      PORTB |= 0x80;

      PORTH |= 0x08; //Turns fan on

      int waterLevel = adc_read(0);
      int temp = dht.readTemperature(true);
      unsigned long currentMillis = millis();
      if(currentMillis % 60000 == 0){
        waterLevel = adc_read(0);
        temp = dht.readTemperature(true);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Water Level: ");
        lcd.print(waterLevel);
        lcd.setCursor(0, 1);
        lcd.print("Temperature: ");
        lcd.print(temp);


      }

      if(waterLevel < 40){
        state = 2;
        time();
        putChar(' ');
        for(int i = 0; i < 24; i++){
          putChar(stateError[i]);
        }
        putChar('\n');
        changed = true;
      }

      if(temp < 76){
        state = 1;
        time();
        putChar(' ');
        for(int i = 0; i < 23; i++){
          putChar(stateIdle[i]);
        }
        putChar('\n');
      }
    }
  }

  void U0Init(int U0baud)
  {
    unsigned long FCPU = 16000000;
    unsigned int tbaud;
    tbaud = (FCPU / 16 / U0baud - 1);
    // Same as (FCPU / (16 * U0baud)) - 1;
    *myUCSR0A = 0x20;
    *myUCSR0B = 0x18;
    *myUCSR0C = 0x06;
    *myUBRR0  = tbaud;
  }

  unsigned char kbhit()
  {
    return *myUCSR0A & RDA;
  }

  unsigned char getChar()
  {
    return *myUDR0;
  }

  void putChar(unsigned char U0pdata)
  {
  while(!(*myUCSR0A & TBE));
  *myUDR0 = U0pdata;
  }

  void adc_init() //write your code after each commented line and follow the instruction 
  {
  // Enable ADC
    *my_ADCSRA |= (1 << 7);

    // Set prescaler to 128 (slow but reliable)
    *my_ADCSRA &= 0b11111000;
    *my_ADCSRA |= 0b00000111;

    // Disable auto-trigger
    *my_ADCSRA &= ~(1 << 5);

    // Disable ADC interrupt
    *my_ADCSRA &= ~(1 << 3);

    // Clear MUX5
    *my_ADCSRB &= ~(1 << 3);

    // Use AVCC as reference, right-adjust
    *my_ADMUX = (1 << 6);
  }

  unsigned int adc_read(unsigned char adc_channel_num) 
  {
  // clear MUX4:0
    *my_ADMUX &= 0b11100000;
    // set channel number
    *my_ADMUX |= (adc_channel_num & 0x07);

    // clear MUX5 (always 0 for channels 0–7)
    *my_ADCSRB &= ~(1 << 3);

    // start conversion (ADSC = 1)
    *my_ADCSRA |= (1 << 6);

    // wait for conversion to finish (ADSC becomes 0)
    while (*my_ADCSRA & (1 << 6));

    // MUST read low byte first
    unsigned char low  = *(volatile uint8_t*)0x78;  // ADCL
    unsigned char high = *(volatile uint8_t*)0x79;  // ADCH

    return (high << 8) | low;
  }

  void start(){
    if(state != 1){
      state = 1;
      changed = true;
    }
  }

  void time(){
    DateTime now = rtc.now();
    static char t[9]; 
    snprintf(t, sizeof(t), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    for (int i = 0; i < 8; i++) { 
      putChar(t[i]);
    }
  }
