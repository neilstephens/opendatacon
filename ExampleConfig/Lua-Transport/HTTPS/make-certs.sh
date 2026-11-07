#!/bin/sh
# Makes THROW-AWAY certificates for this example with the openssl command line tool, in ./certs:
# a CA (ca.crt), and a certificate and key for the Sender (sender.crt, sender.key) and for the Receiver (receiver.crt,
# receiver.key). Each is usable as a TLS server and as a TLS client (the examples use mutual TLS) and names the loopback
# address 127.0.0.1 and a host name, so the connections to 127.0.0.1 verify.
# Nothing here is for production use. The keys are made here and are never part of the repository (certs/ is ignored).
# The keys are not password protected: the HTTP stack of ODC does not take an encrypted key (odc.TLSSocket could).
#
#   ./make-certs.sh [outdir] [days]        (KEYBITS in the environment, 2048 or more)
set -e
OUT="${1:-$(dirname "$0")/certs}"
DAYS="${2:-825}"
KEYBITS="${KEYBITS:-2048}"
if [ "$KEYBITS" -lt 2048 ] 2>/dev/null; then
	echo "KEYBITS must be at least 2048" >&2
	exit 1
fi
mkdir -p "$OUT"

# the CA
openssl genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KEYBITS" -out "$OUT/ca.key"
openssl req -x509 -new -key "$OUT/ca.key" -sha256 -days "$DAYS" -subj '/C=AU/O=Example Utility/OU=Certificate Authority/CN=Example Lua-Transport CA' \
	-addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' -out "$OUT/ca.crt"

# a node certificate, usable as a TLS server and as a TLS client
new_node()
{
	file="$1"; name="$2"
	printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth,clientAuth\nsubjectAltName=DNS:%s,IP:127.0.0.1\n' "$name" > "$OUT/$file.ext"
	openssl genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KEYBITS" -out "$OUT/$file.key"
	openssl req -new -key "$OUT/$file.key" -subj "/C=AU/O=Example Utility/OU=Lua-Transport/CN=$name" -out "$OUT/$file.csr"
	openssl x509 -req -in "$OUT/$file.csr" -CA "$OUT/ca.crt" -CAkey "$OUT/ca.key" -CAcreateserial -days "$DAYS" -sha256 \
		-extfile "$OUT/$file.ext" -out "$OUT/$file.crt"
	rm -f "$OUT/$file.ext" "$OUT/$file.csr"
}
new_node sender odc-sender.example
new_node receiver odc-receiver.example

chmod 600 "$OUT"/*.key
echo "Certificates written to $OUT (ca.crt, sender.crt/.key, receiver.crt/.key)"
