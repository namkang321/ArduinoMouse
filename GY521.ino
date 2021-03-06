#include "Wire.h"
#include "Ultrasonic.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const uint8_t MPU_addr = 0x68; // I2C address of the MPU-6050

const float MPU_GYRO_250_SCALE = 131.0;
const float MPU_GYRO_500_SCALE = 65.5;
const float MPU_GYRO_1000_SCALE = 32.8;
const float MPU_GYRO_2000_SCALE = 16.4;
const float MPU_ACCL_2_SCALE = 16384.0;
const float MPU_ACCL_4_SCALE = 8192.0;
const float MPU_ACCL_8_SCALE = 4096.0;
const float MPU_ACCL_16_SCALE = 2048.0;

const char * ssid = "NISARUJ";
const char * pass = "87654321";
const String IP = "172.20.10.5";

struct rawdata {
  int16_t AcX;
  int16_t AcY;
  int16_t AcZ;
  int16_t Tmp;
  int16_t GyX;
  int16_t GyY;
  int16_t GyZ;
};

struct scaleddata {
  float AcX;
  float AcY;
  float AcZ;
  float Tmp;
  float GyX;
  float GyY;
  float GyZ;
};

bool calb = true;
bool screenCalb = true;
rawdata calbRawdata;
scaleddata calbScaleddata;
scaleddata data;
int dist;

int TouchSensor = 14;
int triggerButton = 15;

double timeNow = millis();
double timePrev = timeNow;
double deltaTime = 0.0;

double X_SENSITIVITY = 2.5;
double Y_SENSITIVITY = 2.5;
double Z_SENSITIVITY = 2.5;

double x = 0, y = 0, z = 0;

bool checkI2c(byte addr);
void mpu6050Begin(byte addr);
rawdata mpu6050Read(byte addr, bool Debug);
void setMPU6050scales(byte addr, uint8_t Gyro, uint8_t Accl);
void getMPU6050scales(byte addr, uint8_t & Gyro, uint8_t & Accl);
scaleddata convertRawToScaled(byte addr, rawdata data_in, bool Debug);

Ultrasonic ultrasonic(12, 13);

void setup() {
  Wire.begin();
  Serial.begin(115200);
  Serial.print("Start");
  pinMode(TouchSensor, INPUT);
  pinMode(triggerButton, INPUT);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting WIFI ");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("Wi-Fi connected");
  Serial.print("IP Address : ");
  Serial.println(WiFi.localIP()); //แสดง IP ของบอร์ดที่ได้รับแจกจาก AP

  mpu6050Begin(MPU_addr);
}

void loop() {
  timePrev = timeNow;
  timeNow = millis();
  deltaTime = (timeNow - timePrev) / 1000;
  if (digitalRead(TouchSensor) == HIGH) {
    Serial.println("Touched");
    setMPU6050scales(MPU_addr, 0b00000000, 0b00010000);
    calbRawdata = mpu6050Read(MPU_addr, false);
    calbScaleddata = convertRawToScaled(MPU_addr, calbRawdata, false);
    calb = false;
  }
  if (!calb) {
    rawdata next_sample;
    setMPU6050scales(MPU_addr, 0b00000000, 0b00010000);
    next_sample = mpu6050Read(MPU_addr, false);
    data = convertRawToScaled(MPU_addr, next_sample, false);
    Serial.print("Alpha: " + String(data.GyX + 180) + " Beta: " + String(data.GyZ) + " Gamma: " + String(data.GyY) + " Dist: ");
    dist = ultrasonic.Ranging(CM);
    Serial.println(dist);

    if (digitalRead(triggerButton) == HIGH) {
      Serial.println("SHOOT");
    }

    if (screenCalb && digitalRead(triggerButton) == HIGH) {
      Serial.println("Alpha: " + String(data.GyX + 180) + " Beta: " + String(data.GyZ));
      HTTPClient http; //Declare an object of class HTTPClient

      http.begin("http://" + IP + ":5000/calibrate/" + String((int) data.GyX + 180) + "/" + String((int) data.GyZ) + "/0/" + String(dist)); //Specify request destination
      int httpCode = http.GET(); //Send the request

      if (httpCode > 0) { //Check the returning code
        String payload = http.getString(); //Get the request response payload
        Serial.println(payload); //Print the response payload
        if (payload == "Finished") {
          screenCalb = false;
        }
      }
      http.end(); //Close connection
    }
    if (!screenCalb) {
      Serial.println("Alpha: " + String(data.GyX + 180) + " Beta: " + String(data.GyZ));
      HTTPClient http; //Declare an object of class HTTPClient

      http.begin("http://" + IP + ":5000/shoot/" + String((int) data.GyX + 180) + "/" + String((int) data.GyZ) + "/0/" + String(dist) + "/" + (digitalRead(triggerButton) == HIGH ? "1" : "0")); //Specify request destination
      int httpCode = http.GET(); //Send the request

      if (httpCode > 0) { //Check the returning code
        String payload = http.getString(); //Get the request response payload
        Serial.println(payload); //Print the response payload
      }
      http.end(); //Close connection

    }
    //
  }

  /*if (digitalRead(TouchSensor) == HIGH && !calb) {
    calb = true;
  }*/
  //Serial.println(deltaTime);
  delay(100);
}

