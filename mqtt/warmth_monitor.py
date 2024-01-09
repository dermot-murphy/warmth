# Notes
# Libraies to install:
#   pip install twilio
#   pip install paho-mqtt
# To run:
#   python warmth-checker.py

# MQTT Library
from paho.mqtt import client as mqtt_client

# Free cloud based service to send SMS messages to mobile phone
from twilio.rest import Client

# MQTT
MQTT_BROKER = 'test.mosquitto.org'
MQTT_PORT = 1883
MQTT_TOPIC = "warmth-checker"
MQTT_ACCESS_ID = f'subscribe-100'
MQTT_FIRST_RECONNECT_DELAY = 1
MQTT_RECONNECT_RATE = 2
MQTT_MAX_RECONNECT_COUNT = 12
MQTT_MAX_RECONNECT_DELAY = 60

mqtt_userdata = ""
mqtt_flags = ""

# SMS Configuration (from account creation on Twilio)
SMS_FROM_TELEPHONE_NUMBER = 447700139265
SMS_TO_TELEPHONE_NUMBER = 7801841668
SMS_ACCOUNT_SID = 'ACf8f2af6aefde208d1a9a22f730e1c0b2'
SMS_AUTH_TOKEN = '236c181f880f44823f65ba87741bb35d'

# Client state (ok, hot, cold, etc)
client_previous_state = ""
client_current_state = ""

def Send_SMS(state):

    global SMS_FROM_TELEPHONE_NUMBER
    global SMS_TO_TELEPHONE_NUMBER
    global SMS_ACCOUNT_SID
    global SMS_AUTH_TOKEN
    
    client = Client(SMS_ACCOUNT_SID, SMS_AUTH_TOKEN)

    message = client.messages \
        .create(
             body=state,
             from_ =  SMS_FROM_TELEPHONE_NUMBER,
             to = SMS_TO_TELEPHONE_NUMBER
         )

    print(message.sid)

def connect_mqtt() -> mqtt_client:
    def on_connect(client, mqtt_userdata, mqtt_flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)

    client = mqtt_client.Client(MQTT_ACCESS_ID)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.connect(MQTT_BROKER, MQTT_PORT)
    return client

def on_disconnect(client, userdata, rc):
    logging.info("Disconnected from MQTT broker with result code: %s", rc)
    reconnect_count, reconnect_delay = 0, MQTT_FIRST_RECONNECT_DELAY
    while reconnect_count < MQTT_MAX_RECONNECT_COUNT:
        logging.info("Reconnecting in %d seconds...", reconnect_delay)
        time.sleep(reconnect_delay)

        try:
            client.reconnect()
            logging.info("Reconnected successfully!")
            return
        except Exception as err:
            logging.error("%s. Reconnect failed. Retrying...", err)

        reconnect_delay *= MQTT_RECONNECT_RATE
        reconnect_delay = min(reconnect_delay, MQTT_MAX_RECONNECT_DELAY)
        reconnect_count += 1
    logging.info("Reconnect failed after %s attempts. Exiting...", reconnect_count)
    
def subscribe(client: mqtt_client):
    def on_message(client, userdata, msg):
        global client_previous_state
        global client_current_state
        s = msg.payload.decode()
        print(f"Received `{s}` from `{msg.topic}` topic")
        client_previous_state = client_current_state
        client_current_state = "OK"
        msg = "CLIENT ROOM IS NOW OK"
        if s.find("HOT") >= 0:
            client_current_state = "TOO HOT"
            msg = "WARNING: CLIENT ROOM IS " + client_current_state
        if s.find("COLD") >= 0:
            client_current_state = "TOO COLD"
            msg = "WARNING: CLIENT ROOM IS " + client_current_state
        print ("CLIENT ROOM IS " + client_current_state)
        
        if client_current_state != client_previous_state:
            if client_current_state != "TEST":
                print ("Phoning next of kin")
                Send_SMS(msg)
           

    client.subscribe(MQTT_TOPIC)
    client.on_message = on_message


def run():
    client = connect_mqtt()
    subscribe(client)
    client.loop_forever()


if __name__ == '__main__':
    run()