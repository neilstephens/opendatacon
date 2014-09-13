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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <functional>
#include <iostream>
#include <fstream>
#include <string>
#include <cerrno>

#ifdef _WIN32
#include <io.h>
#include <ws2tcpip.h>
#include <stdarg.h>
#include <stdint.h>
typedef SSIZE_T ssize_t;
//#define S_ISREG(a) true
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)	// regular file
#include <sys/types.h> // off_t, ssize_t
#define MHD_PLATFORM_H
#else
inline void fopen_s(FILE** file, const char * name, const char * mode)
{
    *file = fopen(name, mode);
}
#endif

#include <microhttpd.h>

#include "WebUI.h"

struct connection_info_struct
{
    ParamCollection PostValues;
    struct MHD_PostProcessor *postprocessor = nullptr;
};

constexpr int POSTBUFFERSIZE = 512;
constexpr char ROOTPAGE[] = "/index.html";
constexpr char ROOTJSON[] = "/JSON/";
constexpr char EMPTY_PAGE[] = "<html><head><title>File not found</title></head><body>File not found</body></html>";
constexpr char NO_RESPONDER[] = "<html><head><title>Responder not found</title></head><body>Responder not found</body></html>";

const std::unordered_map<std::string, const std::string> MimeTypeMap {
    { "json", "application/json" },
    { "js", "text/javascript" },
    { "html", "text/html"},
    { "jpg", "image/jpeg"},
    { "css", "text/css"},
    { "txt", "text/plain"},
    { "default", "application/octet-stream"}
};

const std::string& GetMimeType(const std::string& rUrl)
{
    auto last = rUrl.find_last_of("/\\.");
    if (last == std::string::npos) return MimeTypeMap.at("default");
    const std::string pExt = rUrl.substr(last+1);
    
    if(MimeTypeMap.count(pExt) != 0)
    {
        return MimeTypeMap.at(pExt);
    }
    return MimeTypeMap.at("default");
}

const std::string GetPath(const std::string& rUrl)
{
    auto last = rUrl.find_last_of("/\\");
    if (last == std::string::npos) return "";
    return rUrl.substr(0,last);
}

const std::string GetFile(const std::string& rUrl)
{
    auto last = rUrl.find_last_of("/\\");
    if (last == std::string::npos) return rUrl;
    return rUrl.substr(last+1);
}

/* Test Certificate */
//openssl genrsa -out server.key 2048
const char default_cert_pem[] = "\
-----BEGIN CERTIFICATE-----\
MIIDTzCCAjegAwIBAgIJALMlRzO1GxWWMA0GCSqGSIb3DQEBBQUAMCMxITAfBgNV\
BAoTGEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0xNDA4MzEwNzEwMDdaFw0y\
NDA4MjgwNzEwMDdaMCMxITAfBgNVBAoTGEludGVybmV0IFdpZGdpdHMgUHR5IEx0\
ZDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALy6snqeSt9zLRbuxJMC\
nIIuvV9MLBWf6G4f1yr51tvNrL63Z+QtVn4n+EclSZMSzbYwWGDudWQEI3/aB6TW\
45gXiZiINwCuhRWCIMhfRjar0pCwuinA/m+oK4n/hMcR/CH2kocUIB1XWRZojRXz\
UPJvgeN41vmbzskRx/NiiSW+L0LeaXsO9lNVid+TQqLuoEC3UuDiF9wgxaB8bwxB\
tzHkzY+ZiH1JhPLCKy7vmMNdZ0IBd7ZJWS8R3v5PJKOtsiAeT6gscQajpBl3a05w\
A5F6A7tguLpwEbds19RI7AhTvceJdKzCBbJD6gQLVRkwOlxdCcNo+lIcLi/mWfro\
GeECAwEAAaOBhTCBgjAdBgNVHQ4EFgQUPMEnSKiWMmJSPuXLK+J+hRUBVhgwUwYD\
VR0jBEwwSoAUPMEnSKiWMmJSPuXLK+J+hRUBVhihJ6QlMCMxITAfBgNVBAoTGElu\
dGVybmV0IFdpZGdpdHMgUHR5IEx0ZIIJALMlRzO1GxWWMAwGA1UdEwQFMAMBAf8w\
DQYJKoZIhvcNAQEFBQADggEBAKMrwZx/VuUu2jrsCy5SSi4Z2U3QCo08TtmJXYY+\
ekZGAQud84bfJTcr3uR0ubLZDHhxU35N96k8YCkJERx4MUhnwuJHa7nEhJcrsM5z\
ZSKcZ5wpH6JDDt1DN4Hms+PMiLuDkPfZL7dV1r9GFrzN1/PYKrD1K5QQyt9I/MAD\
WBP6nipRqM2kEscggH911XntElBUnj9jFjFnpHjaNJAnz05PAORXrCrXA2EKz6RH\
y/Ep3/khCkj2C3DlRowRTzQwJ0eezMf7UsjeRkZZIvjis1381owJrRm3yjRYDUa6\
7e03d+42UKqZx1Ka1to5D6Al1ygiP3hl0bQj+/wpToK6uVw=\
-----END CERTIFICATE-----";

