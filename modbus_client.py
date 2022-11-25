import time
from threading import Thread, Lock
from pyModbusTCP.client import ModbusClient

SERVER_HOST = "192.168.23.123"
SERVER_PORT = 502

# set global
hregs = []
istsregs = []

# init a thread lock
regs_lock = Lock()
HREG_SERVO1 = 400
ISTS_REG = 100 

def polling_thread():
    global hregs, iregs, coils, istsregs, HREG_SERVO1, IREG_TEMP, COIL_PUMP, ISTS_REG, regs_lock

    c = ModbusClient(host=SERVER_HOST, port=SERVER_PORT, auto_open=True)
    # polling loop
    while True:

        # do modbus reading on socket
        hreg_list = c.read_holding_registers(HREG_SERVO1, 1)
        ists_list = c.read_discrete_inputs(ISTS_REG, 1)

        # if read is ok, store result in regs (with thread lock)
        if hreg_list:
            with regs_lock:
                hregs = list(hreg_list)
        if ists_list:
            with regs_lock:
                istsregs = list(ists_list)

        if istsregs:
            if istsregs[0] == True:
                c.write_single_register(HREG_SERVO1, 8500)

            elif istsregs[0] == False:
                c.write_single_register(HREG_SERVO1, 1500)

        print("1-HREG@40401:{}".format(hregs))
        print("4-ISTS@10101:{}".format(istsregs)) 
        time.sleep(0.5)
    

# start polling thread
tp = Thread(target=polling_thread)
# set daemon: polling thread will exit if main thread exit
tp.daemon = True
tp.start()


import time
import random
import paho.mqtt.client as mqtt

# Import Adafruit IO REST client.
from Adafruit_IO import Client, Feed, RequestError

ADAFRUIT_IO_KEY = ''
ADAFRUIT_IO_USERNAME = 'MylesNunez'
aio = Client(ADAFRUIT_IO_USERNAME, ADAFRUIT_IO_KEY)

global gdata1
global mqtt_ipsvr

# GPS Coordinates of a random place in Singapore
value = 40
lat = 1.395379
lon = 103.873538
ele = 6 # elevation above sea level (meters)
last_value = 0

gdata1 = 0.0
mqtt_ipsvr = "192.168.23.1"

# Assign a location feed, if one exists already
try:
    location = aio.feeds('location')
except RequestError: # Doesn't exist, create a new feed
    feed = Feed(name="location")
    location = aio.create_feed(feed)

metadata = {'lat':lat, 'lon':lon, 'ele':ele, 'created_at': time.asctime(time.gmtime()) }
aio.send_data(location.key, value, metadata)

# limit feed updates to every 3 seconds, avoid IO throttle
loop_delay = 3

def clientconn():
    global client
    global mqtt_ipsvr
    client = mqtt.Client("DESKTOP-8LHT5CI")
    client.on_connect = on_connect
    client.on_message = on_message
    print("mqtt_ipsvr addr={}".format(mqtt_ipsvr))
    client.connect(mqtt_ipsvr, 1883, 60)         

def on_connect(client, userdata, flags, rc):
    client.subscribe("test/temp1") 

def on_message(client, userdata, msg):
    global gdata1
    global last_value
    
    if msg.topic == "test/temp1":
        gdata1 = msg.payload.decode("utf-8")
        print('Message Received: {},{}\n'.format(msg.topic, gdata1)) 
        print("message temp1: {}".format(gdata1))
        print("value: {}".format(gdata1), "\nLook at the value!!")

        if gdata1 == last_value:
            return
        last_value = gdata1

        aio.send_data(location.key, gdata1)
    # wait loop_delay seconds to avoid api throttle
    time.sleep(loop_delay)   

if __name__ == '__main__':
    clientconn()
    client.loop_forever()
