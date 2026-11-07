# Makes THROW-AWAY certificates for the ICCP-TLS example with the openssl command line tool:
# a CA, a server certificate and a client certificate (RSA, 2048 bits by default, SHA-256), in ./certs.
# Nothing here is for production use.
# The keys are generated here and are never part of the repository (do not commit the certs directory).
# RSA is used because some ICCP stacks do not accept other key algorithms, and the key must be at least 2048 bits
# (-KeyBits cannot go below that).
# The subjects carry a full, human readable distinguished name (country, state, organisation, organisational unit
# and common name), for example:  C=AU, ST=New South Wales, O=Example Utility, OU=ICCP Servers, CN=iccp-server.example
# The common names are what the example configurations use (ServerName, PeerCertAllowList "CN:...").
# Every node also gets PEM bundles (<node>-chain.pem, <node>-cert-key.pem) and a PKCS#12 package (<node>.p12) holding
# the certificate, its private key and the CA, for importing into the Windows certificate store.
#
#   powershell -File make-certs.ps1 [-OutDir certs] [-OpenSsl C:\path\to\openssl.exe] [-Days 825] [-KeyBits 2048]
#                                   [-Country AU] [-State 'New South Wales'] [-Org 'Example Utility']
#                                   [-P12Password example] [-P12Modern]
# By default the .p12 files use the older 3DES / SHA-1 protection that every Windows version can import. -P12Modern
# uses OpenSSL's current default (AES-256 / SHA-256), which needs Windows 11 or Windows Server 2022 or later.
param(
	[string]$OutDir = (Join-Path $PSScriptRoot 'certs'),
	[string]$OpenSsl = 'openssl',
	[int]$Days = 825,
	[ValidateRange(2048, 16384)][int]$KeyBits = 2048,
	[string]$Country = 'AU',
	[string]$State = 'New South Wales',
	[string]$Org = 'Example Utility',
	[string]$P12Password = 'example',
	[switch]$P12Modern
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $OutDir | Out-Null

function Run
{
	& $OpenSsl @args
	if($LASTEXITCODE -ne 0) { throw "openssl failed: $args" }
}

# the distinguished name, as an openssl -subj string (the values must not contain / or =)
function Subject($ou, $cn)
{
	"/C=$Country/ST=$State/O=$Org/OU=$ou/CN=$cn"
}

# the CA
Run genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KeyBits" -out "$OutDir/ca.key"
Run req -x509 -new -key "$OutDir/ca.key" -sha256 -days $Days -subj (Subject 'Certificate Authority' 'Example ICCP CA') `
	-addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' -out "$OutDir/ca.crt"

# a node certificate: usable as a TLS server and as a TLS client (IEC 62351-3 wants mutual authentication)
function New-Node($file, $ou, $name, $san)
{
	$ext = Join-Path $OutDir "$file.ext"
	# keyEncipherment as well as digitalSignature: an RSA certificate may be used for RSA key exchange by older stacks
	Set-Content -Path $ext -Value @('basicConstraints=critical,CA:FALSE', 'keyUsage=critical,digitalSignature,keyEncipherment', 'extendedKeyUsage=serverAuth,clientAuth', "subjectAltName=$san")
	Run genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KeyBits" -out "$OutDir/$file.key"
	Run req -new -key "$OutDir/$file.key" -subj (Subject $ou $name) -out "$OutDir/$file.csr"
	Run x509 -req -in "$OutDir/$file.csr" -CA "$OutDir/ca.crt" -CAkey "$OutDir/ca.key" -CAcreateserial -days $Days -sha256 -extfile $ext -out "$OutDir/$file.crt"
	Remove-Item $ext, "$OutDir/$file.csr"

	# PEM bundles (all the files here are PEM): the node certificate followed by the CA (the chain), and
	# the node certificate and its private key in one file (keep that one private, like the .key)
	Set-Content -Path "$OutDir/$file-chain.pem" -Value ((Get-Content "$OutDir/$file.crt") + (Get-Content "$OutDir/ca.crt")) -Encoding ascii
	Set-Content -Path "$OutDir/$file-cert-key.pem" -Value ((Get-Content "$OutDir/$file.crt") + (Get-Content "$OutDir/$file.key")) -Encoding ascii

	# the PKCS#12 package for Windows: certificate + private key + CA, protected with $P12Password (throw-away)
	$p12Args = @('pkcs12', '-export', '-inkey', "$OutDir/$file.key", '-in', "$OutDir/$file.crt", '-certfile', "$OutDir/ca.crt",
		'-name', $name, '-passout', "pass:$P12Password", '-out', "$OutDir/$file.p12")
	if(-not $P12Modern) { $p12Args += @('-certpbe', 'PBE-SHA1-3DES', '-keypbe', 'PBE-SHA1-3DES', '-macalg', 'sha1') }
	Run @p12Args
}
New-Node 'iccp-server' 'ICCP Servers' 'iccp-server.example' 'DNS:iccp-server.example,IP:127.0.0.1'
New-Node 'iccp-client' 'ICCP Clients' 'iccp-client.example' 'DNS:iccp-client.example'

Write-Host "Certificates written to $OutDir (PEM: ca.crt, iccp-server.crt/.key, iccp-client.crt/.key,"
Write-Host "  the bundles iccp-server-chain.pem, iccp-server-cert-key.pem, iccp-client-chain.pem, iccp-client-cert-key.pem,"
Write-Host "  and the PKCS#12 packages iccp-server.p12, iccp-client.p12 with the password '$P12Password')"