//openssl req -days 3650 -out server.pem -new -x509 -key server.key
const char default_key_pem[] = "\
-----BEGIN RSA PRIVATE KEY-----\
MIIEpAIBAAKCAQEAvLqyep5K33MtFu7EkwKcgi69X0wsFZ/obh/XKvnW282svrdn\
5C1Wfif4RyVJkxLNtjBYYO51ZAQjf9oHpNbjmBeJmIg3AK6FFYIgyF9GNqvSkLC6\
KcD+b6grif+ExxH8IfaShxQgHVdZFmiNFfNQ8m+B43jW+ZvOyRHH82KJJb4vQt5p\
ew72U1WJ35NCou6gQLdS4OIX3CDFoHxvDEG3MeTNj5mIfUmE8sIrLu+Yw11nQgF3\
tklZLxHe/k8ko62yIB5PqCxxBqOkGXdrTnADkXoDu2C4unARt2zX1EjsCFO9x4l0\
rMIFskPqBAtVGTA6XF0Jw2j6UhwuL+ZZ+ugZ4QIDAQABAoIBAQCxkIYzz5JqQZb+\
qI7SMfbGlOsfKi+f+N9aHSL4EDAShaQtm6lniTCDaV+ysGZUtbBN5ZaBPFm+TBaK\
R7xBXtyrUBnpJN97CLe10MS/QMRy0548+8lrV2UL8JFmOL3X/hfWbILYDBta/7+V\
0bBMIqzaLAds2ViJaApaKxyQ5PhcRMFxLPnm7SRdltScpjkGQNcC2ilA+ezknOq1\
rj0MR0niaMezwsz590/h9qUAkxBxJSZL86HOKiZ678haNwgHrgxQBJPIwTliEB9M\
xPTxLyM+feHu9oUpYgzrEV/+sBENZY3nsqj8iinIYFCZGcRUAnyjRKDNZn3/tYmN\
xP8KXLExAoGBAOX+P3+w8lZoMdvtPu4NYiCmNmJTVa2raqgq02drswZDKf1LOJoW\
GSXUr9xkpg5BqQyRys+yJwFXKTwvH2Py/KrBUuKj/UusKmM/ycnj/an5w7zmeubB\
bUahUTmLiDM5jzvv4gDqfoGswZoxhJ9XGWpVCOFdbpukMzHe3MiI0xKbAoGBANIR\
9XDKMuXbUOiJ+X+pSXuvfYSklgoKaFzr6ozsN+jXiPdw9WWF+j2yz8zb0v6fkwJi\
HlHIuvosnNf6L2UGF/5T0Eal7yIEsPK+MTgIw0Zr0IOpCUasAbALkcQMd+rBjdbV\
NeOpVwC/Zr1kC5hKI+V6VA7QmWLjb+Fy8E4jqf8zAoGBANbLSlZg1RKpoNb6jSkZ\
yqkfUe8mUQAu9R81T9ZomPuiQlbSp3wQY1AXgF5eiU8LN2wLxNOQWClCU7pnb/OS\
fTKj9lrAONExayzh5/zrNn5GSu3ieqmDwCCUjB0oGP1uJj0d3X5pgdhtlSoCUQ/W\
8l+CJxcCgUhOY5mRv7RxRF89AoGAdL7yTrqwyrm2H2X+uQoWAp0m/r6RfAcItQuP\
kL3+3HJcdlfaqY9p4Tws7EcG3edFRj/NZdpOv5ZnnEg4asaWMwvVZk31tkwxIta8\
d8226L4mZeVdeF9DmNj1K6VaR6dF8q0Pg/Sqm4nDyWF+aCZcCL6RVKJtfF214e+E\
yYhcg60CgYATTs3kW5cmGfdvkXHCSodIHonZLzHLOkn81S0W6FEM8zG1MSVLPA+J\
DE+cahwFk7x5dZ28WePVnm/QqIFdyq3g9MliQrlIGVbbn3DtvVVBT5cc2/NPDnHN\
9Ew4HhHIV+smoLTlGglfrlCuHXcrEzK5l5AMy9gD62OnhR3b3y0o4g==\
-----END RSA PRIVATE KEY-----";

