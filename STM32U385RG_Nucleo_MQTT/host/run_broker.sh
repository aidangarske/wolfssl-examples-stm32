#!/bin/bash
# run_broker.sh — Start a local wolfMQTT broker with mTLS on port 8883.
#
# Expects certs/ to contain CA + server cert/key (from gen_test_certs.sh).
#
# The broker uses wolfSSL for TLS and requires client certificates (mutual auth).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOP="$(dirname "$SCRIPT_DIR")"
CERT_DIR="$TOP/certs"
WOLFMQTT_ROOT="${WOLFMQTT_ROOT:-$(realpath "$TOP/../../wolfmqtt")}"

if [ ! -f "$CERT_DIR/server-cert.pem" ]; then
    echo "Error: certs not found. Run gen_test_certs.sh first."
    exit 1
fi

BROKER="$WOLFMQTT_ROOT/src/mqtt_broker"
if [ ! -x "$BROKER" ]; then
    echo "Broker not found at $BROKER"
    echo "Build with: cd $WOLFMQTT_ROOT && ./configure --enable-mqtt-broker && make"
    exit 1
fi

echo "Starting wolfMQTT broker on :8883 (TLS 1.3, mTLS required)"
echo "  CA:     $CERT_DIR/ca-cert.pem"
echo "  Server: $CERT_DIR/server-cert.pem"
echo "  Key:    $CERT_DIR/server-key.pem"

exec "$BROKER" \
    -t \
    -s 8883 \
    -V 13 \
    -A "$CERT_DIR/ca-cert.pem" \
    -c "$CERT_DIR/server-cert.pem" \
    -K "$CERT_DIR/server-key.pem" \
    -v 3
