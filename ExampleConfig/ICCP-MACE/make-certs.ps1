# Makes THROW-AWAY certificates for the ICCP-MACE example with the openssl command line tool:
# a CA, a server certificate and a client certificate (RSA, 2048 bits by default, SHA-256), in .\certs, and a second CA with a
# client certificate (iccp-client-other) for the example's refusal.
# MACE uses RSA keys only (the signature is a PKCS#1 v1.5 RSA signature) of 2048 to 8192 bits by default.
# Nothing here is for production use. The keys are generated here and are never part of the repository (do not commit the certs directory).
#
#   powershell -File make-certs.ps1 [-OutDir certs] [-OpenSsl C:\path\to\openssl.exe] [-Days 825] [-KeyBits 2048]
#                                   [-Country AU] [-State 'New South Wales'] [-Org 'Example Utility']
param(
	[string]$OutDir = (Join-Path $PSScriptRoot 'certs'),
	[string]$OpenSsl = 'openssl',
	[int]$Days = 825,
	[ValidateRange(2048, 8192)][int]$KeyBits = 2048,
	[string]$Country = 'AU',
	[string]$State = 'New South Wales',
	[string]$Org = 'Example Utility'
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

function New-Ca($file, $name)
{
	Run genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KeyBits" -out "$OutDir/$file.key"
	Run req -x509 -new -key "$OutDir/$file.key" -sha256 -days $Days -subj (Subject 'Certificate Authority' $name) `
		-addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' -out "$OutDir/$file.crt"
}

# a node certificate signed by the CA $ca
function New-Node($file, $ou, $name, $san, $ca)
{
	$ext = Join-Path $OutDir "$file.ext"
	Set-Content -Path $ext -Value @('basicConstraints=critical,CA:FALSE', 'keyUsage=critical,digitalSignature', "subjectAltName=$san")
	Run genpkey -algorithm RSA -pkeyopt "rsa_keygen_bits:$KeyBits" -out "$OutDir/$file.key"
	Run req -new -key "$OutDir/$file.key" -subj (Subject $ou $name) -out "$OutDir/$file.csr"
	Run x509 -req -in "$OutDir/$file.csr" -CA "$OutDir/$ca.crt" -CAkey "$OutDir/$ca.key" -CAcreateserial -days $Days -sha256 -extfile $ext -out "$OutDir/$file.crt"
	Remove-Item $ext, "$OutDir/$file.csr"
}

New-Ca 'ca' 'Example ICCP CA'
New-Node 'iccp-server' 'ICCP Servers' 'iccp-server.example' 'DNS:iccp-server.example,IP:127.0.0.1' 'ca'
New-Node 'iccp-client' 'ICCP Clients' 'iccp-client.example' 'DNS:iccp-client.example' 'ca'
New-Ca 'other-ca' 'Other Example CA'
New-Node 'iccp-client-other' 'ICCP Clients' 'iccp-client.example' 'DNS:iccp-client.example' 'other-ca'

Write-Host "Certificates written to $OutDir (ca.crt, iccp-server.crt/.key, iccp-client.crt/.key, and for the refusal other-ca.crt, iccp-client-other.crt/.key)"