static ssize_t
file_reader(void *cls, uint64_t pos, char *buf, size_t max)
{
	FILE *file = (FILE *)cls;
    
	(void)fseek(file, pos, SEEK_SET);
	return fread(buf, 1, max, file);
}

static void
file_free_callback(void *cls)
{
	FILE *file = (FILE *)cls;
	fclose(file);
}

/* response handler callback wrapper */
static int ahc(void *cls,
               struct MHD_Connection *connection,
               const char *url,
               const char *method,
               const char *version,
               const char *upload_data,
               size_t *upload_data_size,
               void **ptr)
{
	WebUI* test = (WebUI*)cls;
	return test->http_ahc(cls, connection, url, method, version, upload_data, upload_data_size, ptr);
}

static
std::string load_key(const char *filename)
{
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in)
    {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return(contents);
    }
    throw(errno);
}

WebUI::WebUI(uint16_t pPort) :
    d(nullptr),
    port(pPort)
{
    try
    {
        key_pem = load_key("server.key");
        cert_pem = load_key("server.pem");
        
    }
    catch (std::exception e)
    {
        std::cout << "The key/certificate files could not be read. Reverting to default certificate.\n" << std::endl;
        cert_pem = default_cert_pem;
        key_pem = default_key_pem;
    }
}

WebUI::~WebUI()
{

}

void WebUI::AddResponderCollection(const std::string name, const IUIResponderCollection& pResponderCollection)
{
    Responders[name] = &pResponderCollection;
}

