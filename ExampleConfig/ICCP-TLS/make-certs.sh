#!/bin/sh
# Makes THROW-AWAY certificates for the ICCP-TLS example with the openssl command line tool:
# a CA, a server certificate and a client certificate (RSA, 2048 bits by default, SHA-256), in ./certs.
# Nothing here is for production use.
# The keys are generated here and are never part of the repository (do not commit the certs directory).
# RSA is used because some ICCP stacks do not accept other key algorithms, and the key must be at least 2048 bits
# (KEYBITS cannot go below that).
# The subjects carry a full, human readable distinguished name (country, state, organisation, organisational unit
# and common name), for example:  C=AU, ST=New South Wales, O=Example Utility, OU=ICCP Servers, CN=iccp-server.example
# The common names are what the example configurations use (ServerName, PeerCertAllowList "CN:...").
# Every node also gets PEM bundles (<node>-chain.pem, <node>-cert-key.pem) and a PKCS#12 package (<node>.p12) holding
# the certificate, its private key and the CA, for importing into the Windows certificate store.
#
#   ./make-certs.sh [outdir] [days]
# Settings in the environment: COUNTRY (AU), STATE (New South Wales), ORG (Example Utility), KEYBITS (2048),
# P12PASS (example, the password of the .p12 files) and P12MODERN=1.
# By default the .p12 files use the older 3DES / SHA-1 protection that every Windows version can import. P12MODERN=1
# uses OpenSSL's current default (AES-256 / SHA-256), which needs Windows 11 or Windows Server 2022 or later.
set -e
OUT="${1:-$(dirname "$0")/certs}"
DAYS="${2:-825}"
COUNTRY="${COUNTRY:-AU}"
STATE="${STATE:-New South Wales}"
ORG="${ORG:-Example Utility}"
KEYBITS="${KEYBITS:-2048}"
P12PASS="${P12PASS:-example}"
P12MODERN="${P12MODERN:-0}"
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

# the CA
openssl genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KEYBITS" -out "$OUT/ca.key"
openssl req -x509 -new -key "$OUT/ca.key" -sha256 -days "$DAYS" -subj "$(subject 'Certificate Authority' 'Example ICCP CA')" \
	-addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' -out "$OUT/ca.crt"

# a node certificate: usable as a TLS server and as a TLS client (IEC 62351-3 wants mutual authentication)
new_node()
{
	file="$1"; ou="$2"; name="$3"; san="$4"
	# keyEncipherment as well as digitalSignature: an RSA certificate may be used for RSA key exchange by older stacks
	printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth,clientAuth\nsubjectAltName=%s\n' "$san" > "$OUT/$file.ext"
	openssl genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KEYBITS" -out "$OUT/$file.key"
	openssl req -new -key "$OUT/$file.key" -subj "$(subject "$ou" "$name")" -out "$OUT/$file.csr"
	openssl x509 -req -in "$OUT/$file.csr" -CA "$OUT/ca.crt" -CAkey "$OUT/ca.key" -CAcreateserial -days "$DAYS" -sha256 \
		-extfile "$OUT/$file.ext" -out "$OUT/$file.crt"
	rm -f "$OUT/$file.ext" "$OUT/$file.csr"

	# PEM bundles (all the files here are PEM): the node certificate followed by the CA (the chain), and
	# the node certificate and its private key in one file (keep that one private, like the .key)
	cat "$OUT/$file.crt" "$OUT/ca.crt" > "$OUT/$file-chain.pem"
	cat "$OUT/$file.crt" "$OUT/$file.key" > "$OUT/$file-cert-key.pem"

	# the PKCS#12 package for Windows: certificate + private key + CA, protected with P12PASS (throw-away)
	if [ "$P12MODERN" = "1" ]; then
		openssl pkcs12 -export -inkey "$OUT/$file.key" -in "$OUT/$file.crt" -certfile "$OUT/ca.crt" -name "$name" \
			-passout "pass:$P12PASS" -out "$OUT/$file.p12"
	else
		openssl pkcs12 -export -inkey "$OUT/$file.key" -in "$OUT/$file.crt" -certfile "$OUT/ca.crt" -name "$name" \
			-certpbe PBE-SHA1-3DES -keypbe PBE-SHA1-3DES -macalg sha1 -passout "pass:$P12PASS" -out "$OUT/$file.p12"
	fi
}
new_node iccp-server 'ICCP Servers' iccp-server.example 'DNS:iccp-server.example,IP:127.0.0.1'
new_node iccp-client 'ICCP Clients' iccp-client.example 'DNS:iccp-client.example'

chmod 600 "$OUT"/*.key "$OUT"/*-cert-key.pem "$OUT"/*.p12
echo "Certificates written to $OUT (PEM: ca.crt, iccp-server.crt/.key, iccp-client.crt/.key,"
echo "  the bundles iccp-server-chain.pem, iccp-server-cert-key.pem, iccp-client-chain.pem, iccp-client-cert-key.pem,"
echo "  and the PKCS#12 packages iccp-server.p12, iccp-client.p12 with the password '$P12PASS')"
