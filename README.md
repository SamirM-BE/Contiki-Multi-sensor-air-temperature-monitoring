# Project course LINGI2146
This repository contains the source code for the project of Mobile and Embedded computing

#### Contributors :
@[SamirM-BE](https://github.com/SamirM-BE "SamirM-BE"), @[olivierperdaens](https://github.com/olivierperdaens "olivierperdaens"), @[Djafa](https://github.com/Djafa "Djafa")

## How to run?

You have to clone this git **in the root of the contiki folder** for it to work properly.
### Install
the server requires python to run.
install it with
```bash
sudo apt-get install python3-minimal
```

### Execute
run
```bash
python3 server.py
```
At this point, you can create any network in Cooja. 
**To connect to the server, please use the serial socket of the border router as a CLIENT with the port 60001.** 


### Network elements
* sensor-node.c ==> this is a sensor node
* compute-node.c ==> this is a computational node
* border-router.c ==> this is a border router, the border router is mandatory to communicate with the server

### Network example
<img src="https://github.com/SamirM-BE/mobileP2/blob/master/networkExample.png" width="500" height="500">
If you want to run this network, please import the networkExample.csc file in Cooja 
