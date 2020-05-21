#!/usr/bin/env python3

import socket
import re

HOST = '127.0.0.1'
PORT = 60001

minutes = []
temperatures = []


def handle_line(line):
    regex = r"DATA: (?P<src_addr>\d+.\d+), (?P<rcvd_minute>\d+.\d+), (?P<rcvd_temperature>\d+.\d+)"
    match = re.match(r"DATA: (?P<src_addr>\d+\.\d+), (?P<rcvd_minute>\d+), (?P<rcvd_temperature>\d+)", line)
    if match is None:
        return
    else:
        rcvd_minute = match.group('rcvd_minute')
        minutes.append(int(rcvd_minute))
        rcvd_temperature = match.group('rcvd_temperature')
        temperatures.append(float(rcvd_temperature))
        src_addr = match.group('src_addr')
        print(
            'Donnees recuperees, min: {} temperature: {}  src_addr: {}'.format(rcvd_minute, rcvd_temperature, src_addr))
        if len(minutes) == 30:
            if leastSquare(minutes, temperatures) == 1:
                return True
            del minutes[:]
            del temperatures[:]
            return False


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
                if handle_line(line):
                    conn.sendall(b'openValve\n')
                line = ""
            if not data:
                break