void mpu6050Begin(byte addr) {
  // This function initializes the MPU-6050 IMU Sensor
  // It verifys the address is correct and wakes up the
  // MPU.
  if (checkI2c(addr)) {
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x6B); // PWR_MGMT_1 register
    Wire.write(0); // set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);

    delay(30); // Ensure gyro has enough time to power up
  }
}

bool checkI2c(byte addr) {
  // We are using the return value of
  // the Write.endTransmisstion to see if
  // a device did acknowledge to the address.
  Serial.println(" ");
  Wire.beginTransmission(addr);

  if (Wire.endTransmission() == 0) {
    Serial.print(" Device Found at 0x");
    Serial.println(addr, HEX);
    return true;
  } else {
    Serial.print(" No Device Found at 0x");
    Serial.println(addr, HEX);
    return false;
  }
}

rawdata mpu6050Read(byte addr, bool Debug) {
  // This function reads the raw 16-bit data values from
  // the MPU-6050

  rawdata values;

  Wire.beginTransmission(addr);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(addr, 14, true); // request a total of 14 registers
  values.AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  values.AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  values.AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  values.Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  values.GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  values.GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  values.GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  if (Debug) {
    Serial.print(" GyX = ");
    Serial.print(values.GyX);
    Serial.print(" | GyY = ");
    Serial.print(values.GyY);
    Serial.print(" | GyZ = ");
    Serial.print(values.GyZ);
    Serial.print(" | Tmp = ");
    Serial.print(values.Tmp);
    Serial.print(" | AcX = ");
    Serial.print(values.AcX);
    Serial.print(" | AcY = ");
    Serial.print(values.AcY);
    Serial.print(" | AcZ = ");
    Serial.println(values.AcZ);
  }

  return values;
}

void setMPU6050scales(byte addr, uint8_t Gyro, uint8_t Accl) {
  Wire.beginTransmission(addr);
  Wire.write(0x1B); // write to register starting at 0x1B
  Wire.write(Gyro); // Self Tests Off and set Gyro FS to 250
  Wire.write(Accl); // Self Tests Off and set Accl FS to 8g
  Wire.endTransmission(true);
}

