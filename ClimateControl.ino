/*
	V1.2a Climate Control Arduino Project for Arduino Mega 2560

	By Martin Collins
	31st May 2019
*/

#include "Arduino.h"
#include <LiquidCrystal.h>
#include <dht_nonblocking.h>
#define dht_sensor_type DHT_TYPE_11

const bool DEBUG = false;

//Custom degree character
byte degree[8] = {
  B00110,
  B01001,
  B01001,
  B00110,
  B00000,
  B00000,
  B00000,
  B00000,
};

//Custom "Fan On" indicator
byte fan_on[8] = {
  B00100,
  B10101,
  B01110,
  B11111,
  B01110,
  B10101,
  B00100,
  B00000,
};

//Custom "Fan Off" indicator
byte fan_off[8] = {
  B00100,
  B00100,
  B00100,
  B11111,
  B00100,
  B00100,
  B00100,
  B00000,
};

//Inputs
const int dht_sensor_pin = 2; //DHT11 Sensor pin
const int btn_up = 32, btn_down = 30, btn_mode = 28; // Button pins
//Outputs
const int motor_pin = 3; //Motor pin
const int led_red = 22, led_green = 24, led_blue = 26; //LED pins
const int rs = 8, en = 9, d4 = 10, d5 = 11, d6 = 12, d7 = 13; //LCD pins
const int lcd_cols = 16, lcd_lines = 2;

//Object declarations
DHT_nonblocking dht_sensor(dht_sensor_pin, dht_sensor_type);
LiquidCrystal lcd(rs, en, d4, d5, d6, d7); // @suppress("Abstract class cannot be instantiated")

unsigned long poll_delay = 30000ul; // Duration between readings, Default = 30 secs.
const unsigned long min_poll_delay = 10000ul; //Minimum polling delay. Default = 10 secs.
const unsigned long lcd_refresh_delay = 2000ul; // 2 secs delay before refreshing LCD screen.
unsigned long lcd_refresh_timestamp = 0; // Holds timestamp of when lcd refresh was requested.
float user_hot_threshold = 32.0; // Holds user selectable hot threshold. Default = 32 degrees C
float user_cold_threshold = 0.0; // Holds user selectable cold threshold. Default = 0 degrees C
bool user_temp_fahrenheit = true; // Holds user selectable Fahrenheit indicator. Default = true
int user_mode = 0; // Holds user selectable mode. 1=set hot threshold, 2=set cold threshold, 3=temp in F/C
bool refreshLCD = false;
bool firstRun = true;

//Variables for debouncing
bool last_btn_up = LOW;
bool current_btn_up = LOW;
bool last_btn_down = LOW;
bool current_btn_down = LOW;
bool last_btn_mode = LOW;
bool current_btn_mode = LOW;
const int debounce_delay = 100;

float temperature;
float humidity;

void setup()
{
	if(DEBUG == true)
	{
		Serial.begin(9600);
	}
	//Setup Motor as Output
	pinMode(motor_pin, OUTPUT);
	//Setup LEDs as Output
	pinMode(led_red, OUTPUT);
	pinMode(led_green, OUTPUT);
	pinMode(led_blue, OUTPUT);
	//Setup LCD
	lcd.begin(lcd_cols, lcd_lines);
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Waiting for");
	lcd.setCursor(0,1);
	lcd.print("first reading...");
	//Create custom chars
	lcd.createChar(0, fan_off);
	lcd.createChar(1, fan_on);
	lcd.createChar(3, degree);
	Serial.println(F("<Arduino Ready>"));
}

void loop()
{
	checkLCDRefresh();
	checkInputFromButtons();
	checkForNewTempHumReading();
}

static bool measure_environment(float *temperature, float *humidity) // @suppress("Unused static function")
{
  static unsigned long measurement_timestamp = millis();

  if((millis() - measurement_timestamp) >= poll_delay || firstRun == true)
  {
    if(dht_sensor.measure(temperature,humidity))
    {
    	if(firstRun)
    	{
    		firstRun = false;
    	}
    	measurement_timestamp = millis();
    	return(true);
    }
  }
  return(false);
}

bool debounce(bool last, int pin)
{
  bool current = digitalRead(pin);
  if (last != current)
  {
    delay(debounce_delay);
    current = digitalRead(pin);
  }
  return current;
}

void enableLED(int pin)
{
	//Make sure all LEDs are turned off
	digitalWrite(led_red, LOW);
	digitalWrite(led_green, LOW);
	digitalWrite(led_blue, LOW);
	//Now turn on the LED we want
	digitalWrite(pin, HIGH);
}

void processTempHum()
{
	char unit;
	float displayTemp;

	if (user_temp_fahrenheit)
	{
		unit = 'F';
		displayTemp = convertTempfromCtoF(&temperature);
	}
	else
	{
		unit = 'C';
		displayTemp = temperature;
	}

	lcd.clear();
	//First LCD line
	lcd.setCursor(0,0);
	lcd.print("Temp:");
	lcd.print(displayTemp, 1);
	lcd.write(byte(3));
	lcd.print(unit);
	//Second LCD line
	lcd.setCursor(0,1);
	lcd.print("Hum.:");
	lcd.print(humidity, 1);
	lcd.print("%");

	if (temperature <= user_cold_threshold)
	{
		//COLD
		enableLED(led_blue);
		analogWrite(motor_pin, 0);
		lcd.setCursor(11,1);
		lcd.print("[C]");
		lcd.setCursor(15,1);
		lcd.write(byte(0));
	}
	else if (temperature >= user_hot_threshold)
	{
		//HOT
		enableLED(led_red);
		analogWrite(motor_pin, 255);
		lcd.setCursor(11,1);
		lcd.print("[H]");
		lcd.setCursor(15,1);
		lcd.write(byte(1));
	}
	else
	{
		//OK
		enableLED(led_green);
		analogWrite(motor_pin, 0);
		lcd.setCursor(11,1);
		lcd.print("[G]");
		lcd.setCursor(15,1);
		lcd.write(byte(0));
	}
}

