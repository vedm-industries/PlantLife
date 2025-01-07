#include <LiquidCrystal.h>
#include <dht_nonblocking.h>
#include "pitches.h"
#include "IRremote.h"
#include <Servo.h>
#include "SR04.h"
#include <Wire.h>

#define DHT_SENSOR_TYPE DHT_TYPE_11
#define DHT_SENSOR_PIN 2
#define TRIG_PIN 6
#define ECHO_PIN 5
#define RECEIVER_PIN 13
#define MOISTURE_SENSOR_PIN A0
#define LIGHT_SENSOR_PIN A1
#define BUZZER_PIN 3

Servo myservo;
SR04 sr04 = SR04(ECHO_PIN, TRIG_PIN);
IRrecv irrecv(RECEIVER_PIN);
decode_results results;
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
DHT_nonblocking dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

const int MPU_addr = 0x68;  // I2C address of the MPU-6050
int16_t AcX, AcY, AcZ;
int16_t prevAcX = 0, prevAcY = 0, prevAcZ = 0;

int melody[] = {NOTE_C5, NOTE_D5, NOTE_E5};
int duration = 500;

void setup() {
    Wire.begin();
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x6B);  // PWR_MGMT_1 register
    Wire.write(0);     // Set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);

    myservo.attach(4);
    irrecv.enableIRIn();
    Serial.begin(9600);
    lcd.begin(16, 2);
}

static bool measure_environment(float *temperature, float *humidity) {
    static unsigned long measurement_timestamp = millis();

    /* Measure once every three seconds. */
    if (millis() - measurement_timestamp > 3000ul) {
        if (dht_sensor.measure(temperature, humidity) == true) {
            measurement_timestamp = millis();
            return (true);
        }
    }

    return (false);
}

void play_melody(int index) {
    tone(BUZZER_PIN, melody[index], duration);
}

void translateIR() {
    if (irrecv.decode(&results)) {
        Serial.print("IR code:0x");
        Serial.println(results.value, HEX);
        switch (results.value) {
            case 0x5D4F13AA:  // POWER button
                Serial.println("POWER");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Temp: ");
                lcd.print(temperature, 1);
                break;

            case 0x27E434F3:  // FUNC/STOP button
                Serial.println("FUNC/STOP");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Humidity: ");
                lcd.print(humidity, 1);
                break;

            case 0x2D7E2EC1:  // VOL+ button
                Serial.println("VOL+");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Light: ");
                lcd.print(light, 1);
                break;

            case 0xEA15FF00:  // VOL- button
                Serial.println("VOL-");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Soil Moisture: ");
                lcd.print(moisture, 1);
                break;

            case 0xF609FF00:  // UP button
                Serial.println("UP");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Height: ");
                lcd.print(plant_height, 1);
                break;

            case 0xF807FF00:  // DOWN button
                Serial.println("DOWN");
                adjust_servo_position(0);  // Move servo to 0 degrees
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Servo at 0");
                break;

            case 0xF20DFF00:  // ST/REPT button
                Serial.println("ST/REPT");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Special Msg");
                lcd.setCursor(0, 1);
                lcd.print("Custom Action");
                break;

            case 0xE916FF00:  // 0 button
                Serial.println("0");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Button 0 Pressed");
                break;

            // Add more cases for other buttons if needed...

            default:
                Serial.println("Other button");
                break;
        }
        irrecv.resume(); // Receive the next value
        delay(500);
    }
}

void read_accelerometer() {
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x3B);  // Starting with register 0x3B (ACCEL_XOUT_H)
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_addr, 6, true);  // Request a total of 6 registers (for X, Y, Z)

    AcX = Wire.read() << 8 | Wire.read();
    AcY = Wire.read() << 8 | Wire.read();
    AcZ = Wire.read() << 8 | Wire.read();

    // Only display accelerometer data if there's a significant change
    if (abs(AcX - prevAcX) > 250 || abs(AcY - prevAcY) > 250 || abs(AcZ - prevAcZ) > 250) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("X: "); lcd.print(AcX);
        lcd.setCursor(0, 1);
        lcd.print("Y: "); lcd.print(AcY);
        lcd.setCursor(8, 1);
        lcd.print(" Z: "); lcd.print(AcZ);
        delay(1000);

        // Update the last readings
        prevAcX = AcX;
        prevAcY = AcY;
        prevAcZ = AcZ;
    }
}

void adjust_servo_position(int position) {
    myservo.write(position);
    delay(1000);  // Wait for the servo to reach the position
}

void loop() {
    // IR remote handling
    translateIR();

    // Get plant height from ultrasonic sensor
    long plant_height = sr04.Distance();

    // Read sensors
    int moisture = analogRead(MOISTURE_SENSOR_PIN);
    int light = analogRead(LIGHT_SENSOR_PIN);
    float temperature, humidity;
    if (measure_environment(&temperature, &humidity) == true) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("T = ");
        lcd.print(temperature, 1);
        lcd.print(" C");

        lcd.setCursor(0, 1);
        lcd.print("H = ");
        lcd.print(humidity, 1);
        lcd.print(" %");

        delay(3000);  // Display temp and humidity for 3 seconds

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Soil: ");
        lcd.print(moisture);

        lcd.setCursor(0, 1);
        lcd.print("Light: ");
        lcd.print(light);

        delay(3000);  // Display soil and light for 3 seconds

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Water: ");
        if (moisture <= 500 || light >= 500 || temperature >= 35) {
            lcd.print("A lot");
            play_melody(0);
            adjust_servo_position(0);  // Low water level
        } else if (moisture <= 899 || light >= 250 || temperature >= 23) {
            lcd.print("Mid, add");
            lcd.setCursor(0, 1);
            lcd.print("200 mL water");
            play_melody(1);
            adjust_servo_position(90);  // Medium water level
        } else {
            lcd.print("Enough");
            play_melody(2);
            adjust_servo_position(180);  // Enough water level
        }
        delay(3000);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Height: ");
        lcd.print(plant_height);
        delay(2000);
    }

    // Read accelerometer data
    read_accelerometer();
}




        int pot_volume_ml = 1000;  // Example: 1000 mL pot
        int plant_size = 50;       // Example: scale of 1-100 for plant size
        int water_amount = 0; // Initialize water amount

        if (moisture <= 500 || light >= 500 || temperature >= 35) {
            lcd.print("A lot");
            play_melody(0);
            adjust_servo_position(0);  // Low water level
            water_amount = pot_volume_ml * 0.3;  // 30% of pot volume
        } else if (moisture <= 899 || light >= 250 || temperature >= 23) {
            lcd.print("Mid, add");
            play_melody(1);
            adjust_servo_position(90);  // Medium water level
            water_amount = pot_volume_ml * 0.2;  // 20% of pot volume
        } else {
            lcd.print("Enough");
            play_melody(2);
            adjust_servo_position(180);  // Enough water level
        }

        // Adjust water amount based on plant size
        water_amount = water_amount * (plant_size / 100.0);

        // Display water amount in mL on LCD
        lcd.setCursor(0, 1);
        lcd.print(water_amount);
        lcd.print(" mL water");
