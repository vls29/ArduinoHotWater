#include <SPI.h>
#include <Ethernet.h>

///////// CHANGEABLE VALUES /////////

const char serverAddress[] = "home-monitoring.scaleys.co.uk";
const int serverPort = 80;

const char serviceEndpoint[] = "/hotwater";

const unsigned long millisecondsBetweenCalls = 60000L;

const double temperatureOffset = 0.0;

const double temperatureMultiplier = 1.252;
const double temperatureCalculationOffset = 1.188;

///////// CHANGEABLE VALUES ABOVE /////////

EthernetClient ethernetClient;
const byte mac[] = {0x90, 0xA0, 0xDA, 0x0E, 0x9B, 0xE5};

unsigned long counter = 1L;
unsigned long cReadings = 0L;

const int temperatureSensorPin = A0;
const double analogueRange = 1024.0;
const double voltage = 5.0;
const double offset = 0.5;
const double milliVolts = 100.0;

// timing stuff
unsigned long lastTimeUploaded = millis();
unsigned long previousTime = 0UL;

// immersion sensor
const int immersionSensorPin = A5;
unsigned long onCount = 0L;
unsigned long offCount = 0L;
const unsigned int threshold = 20L;

void setup() {
  Serial.begin(9600);
  connectToEthernet();
}

void connectToEthernet()
{
  unsigned long millisecondsPerMinute = 60000;

  // attempt to connect to Wifi network:
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP waiting 1 minute");
    delay(millisecondsPerMinute);

    if (Ethernet.begin(mac) == 0)
    {
      Serial.println("Failed to configure Ethernet using DHCP waiting 1 more minute");
      delay(millisecondsPerMinute);

      if (Ethernet.begin(mac) == 0) {
        Serial.println("Failed to configure Ethernet using DHCP stopping - will need reset");
        while (true);
      }
    }

  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("connecting...");

  Serial.print("Connected to the network IP: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  readTemperatureSensorValue();
  readImmersionPlugSensorValue();

  if (isTimeToUploadData())
  {
    Serial.println("Uploading data");
    sendResultsToServer();
    resetReadingsAfterUpload();
  }

  delay(1);
}

void readImmersionPlugSensorValue()
{
  int sensorVal = analogRead(immersionSensorPin);

  calculateOnOffStatus(sensorVal);
}

void resetImmersionPlugSensorCounts()
{
  onCount = 0L;
  offCount = 0L;
}

void calculateOnOffStatus(int sensorVal)
{
  if ( sensorVal < threshold)
  {
    offCount++;
  }
  else
  {
    onCount++;
  }
}

boolean isTimeToUploadData() {
  unsigned long currentTime = millis();

  if (currentTime < previousTime)  {
    lastTimeUploaded = currentTime;
  }

  previousTime = currentTime;

  if ( (currentTime - lastTimeUploaded) >= millisecondsBetweenCalls) {
    Serial.println("Time to upload");
    lastTimeUploaded = currentTime;
    return true;
  }
  return false;
}

int getPeriodImmersionOnOffStatus()
{
  Serial.println("On Count: " + String(onCount) + " Off Count: " + String(offCount));

  if (onCount > offCount)
  {
    return 10;
  }

  return 0;
}

/* Reads the temperature sensor */
void readTemperatureSensorValue() {
  int sensorVal = analogRead(temperatureSensorPin);
  cReadings = cReadings + sensorVal;
  counter++;
}

void resetReadingsAfterUpload()
{
  resetTemperatureSensorCounts();
  resetImmersionPlugSensorCounts();
}

void resetTemperatureSensorCounts()
{
  counter = 1L;
  cReadings = 0L;
}

double calculateAverageTemperatureOverPeriod()
{
  double averageSensorValue = averageSensorVal();

  // convert the ADCreading to voltage
  double voltageAv = (averageSensorValue / analogueRange) * voltage;

  double temperatureAv = (voltageAv - offset) * milliVolts;
  temperatureAv = ((temperatureMultiplier * (temperatureAv + temperatureOffset)) + temperatureCalculationOffset);

  Serial.println("Sensor Value: " + String(averageSensorValue) + ", Av Volts: " + String(voltageAv) + ", degrees C: " + String(temperatureAv));

  return temperatureAv;
}

double averageSensorVal()
{
  return (double)cReadings / (double)counter;
}

void sendResultsToServer() {
  Serial.println("sendResultsToServer");

  String postData = getPostData();
  Serial.println("post data: " + postData);

  if (ethernetClient.connect(serverAddress, serverPort)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    ethernetClient.println("POST " + String(serviceEndpoint) + " HTTP/1.1");
    ethernetClient.println("Host: " + String(serverAddress) + ":" + serverPort);
    ethernetClient.println("Content-Type: application/json");
    ethernetClient.println("Content-Length: " + String(postData.length()));
    ethernetClient.println("Pragma: no-cache");
    ethernetClient.println("Cache-Control: no-cache");
    ethernetClient.println("Connection: close");
    ethernetClient.println();

    ethernetClient.println(postData);
    ethernetClient.println();

    delay(10);
    ethernetClient.stop();
    ethernetClient.flush();
    Serial.println("Called server");
  }
}

String getPostData()
{
  double averagedTemperature = calculateAverageTemperatureOverPeriod();
  Serial.print("temp to post is: " + String(averagedTemperature));

  char tempChar[10];
  dtostrf(averagedTemperature, 3, 2, tempChar);
  return "{\"t\":" + String(tempChar) + ",\"i\":" + getPeriodImmersionOnOffStatus() + "}";
}
