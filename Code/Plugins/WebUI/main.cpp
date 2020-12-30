/*	opendatacon
 *
 *	Copyright (c) 2014:
 *
 *		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 *		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */
//
//  main.cpp
//  opendatacon
//
//  Created by Alan Murray on 30/11/2014.
//
//

#include "WebUI.h"
#include <fstream>

/* Test Certificate */
//openssl genrsa -out server.key 2048
const char default_cert_pem[] = R"001(
-----BEGIN CERTIFICATE-----
MIIFoTCCA4mgAwIBAgIUYNrdJQ/9gQy6DBV8O20G8Sxo0pcwDQYJKoZIhvcNAQEN
BQAwXzELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM
GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDEYMBYGA1UEAwwPb3BlbmRhdGFjb24u
bmV0MCAXDTIwMDkxNzIxMjkyM1oYDzIxMjAwODI0MjEyOTIzWjBfMQswCQYDVQQG
EwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lk
Z2l0cyBQdHkgTHRkMRgwFgYDVQQDDA9vcGVuZGF0YWNvbi5uZXQwggIiMA0GCSqG
SIb3DQEBAQUAA4ICDwAwggIKAoICAQDOFlx0YR0kDhw5Ca1YplOTcmeRrUe3ELwt
3igMEP+0FfwgfxPXrCiJkN87mps8xR0qaUdI/99YFeu9vN9OQOkkSYbix7vqTFgJ
BmZD0ftYxR37npsojKqsFIu5vadiLgYR+r2Q3MeqIZJQ9kiMKi6chhqgX8LCEQnK
7rSDbBy1ed4RoHeu6EyjRrFB2ICPgZtBQtPkM+cI0SQ807Xu2Y2+VC4DCkWqc7Ku
G3QlsN5m0Sd0iN8ImFnxNKimw/gbFeWEp9cEuRsnGsBmCuN7bDzrxnofYYfKwNH+
YW0s8laAJg1Rie6sOh7NSZR3P0gabJm30rYC6jjuAvmrIFEGLVk0luQ7sj5Nr17q
eAUISvTtoC/tQqyXljSzbhW4EmAO5lTNsA7N10sCxDrrXiXPTN5MsyQLubs93/M0
So8f70/UMUPwMcXoEKwum1NEMnZ2vd19KZgUJ3DPsdN9RHORJxuzAqAZBdc5B9ua
R9g8xbl4aQkS7Zii5DaY3mYRJY/apKoBjLzOBjEcmoe4sm78+vUdLN7sH11DUYAL
idNTYD2tre6d+nAIaHf860kFEIIzxO4pSMedR+2MEhZ0kGJD8ZyJ/Jv9cazc4D7p
qn/VHAxUmDOHTn+ACbhBDfak1UakJbNt+yZtgSwaAnZwhY3BktKujTXGHNy1+0Iq
ZBFQ4sVJGQIDAQABo1MwUTAdBgNVHQ4EFgQUROGL8zWErJBNXCSMyojX0UkgXwsw
HwYDVR0jBBgwFoAUROGL8zWErJBNXCSMyojX0UkgXwswDwYDVR0TAQH/BAUwAwEB
/zANBgkqhkiG9w0BAQ0FAAOCAgEAteukOHqfnAJoPknpSIQLiL57kfvcKvyYoRXT
9aTi7tLOB6j0iZfu1MMlK2hmsRfCguwMKw7wUq3cYJD5esjKQvO6Us1c2zFxQdJM
cAyn2Ug3pUi2YPfLZsmdCKsxP4Im3m9T1SqSXrc6ibSIqtAWvnEvWF9fHErHvBln
lXumvsnAarJsgSCYKMDgoes2+az5iBi+7DQYGR0NZ24tUDMpGNbeptziirINBrZ2
KQ5FF6l0O90n1mykT4u2Z/1Xbv5v4gfmXt4HKrWaiL6NY9zM/GQEl36CIBNgVlz+
O68joZt+DPko9PLQ4aT3EOZD7XyA4OMcozuWhOe+pgnRghUus5yek8gyR0V0Jao7
nL8We1JM6J0W+hIBIawLJ5EeiDI6+7dct6V/UqecO+bjUP7Y8RPS0Y4h72Fb6mad
0EXvF+Ybis1xv7n8BvUA6+jUqw2Q9mWFVR6QWGSNy9MlyXw9/FlqOLByrQ7IEuqU
w25Wnzi3+OKZmiS5/mjWvZDoQH18NRO5FdgfHykPOlYMow1D/ojlZhrG+oYZKLgf
yCYfLUhaJ3Giqz3pvgZV3YatzER9lUihoSti5QtOqtoTJfzc3fJCiiBTeZjyvJ8r
bUrUAexSznSkuGKNRhhI9KAdKcF1ZurwV8Y9RqCvQzWY5bpxXKllkcAW84Pmws+M
MnvNKGA=
-----END CERTIFICATE-----
)001";

//openssl req -days 3650 -out server.pem -new -x509 -key server.key
const char default_key_pem[] = R"001(
-----BEGIN PRIVATE KEY-----
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
whkivOuTkh7GPlH1NEKqIKEA97Tw
-----END PRIVATE KEY-----
)001";

inline bool file_exists (const std::string& name)
{
	std::ifstream f(name.c_str());
	return f.good();
}

extern "C" WebUI* new_WebUIPlugin(const std::string& Name, const std::string& File, const Json::Value& Overrides)
{
	std::string ip = "0.0.0.0";
	uint16_t port = 443;
	std::string web_root = "www";
	std::string web_crt = "server.crt";
	std::string web_key = "server.key";
	std::string tcp_port = "10593";
	size_t log_q_size = 200;
	if(Overrides.isObject())
	{
		if(Overrides.isMember("IP"))
			ip= Overrides["IP"].asString();

		if(Overrides.isMember("Port"))
			port = Overrides["Port"].asUInt();

		if (Overrides.isMember("WebRoot"))
			web_root = Overrides["WebRoot"].asString();
		if (Overrides.isMember("WebCert"))
			web_crt = Overrides["WebCert"].asString();
		if (Overrides.isMember("WebKey"))
			web_key = Overrides["WebKey"].asString();
		if (Overrides.isMember("LogPort"))
			tcp_port = Overrides["LogPort"].asString();
		if (Overrides.isMember("MaxLogMessages"))
			log_q_size = Overrides["MaxLogMessages"].asUInt();
	}

	if(!file_exists(web_crt))
	{
		std::ofstream fout(web_crt);
		fout<<default_cert_pem;
		fout.flush();
	}
	if(!file_exists(web_key))
	{
		std::ofstream fout(web_key);
		fout<<default_key_pem;
		fout.flush();
	}

	return new WebUI(port, web_root, web_crt, web_key, tcp_port, log_q_size);
}

extern "C" void delete_WebUIPlugin(WebUI* aIUI_ptr)
{
	delete aIUI_ptr;
	return;
}
