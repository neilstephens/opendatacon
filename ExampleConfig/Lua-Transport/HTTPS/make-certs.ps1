# Makes THROW-AWAY certificates for this example with the openssl command line tool, in ./certs:
# a CA (ca.crt), and a certificate and key for the Sender (sender.crt, sender.key) and for the Receiver (receiver.crt,
# receiver.key). Each is usable as a TLS server and as a TLS client (the examples use mutual TLS) and names the loopback
# address 127.0.0.1 and a host name, so the connections to 127.0.0.1 verify.
# Nothing here is for production use. The keys are made here and are never part of the repository (certs/ is ignored).
# The keys are not password protected: the HTTP stack of ODC does not take an encrypted key (odc.TLSSocket could).
#
#   powershell -File make-certs.ps1 [-OutDir certs] [-OpenSsl C:\path\to\openssl.exe] [-Days 825] [-KeyBits 2048]
param(
	[string]$OutDir = (Join-Path $PSScriptRoot 'certs'),
	[string]$OpenSsl = 'openssl',
	[int]$Days = 825,
	[ValidateRange(2048, 16384)][int]$KeyBits = 2048
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $OutDir | Out-Null

function Run
{
	& $OpenSsl @args
	if($LASTEXITCODE -ne 0) { throw "openssl failed: $args" }
}

# the CA
Run genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KeyBits" -out "$OutDir/ca.key"
Run req -x509 -new -key "$OutDir/ca.key" -sha256 -days $Days -subj '/C=AU/O=Example Utility/OU=Certificate Authority/CN=Example Lua-Transport CA' `
	-addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' -out "$OutDir/ca.crt"

# a node certificate, usable as a TLS server and as a TLS client
function New-Node($file, $name)
{
	$ext = Join-Path $OutDir "$file.ext"
	Set-Content -Path $ext -Value @('basicConstraints=critical,CA:FALSE', 'keyUsage=critical,digitalSignature,keyEncipherment', 'extendedKeyUsage=serverAuth,clientAuth', "subjectAltName=DNS:$name,IP:127.0.0.1")
	Run genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KeyBits" -out "$OutDir/$file.key"
	Run req -new -key "$OutDir/$file.key" -subj "/C=AU/O=Example Utility/OU=Lua-Transport/CN=$name" -out "$OutDir/$file.csr"
	Run x509 -req -in "$OutDir/$file.csr" -CA "$OutDir/ca.crt" -CAkey "$OutDir/ca.key" -CAcreateserial -days $Days -sha256 -extfile $ext -out "$OutDir/$file.crt"
	Remove-Item $ext, "$OutDir/$file.csr"
}
New-Node 'sender' 'odc-sender.example'
New-Node 'receiver' 'odc-receiver.example'

Write-Host "Certificates written to $OutDir (ca.crt, sender.crt/.key, receiver.crt/.key)"
