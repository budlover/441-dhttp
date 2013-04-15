#!/usr/bin/python

from socket import *
import sys
import random
import os

if __name__ == '__main__':
    if len(sys.argv) < 1:
        sys.stderr.write('bu ke neng...\n')
        sys.exit(-1)

    rd_port  = 9996

    sock = socket(AF_INET, SOCK_STREAM)
    sock.bind(('', rd_port))
    sock.listen(5)

    while 1:
        (client_sock, address) = sock.accept()

        req = client_sock.recv(500)

        if req == 'GETRD o2.txt':
            resp = 'http://localhost:9999/static/5c9ab7cde10c01482faf43176a3a8ed443fb35687964df9944d169ad9df302ec'
        elif req == 'GETRD o1.txt':
            resp = 'http://localhost:9999/static/ee82b309a9243292c664739ffd5960ccdab05924fa6bab4ba29b0a4d47686075'
        else:
            resp = 'not found'

        client_sock.send(resp)
