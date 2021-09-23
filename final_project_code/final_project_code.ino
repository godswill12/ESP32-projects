
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

LiquidCrystal_I2C lcd(0x3F, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'4', '3', '2', '1'},
  {'B', 'A', '0', '9'},
  {'8', '7', '6', '5'},
  {'F', 'E', 'D', 'C'}
};
byte rowPins[ROWS] = {7, 9, 8, 3};
byte colPins[COLS] = {4, 5, 10, 12}; //connect to the column pinouts of the keypad

//initialize an instance of class NewKeypad
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
bool stops = true;
int X;
int Y;
float Time = 0;
float frequency = 0;
float waterFlow = 0;
float total = 0;
float LS = 0;
const int input = A0;
const int test = 9;
const int relay = 11;
bool start = false;
bool pad = false;
char collect[] = {};
String inputString;
long inputInt;
int delivery = 0;
bool Read = false;
int k = 0;
char key;
void(* resetFunc) (void) = 0;
void setup()
{
  digitalWrite(relay, HIGH);
  lcd.init();                      // initialize the lcd
  lcd.init();
  inputString.reserve(10);
  // Print a message to the LCD.
  lcd.backlight();
  Serial.begin(9600);
  lcd.begin(20, 4);
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("FUEL DISPENSER");
  lcd.setCursor(0, 1);
  lcd.print("********************");
  delay(2000);
  pinMode(input, INPUT);
  pinMode(test, OUTPUT);
  analogWrite(test, 100);
  pinMode(relay, OUTPUT);
  pad = true;
}
void loop()
{
  if (delivery == 0 || delivery == 3) {
    Serial.println("RESTART");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CHOOSE WHAT METHOD:");
    lcd.setCursor(0, 1);
    lcd.print("A. LITRE    B. PRICE");
    lcd.setCursor(0, 2);
    lcd.print("PRESS CNT TO PROCEED");
    lcd.setCursor(0, 3);
    lcd.blink();
  }
  while (pad == true && Read == false) {
    Serial.println("DELIVERY CHOICE");
    key = customKeypad.getKey();
    if (key) {
      Serial.println(key);
      lcd.setCursor(0, 3);
      lcd.blink();
      lcd.print(key);
      if (key == 'C') {
        //Litre
        delivery = 1;
        // pad = false;

      }
      else if (key == 'E') {
        //Price
        delivery = 2;
        // pad = false;
      }
      else if (key == 'A') {
        //Restart
        delivery = 3;
        pad = false;
      }
      else if (key == 'F') {
        //Emter or Start
        pad = false;
      }
    }
  }
  if (delivery == 1 && Read == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ENTER THE LITRE:");
    lcd.setCursor(0, 1);
    lcd.blink();
  } 
  else if (delivery == 2 && Read == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ENTER THE PRICE:");
    lcd.setCursor(0, 1);
    lcd.blink();
  }
  pad = true;

  while (pad == true && (delivery != 3) && Read == false) {


    Serial.println("INPUTTING NUMBER");
    key = customKeypad.getKey();
    if (key) {
      Serial.println(key);

      if (key >= '0' && key <= '9') {
        lcd.setCursor(k, 1);
        lcd.print(key);
        k += 1;
        // only act on numeric keys
        inputString += key;               // append new character to input string
      } else if (key == 'F') {
        if (inputString.length() > 0) {
          inputInt = inputString.toInt(); // YOU GOT AN INTEGER NUMBER
          inputString = "";               // clear input
          // DO YOUR WORK HERE
          Serial.print("Collect: ");
          Serial.println(inputInt);
          pad = false;
        }
      } else if (key == 'A') {
        inputString = "";                 // clear input
      }
    }
  }
  //////////////////////////////////////////////////////
  if (delivery == 2 && Read == false) {
    Serial.println("Conversion to litre");
    inputInt = inputInt * 175;
  }
  Serial.println("Reading Time");
  Read = false;
  if (delivery != 3 && Read == false) {
    Read = true;
//    lcd.clear();
//    lcd.setCursor(0, 0);
//    lcd.print("Dispensed in Litres:");
//    lcd.setCursor(0, 1);
//    lcd.print(total);
    Serial.println("Flowing");
    if (total == 0) {
      Serial.println("Relay ON");
      digitalWrite(relay, LOW);
      start = true;
    }
    if (total >= (inputInt)&& stops == true) {
      stops = false;
      Serial.println("Relay OFF");
      digitalWrite(relay, HIGH);
      start = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Dispensed in Litres:");
      lcd.setCursor(0, 1);
      lcd.print(total);
    lcd.setCursor(0, 2);
    lcd.print("TO RESTART PRESS E ");
    lcd.setCursor(19,2);
    lcd.blink();
    pad = true;
    Read = false;
      while (pad == true && Read == false) {
    Serial.println("RESTART SYSTEM");
    key = customKeypad.getKey();
    if (key) {
      Serial.println(key);
      lcd.setCursor(19, 2);
      lcd.blink();
      lcd.print(key);
      if (key == 'E') {
        //Litre
        delivery = 1;
        resetFunc();
        // pad = false;

      }
    }
  }      
    }
    else if (total < inputInt && stops == true){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Dispensed in Litres:");
    lcd.setCursor(0, 1);
    lcd.print(total);
      }
    if (start == true) {
      Serial.println("READING FLOW");
      X = pulseIn(input, HIGH);
      Y = pulseIn(input, LOW);
      Time = X + Y;
      frequency = 1000000 / Time;
      waterFlow = frequency / 6.5;
      LS = waterFlow / 60;
      if (frequency >= 0)
      {
        if (isinf(frequency))
        {
          Serial.println("L/Min: 0.00");
          Serial.print("Total: ");
          Serial.print(total);
          Serial.println(" L");
        }
        else
        {
          total = total + LS;
          Serial.println(frequency);
          Serial.print("L/Min: ");
          Serial.println(waterFlow);
          Serial.print("Total: ");
          Serial.print(total);
          Serial.println(" L");
        }
      }
      delay(1000);
    }
  }
}
