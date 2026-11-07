#!/bin/sh
# Makes THROW-AWAY certificates for the ICCP-MACE example with the openssl command line tool:
# a CA, a server certificate and a client certificate (RSA, 2048 bits by default, SHA-256), in ./certs, and a second CA with a
# client certificate (iccp-client-other) for the example's refusal.
# MACE uses RSA keys only (the signature is a PKCS#1 v1.5 RSA signature) of 2048 to 8192 bits by default.
# Nothing here is for production use. The keys are generated here and are never part of the repository (do not commit the certs directory).
#
#   ./make-certs.sh [outdir] [days]
# Settings in the environment: COUNTRY (AU), STATE (New South Wales), ORG (Example Utility), KEYBITS (2048).
set -e
OUT="${1:-$(dirname "$0")/certs}"
DAYS="${2:-825}"
COUNTRY="${COUNTRY:-AU}"
STATE="${STATE:-New South Wales}"
ORG="${ORG:-Example Utility}"
KEYBITS="${KEYBITS:-2048}"
if [ "$KEYBITS" -lt 2048 ] 2>/dev/null; then
	echo "KEYBITS must be at least 2048" >&2
	exit 1
fi
mkdir -p "$OUT"

# the distinguished name, as an openssl -subj string (the values must not contain / or =)
subject()
{
	printf '/C=%s/ST=%s/O=%s/OU=%s/CN=%s' "$COUNTRY" "$STATE" "$ORG" "$1" "$2"
}

new_ca()
{
	file="$1"; name="$2"
	openssl genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KEYBITS" -out "$OUT/$file.key"
	openssl req -x509 -new -key "$OUT/$file.key" -sha256 -days "$DAYS" -subj "$(subject 'Certificate Authority' "$name")" \
		-addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' -out "$OUT/$file.crt"
}

# a node certificate signed by the CA "$5"
new_node()
{
	file="$1"; ou="$2"; name="$3"; san="$4"; ca="$5"
	printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nsubjectAltName=%s\n' "$san" > "$OUT/$file.ext"
	openssl genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KEYBITS" -out "$OUT/$file.key"
	openssl req -new -key "$OUT/$file.key" -subj "$(subject "$ou" "$name")" -out "$OUT/$file.csr"
	openssl x509 -req -in "$OUT/$file.csr" -CA "$OUT/$ca.crt" -CAkey "$OUT/$ca.key" -CAcreateserial -days "$DAYS" -sha256 \
		-extfile "$OUT/$file.ext" -out "$OUT/$file.crt"
	rm -f "$OUT/$file.ext" "$OUT/$file.csr"
}

new_ca ca 'Example ICCP CA'
new_node iccp-server 'ICCP Servers' iccp-server.example 'DNS:iccp-server.example,IP:127.0.0.1' ca
new_node iccp-client 'ICCP Clients' iccp-client.example 'DNS:iccp-client.example' ca
new_ca other-ca 'Other Example CA'
new_node iccp-client-other 'ICCP Clients' iccp-client.example 'DNS:iccp-client.example' other-ca

chmod 600 "$OUT"/*.key
echo "Certificates written to $OUT (ca.crt, iccp-server.crt/.key, iccp-client.crt/.key, and for the refusal other-ca.crt, iccp-client-other.crt/.key)"
