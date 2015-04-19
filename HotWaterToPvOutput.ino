#include <SPI.h>
#include <Ethernet.h>

///////// CHANGEABLE VALUES /////////

char pompeii[] = "192.168.0.16";
int pompeiiPort = 80;

double minutesBetweenCalls = 1.0;

const double temperatureOffset = 0.0;

const double temperatureMultiplier = 1.252;
const double temperatureCalculationOffset = 1.188;


///////// CHANGEABLE VALUES ABOVE /////////

EthernetClient pompeiiClient;
byte mac[] = {
  0x90, 0xA0, 0xDA, 0x0E, 0x9B, 0xE6};
char pompeiiService[] = "/pvoutput-post-temp.php";

long counter = 1L;
long cReadings = 0L;

const int temperatureSensorPin = A0;
const double analogueRange = 1024.0;
const double voltage = 5.0;
const double offset = 0.5;
const double milliVolts = 100.0;

// timing stuff
unsigned long lastTimeUploaded = millis();

unsigned long millisecondsPerMinute = 60000;
unsigned long minutesInHour = 60;
unsigned long timeBetweenCalls = minutesBetweenCalls * millisecondsPerMinute;


// taos sensor
const int taosSensorPin = A5;
unsigned long onCount = 0L;
unsigned long offCount = 0L;
unsigned int threshold = 500L;

void setup() {
  Serial.begin(9600);
  connectToEthernet();
}

void connectToEthernet()
{
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
        while(true);
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

  //getTimeFromPompeii();
  if (isTimeToUploadData())
  {
    Serial.println("Uploading data");
    sendResultsToPompeii();
    resetReadingsAfterUpload();
  }

  delay(1);
}

void readImmersionPlugSensorValue()
{
  int sensorVal = analogRead(taosSensorPin);
  //Serial.println(sensorVal);

  calculateOnOffStatus(sensorVal);
}

void resetImmersionPlugSensorCounts()
{
  onCount = 0L;
  offCount = 0L;
}

void calculateOnOffStatus(int sensorVal)
{
  if( sensorVal < threshold)
  {
    offCount++;
  }
  else
  {
    onCount++;
  }
}

boolean isTimeToUploadData() {
  unsigned long time = millis();

  if( (time - lastTimeUploaded) >= timeBetweenCalls) {
    Serial.println("Time to upload");
    lastTimeUploaded = time;
    return true;
  }
  return false;
}

int getPeriodImmersionOnOffStatus()
{
  Serial.print("On Count: ");
  Serial.print(onCount);
  Serial.print(" Off Count: ");
  Serial.println(offCount);

  if( onCount > offCount)
  {
    return 10;
  }
  else
  {
    return 0;
  }
}

/* Reads the temperature sensor */
void readTemperatureSensorValue() {
  int sensorVal = analogRead(temperatureSensorPin);
  cReadings = cReadings + sensorVal;
  counter = counter + 1L;
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
  Serial.print("Sensor Value: ");
  Serial.print(averageSensorVal());

  // convert the ADCreading to voltage
  double voltageAv = (averageSensorVal()/analogueRange) * voltage;

  Serial.print(", Av Volts: ");

  Serial.print(voltageAv);
  Serial.print(", degrees C: ");
  double temperatureAv = (voltageAv - offset) * milliVolts;
  temperatureAv = ((temperatureMultiplier * (temperatureAv + temperatureOffset)) + temperatureCalculationOffset);
  Serial.println(temperatureAv);

  return temperatureAv;
}

double averageSensorVal()
{
  return (double)cReadings/(double)counter;
}

void sendResultsToPompeii() {
  Serial.println("sendResultsToPompeii");

  String postData = getPostData();
  Serial.println("post data: " + postData);

  if (pompeiiClient.connect(pompeii, pompeiiPort)) {
    Serial.println("connected to pompeii");
    // Make a HTTP request:
    pompeiiClient.print("POST ");
    pompeiiClient.print(pompeiiService);
    pompeiiClient.println(" HTTP/1.1");
    pompeiiClient.print("Host: ");
    pompeiiClient.print(pompeii);
    pompeiiClient.print(":");
    pompeiiClient.println(pompeiiPort);
    pompeiiClient.println("Accept: text/html");
    pompeiiClient.println("Content-Type: application/x-www-form-urlencoded; charset=UTF-8");
    pompeiiClient.print("Content-Length: ");
    pompeiiClient.println(postData.length());
    pompeiiClient.println("Pragma: no-cache");
    pompeiiClient.println("Cache-Control: no-cache");
    pompeiiClient.println("Connection: close");
    pompeiiClient.println("X-Data-Source: HOT_WATER");
    pompeiiClient.println();

    pompeiiClient.println(postData);
    pompeiiClient.println();

    pompeiiClient.stop();
    pompeiiClient.flush();
    Serial.println("Called pompeii");
  }
}

String getPostData()
{
  double averagedTemperature = calculateAverageTemperatureOverPeriod();
  Serial.print("temp to post is: ");
  Serial.println(averagedTemperature);

  char tempChar[10];
  dtostrf(averagedTemperature,3,2,tempChar);
  return "t=" + String(tempChar) + "&i=" + getPeriodImmersionOnOffStatus();
}
