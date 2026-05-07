#!/usr/bin/env python3
"""uart_bridge.py — bidirectional UART <-> TCP bridge.

Usage:
    ./uart_bridge.py /dev/ttyACM0 localhost:8883

Forwards raw bytes between a serial port (the STM32U385 USART1 VCP) and a TCP
endpoint (the local wolfMQTT broker on port 8883). No framing, no escaping —
MQTT/TLS records ride the wire verbatim.
"""

import sys
import socket
import threading
import serial


def serial_to_tcp(ser, sock):
    try:
        while True:
            data = ser.read(1)
            if data:
                data += ser.read(ser.in_waiting or 0)
                sock.sendall(data)
    except Exception as e:
        print(f"[serial->tcp] {e}")


def tcp_to_serial(sock, ser):
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            ser.write(data)
    except Exception as e:
        print(f"[tcp->serial] {e}")


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <serial-port> <host:port>")
        sys.exit(1)

    serial_port = sys.argv[1]
    host, port = sys.argv[2].rsplit(":", 1)
    port = int(port)

    ser = serial.Serial(serial_port, 115200, timeout=1)
    print(f"Opened {serial_port} @ 115200")

    sock = socket.create_connection((host, port))
    print(f"Connected to {host}:{port}")

    t1 = threading.Thread(target=serial_to_tcp, args=(ser, sock), daemon=True)
    t2 = threading.Thread(target=tcp_to_serial, args=(sock, ser), daemon=True)
    t1.start()
    t2.start()

    try:
        t1.join()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        sock.close()
        print("Bridge closed")


if __name__ == "__main__":
    main()