void getMPU6050scales(byte addr, uint8_t & Gyro, uint8_t & Accl) {
  Wire.beginTransmission(addr);
  Wire.write(0x1B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(addr, 2, true); // request a total of 14 registers
  Gyro = (Wire.read() & (bit(3) | bit(4))) >> 3;
  Accl = (Wire.read() & (bit(3) | bit(4))) >> 3;
}

scaleddata convertRawToScaled(byte addr, rawdata data_in, bool Debug) {

  scaleddata values;
  scaleddata rvalues;
  float scale_value = 0.0;
  byte Gyro, Accl;

  getMPU6050scales(MPU_addr, Gyro, Accl);

  if (Debug) {
    Serial.print("Gyro Full-Scale = ");
  }

  switch (Gyro) {
  case 0:
    scale_value = MPU_GYRO_250_SCALE;
    if (Debug) {
      Serial.println("±250 °/s");
    }
    break;
  case 1:
    scale_value = MPU_GYRO_500_SCALE;
    if (Debug) {
      Serial.println("±500 °/s");
    }
    break;
  case 2:
    scale_value = MPU_GYRO_1000_SCALE;
    if (Debug) {
      Serial.println("±1000 °/s");
    }
    break;
  case 3:
    scale_value = MPU_GYRO_2000_SCALE;
    if (Debug) {
      Serial.println("±2000 °/s");
    }
    break;
  default:
    break;
  }
  if (!calb) {
    values.GyX = (float)(data_in.GyX) / scale_value - calbScaleddata.GyX;
    values.GyY = (float)(data_in.GyY) / scale_value - calbScaleddata.GyY;
    values.GyZ = (float)(data_in.GyZ) / scale_value - calbScaleddata.GyZ;
  } else {
    values.GyX = (float)(data_in.GyX) / scale_value;
    values.GyY = (float)(data_in.GyY) / scale_value;
    values.GyZ = (float)(data_in.GyZ) / scale_value;
    X_SENSITIVITY = abs(values.GyX) + 0.75;
    Y_SENSITIVITY = abs(values.GyY) + 0.75;
    Z_SENSITIVITY = abs(values.GyZ) + 0.75;

  }
  if (abs(values.GyX) < X_SENSITIVITY) values.GyX = 0;
  if (abs(values.GyY) < Y_SENSITIVITY) values.GyY = 0;
  if (abs(values.GyZ) < Z_SENSITIVITY) values.GyZ = 0;

  x += values.GyX * deltaTime;
  y += values.GyY * deltaTime;
  z += values.GyZ * deltaTime * (values.GyZ < 0 ? 0.5 : 1.2);

  if (calb) {
    Serial.println("WTF");
    x = 0;
    y = 0;
    z = 0;
  }
  double xp = x;
  double yp = y;
  double zp = z;

  scale_value = 0.0;
  if (Debug) {
    Serial.print("Accl Full-Scale = ");
  }
  switch (Accl) {
  case 0:
    scale_value = MPU_ACCL_2_SCALE;
    if (Debug) {
      Serial.println("±2 g");
    }
    break;
  case 1:
    scale_value = MPU_ACCL_4_SCALE;
    if (Debug) {
      Serial.println("±4 g");
    }
    break;
  case 2:
    scale_value = MPU_ACCL_8_SCALE;
    if (Debug) {
      Serial.println("±8 g");
    }
    break;
  case 3:
    scale_value = MPU_ACCL_16_SCALE;
    if (Debug) {
      Serial.println("±16 g");
    }
    break;
  default:
    break;
  }
  values.AcX = (float) data_in.AcX / scale_value;
  values.AcY = (float) data_in.AcY / scale_value;
  values.AcZ = (float) data_in.AcZ / scale_value;

  values.Tmp = (float) data_in.Tmp / 340.0 + 36.53;

  if (Debug) {
    Serial.print(" GyX = ");
    Serial.print(values.GyX);
    Serial.print(" °/s| GyY = ");
    Serial.print(values.GyY);
    Serial.print(" °/s| GyZ = ");
    Serial.print(values.GyZ);
    Serial.print(" °/s| Tmp = ");
    Serial.print(values.Tmp);
    Serial.print(" °C| AcX = ");
    Serial.print(xp);
    Serial.print(" g| AcY = ");
    Serial.print(y);
    Serial.print(" g| AcZ = ");
    Serial.print(zp);
    Serial.print(" g  SENSITIVITY");
    Serial.print(X_SENSITIVITY);
    Serial.print(" ");
    Serial.println(Z_SENSITIVITY);
  }

  rvalues.GyX = xp;
  rvalues.GyY = yp;
  rvalues.GyZ = zp;

  return rvalues;
}
