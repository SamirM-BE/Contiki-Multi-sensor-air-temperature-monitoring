#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket
import re
import time


HOST = '127.0.0.1'
PORT = 60001

rcvd_values = []
receiversCnt = {}
bufferOfAddrsReady = []


def handle_line(line):
    global receiver_addr
    res = False
    regex = r"DATA: (?P<src_addr>\d+.\d+), (?P<rcvd_minute>\d+.\d+), (?P<rcvd_temperature>\d+.\d+)"
    match = re.match(r"DATA: (?P<src_addr>\d+\.\d+), (?P<rcvd_minute>\d+), (?P<rcvd_temperature>\d+)", line)
    if match is None:
        return res
    else:
        rcvd_minute = int(match.group('rcvd_minute'))
        rcvd_temperature = float(match.group('rcvd_temperature'))
        src_addr = match.group('src_addr')
        rcvd_values.append((rcvd_minute, rcvd_temperature, src_addr))
        print(
            "Donnees recuperees, min: {} temperature: {}  src_addr: {}".format(rcvd_minute, rcvd_temperature, src_addr))

        handleReceiversCnt(src_addr)  # we maintain a dictionary with the senders addrs and the nb of data send

        if (receiversCnt[src_addr] % 30) == 0:

            src_addr_vals = getValsOfAddr(src_addr)  # we get the values from a particular addr
            if leastSquare([x[0] for x in src_addr_vals],[x[1] for x in src_addr_vals]) == 1:  # leastSquare on those values
                res = True
                bufferOfAddrsReady.append(src_addr+'\n')

            # in the 3 next lines we remove from the set of received values, the values that we have already computed
            tempo = [x for x in rcvd_values if x[2] != src_addr]
            del rcvd_values[:]
            if tempo:
                rcvd_values.extend(tempo)
            return res
    return res


# the purpose of this function is to get the set of values corresponding to a particular address
def getValsOfAddr(from_addr):
    tempo = [x for x in rcvd_values if x[2] == from_addr]
    tempo.sort()
    return tempo


# the purpose of this function is to maintain the receiversCnt dictionnary
def handleReceiversCnt(from_addr):
    if from_addr in receiversCnt:
        receiversCnt[from_addr] = receiversCnt[from_addr] + 1
    else:
        receiversCnt[from_addr] = 1


def leastSquare(minutes, temperatures):
    treshold = 0.2
    sumx = 0.0
    sumx2 = 0.0
    sumy = 0.0
    sumyx = 0.0
    a = 0.0
    i = 1
    n = 30

    for i in range(0, n):
        sumx = sumx + minutes[i]
        sumx2 = sumx2 + minutes[i] * minutes[i]
        sumy = sumy + temperatures[i]
        sumyx = sumyx + temperatures[i] * minutes[i]

    a = (n * sumyx - sumx * sumy) / (n * sumx2 - sumx * sumx)
    print('SLOPE IS EQUAL TO: ', a)
    if a > treshold:
        return 1
    else:
        return 0


with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen(PORT)
    conn, addr = s.accept()
    with conn:
        print('Connected by', addr)
        line = ""
        while True:
            data = (conn.recv(1024)).decode(
                "utf-8")
            if data != "\n":
                line += data
            else:
                line += data
                tempo = handle_line(line)
                #print("tempo is equal to: ",tempo)

                if tempo and bufferOfAddrsReady:
                    toSend = bufferOfAddrsReady.pop(0)
                    #time.sleep(10)
                    conn.sendall(bytes(toSend, encoding='utf-8'))
                line = ""
            if not data:
                break