float convertTempfromCtoF(float *temperatureInC)
{
	return (*temperatureInC * 1.8) + 32.0;
}

void displayParamValue(const char* prefix, float param, bool paramIsTemp, const char* suffix = 0)
{
	char unit;
	float displayParamValue;

	if (paramIsTemp)
	{
		if(user_temp_fahrenheit)
		{
			unit = 'F';
			displayParamValue = convertTempfromCtoF(&param);
		}
		else
		{
			unit = 'C';
			displayParamValue = param;
		}
	}
	else
	{
		displayParamValue = param;
	}
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print(prefix);
	lcd.setCursor(0,1);
	lcd.print(displayParamValue,0);
	if(paramIsTemp)
	{
		lcd.write(byte(3));
		lcd.print(unit);
	}
	if(suffix != 0)
	{
		lcd.print(suffix);
	}
}

void changeUserTempFahrenheit()
{
	lcd.clear();
	lcd.setCursor(0,0);

	char unit;

	user_temp_fahrenheit = !user_temp_fahrenheit;

	if(user_temp_fahrenheit)
	{
		unit = 'F';
	}
	else
	{
		unit = 'C';
	}
	Serial.print("Show temp in: ");
	Serial.print(char(176));
	Serial.println(unit);
	//First LCD line
	lcd.print("Show temp in:");
	lcd.setCursor(0,1);
	//Second LCD line
	lcd.write(byte(3));
	lcd.print(unit);
}

void flagRefreshLCD()
{
	if(refreshLCD == false)
	{
		refreshLCD = true;
	}
	lcd_refresh_timestamp = millis();
}

void checkInputFromButtons()
{
	//Debounce buttons
	current_btn_up = debounce(last_btn_up, btn_up);
	current_btn_down  = debounce(last_btn_down, btn_down);
	current_btn_mode  = debounce(last_btn_mode, btn_mode);

	//Turn down the set temp
	if (last_btn_down== LOW && current_btn_down == HIGH && firstRun != true)
	{
		Serial.println(F("Down button pressed!"));

		switch (user_mode)
		{
			case 1:
				user_hot_threshold--;
				Serial.println(F("Decrease hot threshold by 1 degree."));
				displayParamValue("Hot threshold:",user_hot_threshold,true);
				break;
			case 2:
				user_cold_threshold--;
				Serial.println(F("Decrease cold threshold by 1 degree."));
				displayParamValue("Cold threshold:",user_cold_threshold,true);
				break;
			case 3:
				changeUserTempFahrenheit();
				break;
			case 4:
				if((poll_delay-1000) >= min_poll_delay)
				{
					poll_delay = poll_delay-1000;
					Serial.println(F("Decrease Polling Frequency by 1 second."));
				}
				displayParamValue("Polling freq.:",(poll_delay/1000),false," sec(s)");
				break;
		}
			flagRefreshLCD();
		}
		//Turn up the set temp
		else if (last_btn_up== LOW && current_btn_up  == HIGH && firstRun != true)
		{
			Serial.println(F("Up button pressed!"));

			switch (user_mode)
			{
				case 1:
					user_hot_threshold++;
					Serial.println(F("Increase hot threshold by 1 degree."));
					displayParamValue("Hot threshold:",user_hot_threshold,true);
					break;
				case 2:
					user_cold_threshold++;
					Serial.println(F("Increase cold threshold by 1 degree."));
					displayParamValue("Cold threshold:",user_cold_threshold,true);
					break;
				case 3:
					changeUserTempFahrenheit();
					break;
				case 4:
					poll_delay = poll_delay+1000;
					Serial.println(F("Increase Polling Frequency by 1 second."));
					displayParamValue("Polling freq.:",(poll_delay/1000),false," sec(s)");
					break;
			}
			flagRefreshLCD();
		}
		//Change the mode
		else if (last_btn_mode== LOW && current_btn_mode  == HIGH && firstRun != true)
		{
			Serial.print(F("Mode button pressed!"));

			switch (user_mode)
			{
				case 4:
					user_mode = 1;
					break;
				default:
					user_mode++;
					break;
			}
			Serial.print(F(" Selected mode: "));
			Serial.println(user_mode);

			lcd.clear();
			lcd.setCursor(0,0);

			switch (user_mode)
			{
				case 1:
					Serial.println(F("Set Hot Threshold Mode selected."));
					lcd.print("Hot threshold");
					break;
				case 2:
					Serial.println(F("Set Cold Threshold Mode selected."));
					lcd.print("Cold threshold");
					break;
				case 3:
					Serial.println(F("Set Metric/Non-Metric Mode selected."));
					lcd.print("Metric/N-Metric");
					break;
				case 4:
					Serial.println(F("Set Polling Frequency Mode selected."));
					lcd.print("Set Polling Freq");
					break;
			}
			lcd.setCursor(0,1);
			lcd.print("mode enabled");
			flagRefreshLCD();
		}
}

void checkLCDRefresh()
{
	if(refreshLCD == true && (millis() - lcd_refresh_timestamp) >= lcd_refresh_delay)
	{
		refreshLCD = false;
		processTempHum();
	}
}

void checkForNewTempHumReading()
{
	if(measure_environment(&temperature,&humidity))
	{
		Serial.println(F("New reading received from sensor..."));
		processTempHum();
	}
}