int WebUI::ReturnFile(struct MHD_Connection *connection,
                      const char *url)
{
    struct stat buf;
    FILE *file;
    struct MHD_Response *response;
    int ret;
    
    if ((0 == stat(&url[1], &buf)) && (S_ISREG(buf.st_mode)))
        fopen_s(&file, &url[1], "rb");
    else
        file = nullptr;
    if (file == nullptr)
    {
        response = MHD_create_response_from_buffer(strlen(EMPTY_PAGE),
                                                   (void *)EMPTY_PAGE,
                                                   MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
    }
    else
    {
        response = MHD_create_response_from_callback(buf.st_size, 32 * 1024,     /* 32k PAGE_NOT_FOUND size */
                                                     &file_reader, file,
                                                     &file_free_callback);
        if (response == nullptr)
        {
            fclose(file);
            return MHD_NO;
        }
        MHD_add_response_header(response, "Content-Type", GetMimeType(url).c_str());
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
    }
    return ret;
}

int WebUI::ReturnJSON(struct MHD_Connection *connection, const std::string& url, const IUIResponderCollection& rResponderCollection, const ParamCollection& params)
{
    struct MHD_Response *response;
    int ret;
    
    const std::string responderName = GetFile(url);
    
    if (responderName.length() == 0)
    {
        Json::FastWriter jsonout;
        Json::Value event;
        
        event = rResponderCollection.GetResponse(params);
        
        std::string jsonstring = jsonout.write(event);
        const char* jsoncstr = jsonstring.c_str();
        
        /*
         MHD_RESPMEM_MUST_FREE
         MHD_RESPMEM_MUST_COPY
         */
        response = MHD_create_response_from_buffer(strlen(jsoncstr),
                                                   (void *)jsoncstr,
                                                   MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header (response, "Content-Type", MimeTypeMap.at("json").c_str());
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        
        return ret;
    }
    else
    if (auto responder = rResponderCollection.GetUIResponder(responderName))
    {
        Json::FastWriter jsonout;
        Json::Value event;

        event = responder->GetResponse(params);
        
        std::string jsonstring = jsonout.write(event);
        const char* jsoncstr = jsonstring.c_str();
        
        /*
         MHD_RESPMEM_MUST_FREE
         MHD_RESPMEM_MUST_COPY
         */
        response = MHD_create_response_from_buffer(strlen(jsoncstr),
                                                   (void *)jsoncstr,
                                                   MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header (response, "Content-Type", MimeTypeMap.at("json").c_str());
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        
        return ret;
    }

    response = MHD_create_response_from_buffer(strlen(NO_RESPONDER),
                                               (void *)NO_RESPONDER,
                                               MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    return ret;
}

static int
iterate_post (void *coninfo_cls,
              enum MHD_ValueKind kind, //MHD_POSTDATA_KIND
              const char *key, // POST KEY
              const char *filename,
              const char *content_type,
              const char *transfer_encoding,
              const char *data, // POST VALUE
              uint64_t off,
              size_t size // POST VALUE LENGTH
)
{
    struct connection_info_struct* con_info = (connection_info_struct*) coninfo_cls;
    
    if (kind == MHD_POSTDATA_KIND)
    {
        con_info->PostValues[key] = data;
        return MHD_YES;
    }
    return MHD_NO;
}

static
void request_completed(void *cls, struct MHD_Connection *connection,
                        void **con_cls,
                        enum MHD_RequestTerminationCode toe)
{
    struct connection_info_struct *con_info = (connection_info_struct*)*con_cls;
    
    if (nullptr == con_info) return;
    if (nullptr != con_info->postprocessor) MHD_destroy_post_processor(con_info->postprocessor);
    con_info->postprocessor = nullptr;
    free(con_info);
    *con_cls = NULL;   
}

int WebUI::CreateNewRequest(void *cls,
                      struct MHD_Connection *connection,
                      const char *url,
                      const char *method,
                      const char *version,
                      const char *upload_data,
                      size_t *upload_data_size,
                      void **con_cls)
{
    struct connection_info_struct *con_info;

    con_info = new connection_info_struct;
    if (nullptr == con_info) return MHD_NO;
    *con_cls = (void*)con_info;
    
    if (0 == strcmp(method, MHD_HTTP_METHOD_GET)) return MHD_YES;
    if (0 == strcmp (method, "POST"))
    {
        if (*upload_data_size == 0) return MHD_YES;
        con_info->postprocessor = MHD_create_post_processor(connection, POSTBUFFERSIZE, iterate_post, (void*) con_info);
        
        if (nullptr != con_info->postprocessor) return MHD_YES;
    }

    // unexpected method or couldn't create post processor
    free(con_info);
    return MHD_NO;
}

/* HTTP access handler call back */
int	WebUI::http_ahc(void *cls,
                    struct MHD_Connection *connection,
                    const char *url,
                    const char *method,
                    const char *version,
                    const char *upload_data,
                    size_t *upload_data_size,
                    void **con_cls)
{
    struct connection_info_struct *con_info;

    if(nullptr == *con_cls)
    {
        return CreateNewRequest(cls,
                                connection,
                                url,
                                method,
                                version,
                                upload_data,
                                upload_data_size,
                                con_cls);
    }
    
    ParamCollection params;

    if (0 == strcmp(method, "POST"))
    {
        con_info = (connection_info_struct*)*con_cls;
        
        if (*upload_data_size != 0)
        {
            MHD_post_process(con_info->postprocessor, upload_data, *upload_data_size);
            *upload_data_size = 0;
            
            return MHD_YES;
        }
        if (con_info)
        {
            params = con_info->PostValues;
        }
    }
    
    const std::string responderCollectionName = GetPath(url);
    if (Responders.count(responderCollectionName))
    {
        return ReturnJSON(connection, &url[strlen(ROOTJSON)], *(Responders[responderCollectionName]), params);
    }
    else
    {
        if (strlen(url) == 1)
        {
            return ReturnFile(connection, ROOTPAGE);
        }
        else
        {
            return ReturnFile(connection, url);
        }
    }
}

int WebUI::start()
{
    
    d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG | MHD_USE_SSL,
                         port, // Port to bind to
                         nullptr, // callback to call to check which clients allowed to connect
                         nullptr, // extra argument to apc
                         &ahc, // handler called for all requests
                         this, // extra argument to dh
                         MHD_OPTION_NOTIFY_COMPLETED, &request_completed, this, // completed handler and extra argument
                         MHD_OPTION_CONNECTION_TIMEOUT, 256,
                         MHD_OPTION_HTTPS_MEM_KEY, key_pem.c_str(),
                         MHD_OPTION_HTTPS_MEM_CERT, cert_pem.c_str(),
                         MHD_OPTION_END);
    
    if (d == nullptr)
        return 1;
    return 0;
}

void WebUI::stop()
{
    if (d == nullptr) return;
    MHD_stop_daemon(d);
    d = nullptr;
}


