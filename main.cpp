
#include "mbed.h"
#include "ESP8266Interface.h"
#include <MQTTClientMbedOs.h>

PwmOut pwmMTD00(D6);        // Moottorin pwm-ohjaus
PwmOut pwmMTD01(D5);

DigitalOut LED(D1);  
SPI spi(D11, D12, D13); //  valoanturin pinnit
DigitalOut alsCS(D4);   
AnalogIn pot(A3);

DigitalOut in00(D9);    // pinnit moottori1
DigitalOut in01(D10); 
DigitalOut in10(D2);    // pinnit moottori2
DigitalOut in11(D3);

//int spCount = 1;
//int spCountStep = 1;
float speed = 1.0;   // from 0.0 to 1.0 
int usT = 1000;   // f = 1000 Hz  T = 1 ms = 1000 us // f = 10000 Hz  T = 100 us

// muut variablet
char buffer[128];
int als = 0;         
int getALS();
float getSpeed();
void turnLeft();
void turnRight();
void forwardLoop();

bool moving = false;

int main()
{
    spi.format(8,0);           
    spi.frequency(12000000);    // Valoanturin juttuja
    alsCS.write(1);

    ESP8266Interface esp(MBED_CONF_APP_ESP_TX_PIN, MBED_CONF_APP_ESP_RX_PIN);
    SocketAddress deviceIP;         // device ja broker ip talteen
    SocketAddress MQTTBroker;
    TCPSocket socket;
    MQTTClient client(&socket);
    
    ThisThread::sleep_for(3s);    
    
    printf("\nConnecting wifi..\n");
 
    int ret = esp.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
 
    if(ret != 0)
    {
        printf("\nConnection error\n");
    }
    else
    {
        printf("\nConnection success\n");
    }
        
        
    esp.get_ip_address(&deviceIP);
    printf("Laitteen IP-osoite: %s\n", deviceIP.get_ip_address());
 
    esp.gethostbyname(MBED_CONF_APP_MQTT_BROKER_HOSTNAME, &MQTTBroker, NSAPI_IPv4, "esp");
    MQTTBroker.set_port(MBED_CONF_APP_MQTT_BROKER_PORT);
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.MQTTVersion = 3;
    data.clientID.cstring = MBED_CONF_APP_MQTT_CLIENT_ID;
    data.username.cstring = MBED_CONF_APP_MQTT_AUTH_METHOD;
    data.password.cstring = MBED_CONF_APP_MQTT_AUTH_TOKEN;
 
    MQTT::Message msg;          // Nämä kaiketi vaaditaan kaikki MQTT-viestin muodostamiseen...
    msg.qos = MQTT::QOS0;
    msg.retained = false;
    msg.dup = false;
    msg.payload = (void*)buffer;
    msg.payloadlen = strlen(buffer);
    ThisThread::sleep_for(5s);  


    // Yhdistäminen mqtt brokeriin joka määritelty jsonissa
    printf("Svetlana yhtyy:  %s \n\n", MBED_CONF_APP_MQTT_BROKER_HOSTNAME);
    socket.open(&esp);
    socket.connect(MQTTBroker);
    client.connect(data);

    pwmMTD00.period_us(usT); //pwm
    // pwmMTD00.write(0.2f);
    pwmMTD01.period_us(usT);
    // pwmMTD01.write(0.2f);

    while(1)  {                         
        if (getALS() > 70) {
            forwardLoop();
        }

        else {
            pwmMTD00.write(0);
            pwmMTD01.write(0);
            printf("Svetlana on seis! %i \n", als);

            sprintf(buffer, "{\"d\":{\"Tankki\":\"Svetlana\",\"Liike\":\"0 \",\"Valoanturin arvo:\":%i}}", als);  // JSON-muotoisen viestin muodostus. 
            msg.payload = (void*)buffer;                             
            msg.payloadlen = strlen(buffer);                        
            client.publish("iot-2/evt/Svetlana/fmt/json", msg);     // Oikea formaatti

            moving = false;
        }
        ThisThread::sleep_for(1s);
    }
}

float getSpeed() {
        als = getALS();
        return (float)als / 255;
}

void turnLeft() {
    pwmMTD00.write(1.0f);
    pwmMTD01.write(1.0f);
    in00.write(false);
    in01.write(true);
    in10.write(false);
    in11.write(true)
}

void turnRight() {
    pwmMTD00.write(1.0f);
    pwmMTD01.write(1.0f);
    in00.write(true);
    in10.write(false);
    in10.write(true);
    in11.write(false);
}

void forwardLoop() {
    while (getALS() > 70) {
        speed = getSpeed();
        printf("Svetlanan vauhti kiihtyy! %i \n", als);
        pwmMTD00.write(speed);
        pwmMTD01.write(speed);
        in00.write(true); // forward moottori1
        in01.write(false);
        in10.write(false); // forward moottori2
        in11.write(true);

        /*
        sprintf(buffer, "{\"d\":{\"Tankki\":\"Svetlana\",\"Liike\":\"1 \",\"Valoanturin arvo:\":%i}}", als);  // JSON-muotoisen viestin muodostus. 
        msg.payload = (void*)buffer;                             
        msg.payloadlen = strlen(buffer);                        
        client.publish("iot-2/evt/Svetlana/fmt/json", msg);     // Oikea formaatti
        */

        moving = true;
        ThisThread::sleep_for(1s);
    }

    if (getALS() < 70 && moving == true)
    {
        turnLeft();
        ThisThread::sleep_for(5s);
        if (getALS() > 70) {
            forwardLoop();
        }
        else {
            turnRight();
            ThisThread::sleep_for(10s);
            if (getALS() > 70) {
                forwardLoop();
            }
        }
    }
}

int getALS()    // Funktio palauttaa valoanturin arvon
{
    char alsByte0 = 0;      
    char alsByte1 = 0;    
    char alsByteSh0 = 0;
    char alsByteSh1 = 0;
    char als8bit = 0;
    unsigned short alsRaw = 0;  
    alsCS.write(0); 
    alsByte0 = spi.write(0x00);
    alsByte1 = spi.write(0x00);
    alsCS.write(1);
    alsByteSh0 = alsByte0 << 4;
    alsByteSh1 = alsByte1 >> 4;
    als8bit =( alsByteSh0 | alsByteSh1 );
    alsRaw = als8bit; 

    return (int)alsRaw;             // Palauttaa alsin raaka-arvon väliltä 0-255
}