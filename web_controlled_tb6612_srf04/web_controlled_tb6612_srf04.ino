#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <WiFiUdp.h>              //Used for direct UDP control

//Pins for TB6612 motor driver
enum pins
{
  APWM = 2,
  AIN_2 = 4,
  AIN_1 = 0,
  //STANDBY = 2,//Not used
  BIN_1 = 14,
  BIN_2 = 12,
  BPWM = 13
};

// defines pins numbers
const int trigPin = 15;
const int echoPin = 5;

// defines variables
long duration;
int distance;
long lastPing = 0;
long latch = 0;

ESP8266WebServer server(80);   //Web server object. Will be listening in port 80 (default for HTTP)

const char webPage[] = R"=====(
<!DOCTYPE html>
<html>
<head>
<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>
<script>
$(document).ready(function(){
    $("button").click(function(){
        $.get("/drive?x=" + x + "&z=" + z, function(data, status){});
    });
});
</script>
</head>
<body>
<button>Send an HTTP GET request to a page and get the result back</button>
<div id="zone_joystick" style="background-color: green;"></div>
<div id="output">Hello</div>
</body>
<script src="https://cdnjs.cloudflare.com/ajax/libs/nipplejs/0.6.8/nipplejs.min.js"></script>
<script type="text/javascript">
    var options = {
        zone: document.getElementById('zone_joystick'),
        color: 'blue',
        mode: 'static',
        position: {left: '50%', bottom: '50%'}
    };
    var nipple = nipplejs.create(options);
    var time = 0;
    nipple.on('move', function (evt, data) {
        var d = new Date();
        var n = d.getTime();
        if(n - time < 50){
            return;
        }
        time = n;
        //Move
        //output.innerHTML = "<pre>" + JSON.stringify(data.instance.options.size, null, 4) + "</pre>"
        z = data.instance.frontPosition.x / (data.instance.options.size / 2)
        x = -data.instance.frontPosition.y / (data.instance.options.size / 2)
        output.innerHTML = "<pre>X: " + x + "  Z: " + z + "</pre>"
        $.get("/drive?x=" + x + "&z=" + z, function(data, status){});
    }); 
    nipple.on('end', function (evt, data) {
        //Stop
        output.innerHTML = "<pre>Stop</pre>"
        x = 0
        z = 0
        $.get("/drive?x=" + x + "&z=" + z, function(data, status){});
    });
    
</script>
</html>
)=====";

/*
 * Convert speed to analog signals
 */
void driveMotor(int pwmPin, int forwardPin, int backwardPin, int motorSpeed){
  //Set direction
  if(motorSpeed > 0){
    digitalWrite(forwardPin, HIGH);
    digitalWrite(backwardPin, LOW);
  } else {
    digitalWrite(forwardPin, LOW);
    digitalWrite(backwardPin, HIGH);
  }
  //Set speed
  analogWrite(pwmPin, abs(motorSpeed));
}

/*
 * x - Forward speed 
 * z - Angular speed
 */
void driveMotors(float x, float z){
  float turnRate = z;
  float speed_right = x + turnRate;
  float speed_left = x - turnRate;
  driveMotor(APWM, AIN_1, AIN_2, speed_left * 1024);
  driveMotor(BPWM, BIN_1, BIN_2, speed_right * 1024);
}

void motorHandler(){
  //Use server x and z to drive motors - http://esp8266_ip_address/drive?x=1.0&z=-1.0
  driveMotors(server.arg("x").toFloat(), server.arg("z").toFloat());
  //Just echo back inputs and send a 200 OK
  server.send(200, "text/plain", server.arg("x") + " " + server.arg("z"));  
  latch = millis();
}

void rootPageHandler(){
  server.send(200, "text/plain", webPage);
}

void readDistance(){
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  // Calculating the distance
  distance= duration*0.034/2;
  // Prints the distance on the Serial Monitor
  Serial.print("Distance: ");
  Serial.println(distance);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Setting up motor driver...");
  delay(100);
  //Setup up pins for motor controller
  pinMode(AIN_1, OUTPUT);
  pinMode(AIN_2, OUTPUT);
  pinMode(BIN_1, OUTPUT);
  pinMode(BIN_2, OUTPUT);
  pinMode(APWM, OUTPUT);
  pinMode(BPWM, OUTPUT);
  //pinMode(STANDBY, OUTPUT);
  //digitalWrite(STANDBY, HIGH);
  Serial.println("Stopping motors...");
  delay(100);
  //Make sure motors are stopped
  driveMotors(0,0);

  //Setup wifi
  Serial.println("Setting up Wifi...");
  delay(100);
  WiFiManager wifiManager;
  wifiManager.autoConnect();
  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  //Listen for web api calls
  server.on("/", []() { server.send ( 200, "text/html", webPage );  });
  server.on("/drive", motorHandler);
  server.begin();

  
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

}

void loop() {
  //Listen on port 80 for web api
  server.handleClient();

  if(millis() > lastPing + 50){
    lastPing = millis();
    readDistance();
  }

  if(millis() > latch + 1000){
    if(distance > 200){
      latch = millis();
      driveMotors(-0.4, -0.3);
    } else if(distance > 30){
      driveMotors(0.6, 0);
    } else if(distance > 8) {
      driveMotors(0, 0.4);
      latch = millis();
    } else {
      latch = millis();
      driveMotors(-0.4, 0.3);
    }
  }
  
}
