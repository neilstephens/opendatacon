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

#ifdef _WIN32
#include <io.h>
#include <ws2tcpip.h>
#include <stdarg.h>
#include <stdint.h>
typedef SSIZE_T ssize_t;
//#define S_ISREG(a) true
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)	/* regular file */
#include <sys/types.h> //off_t, ssize_t
#define MHD_PLATFORM_H
#else
//#define fopen_s(a, args...) a = fopen(args...);
void fopen_s(FILE** file, const char * name, const char * mode)
{
    *file = fopen(name, mode);
}
#endif

#include <microhttpd.h>
#include <json/json.h>

#include "WebUI.h"

#define BUF_SIZE 1024
#define MAX_URL_LEN 255
#define POSTBUFFERSIZE  512
#define MAXNAMESIZE     20
#define MAXANSWERSIZE   512

// TODO remove if unused
#define CAFILE "ca.pem"
#define CRLFILE "crl.pem"

#define ROOTPAGE "/index.html"
#define ROOTJSON "/JSON/"
#define PAGE "<html><head><title>libmicrohttpd demo</title></head><body>libmicrohttpd demo</body></html>"
#define EMPTY_PAGE "<html><head><title>File not found</title></head><body>File not found</body></html>"

struct connection_info_struct
{
    int connectiontype;
    char *answerstring;
    struct MHD_PostProcessor *postprocessor;
};

/* Test Certificate */
const char cert_pem[] =
"-----BEGIN CERTIFICATE-----\n"
"MIICpjCCAZCgAwIBAgIESEPtjjALBgkqhkiG9w0BAQUwADAeFw0wODA2MDIxMjU0\n"
"MzhaFw0wOTA2MDIxMjU0NDZaMAAwggEfMAsGCSqGSIb3DQEBAQOCAQ4AMIIBCQKC\n"
"AQC03TyUvK5HmUAirRp067taIEO4bibh5nqolUoUdo/LeblMQV+qnrv/RNAMTx5X\n"
"fNLZ45/kbM9geF8qY0vsPyQvP4jumzK0LOJYuIwmHaUm9vbXnYieILiwCuTgjaud\n"
"3VkZDoQ9fteIo+6we9UTpVqZpxpbLulBMh/VsvX0cPJ1VFC7rT59o9hAUlFf9jX/\n"
"GmKdYI79MtgVx0OPBjmmSD6kicBBfmfgkO7bIGwlRtsIyMznxbHu6VuoX/eVxrTv\n"
"rmCwgEXLWRZ6ru8MQl5YfqeGXXRVwMeXU961KefbuvmEPccgCxm8FZ1C1cnDHFXh\n"
"siSgAzMBjC/b6KVhNQ4KnUdZAgMBAAGjLzAtMAwGA1UdEwEB/wQCMAAwHQYDVR0O\n"
"BBYEFJcUvpjvE5fF/yzUshkWDpdYiQh/MAsGCSqGSIb3DQEBBQOCAQEARP7eKSB2\n"
"RNd6XjEjK0SrxtoTnxS3nw9sfcS7/qD1+XHdObtDFqGNSjGYFB3Gpx8fpQhCXdoN\n"
"8QUs3/5ZVa5yjZMQewWBgz8kNbnbH40F2y81MHITxxCe1Y+qqHWwVaYLsiOTqj2/\n"
"0S3QjEJ9tvklmg7JX09HC4m5QRYfWBeQLD1u8ZjA1Sf1xJriomFVyRLI2VPO2bNe\n"
"JDMXWuP+8kMC7gEvUnJ7A92Y2yrhu3QI3bjPk8uSpHea19Q77tul1UVBJ5g+zpH3\n"
"OsF5p0MyaVf09GTzcLds5nE/osTdXGUyHJapWReVmPm3Zn6gqYlnzD99z+DPIgIV\n"
"RhZvQx74NQnS6g==\n" "-----END CERTIFICATE-----\n";

const char key_pem[] =
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEowIBAAKCAQEAtN08lLyuR5lAIq0adOu7WiBDuG4m4eZ6qJVKFHaPy3m5TEFf\n"
"qp67/0TQDE8eV3zS2eOf5GzPYHhfKmNL7D8kLz+I7psytCziWLiMJh2lJvb2152I\n"
"niC4sArk4I2rnd1ZGQ6EPX7XiKPusHvVE6VamacaWy7pQTIf1bL19HDydVRQu60+\n"
"faPYQFJRX/Y1/xpinWCO/TLYFcdDjwY5pkg+pInAQX5n4JDu2yBsJUbbCMjM58Wx\n"
"7ulbqF/3lca0765gsIBFy1kWeq7vDEJeWH6nhl10VcDHl1PetSnn27r5hD3HIAsZ\n"
"vBWdQtXJwxxV4bIkoAMzAYwv2+ilYTUOCp1HWQIDAQABAoIBAArOQv3R7gmqDspj\n"
"lDaTFOz0C4e70QfjGMX0sWnakYnDGn6DU19iv3GnX1S072ejtgc9kcJ4e8VUO79R\n"
"EmqpdRR7k8dJr3RTUCyjzf/C+qiCzcmhCFYGN3KRHA6MeEnkvRuBogX4i5EG1k5l\n"
"/5t+YBTZBnqXKWlzQLKoUAiMLPg0eRWh+6q7H4N7kdWWBmTpako7TEqpIwuEnPGx\n"
"u3EPuTR+LN6lF55WBePbCHccUHUQaXuav18NuDkcJmCiMArK9SKb+h0RqLD6oMI/\n"
"dKD6n8cZXeMBkK+C8U/K0sN2hFHACsu30b9XfdnljgP9v+BP8GhnB0nCB6tNBCPo\n"
"32srOwECgYEAxWh3iBT4lWqL6bZavVbnhmvtif4nHv2t2/hOs/CAq8iLAw0oWGZc\n"
"+JEZTUDMvFRlulr0kcaWra+4fN3OmJnjeuFXZq52lfMgXBIKBmoSaZpIh2aDY1Rd\n"
"RbEse7nQl9hTEPmYspiXLGtnAXW7HuWqVfFFP3ya8rUS3t4d07Hig8ECgYEA6ou6\n"
"OHiBRTbtDqLIv8NghARc/AqwNWgEc9PelCPe5bdCOLBEyFjqKiT2MttnSSUc2Zob\n"
"XhYkHC6zN1Mlq30N0e3Q61YK9LxMdU1vsluXxNq2rfK1Scb1oOlOOtlbV3zA3VRF\n"
"hV3t1nOA9tFmUrwZi0CUMWJE/zbPAyhwWotKyZkCgYEAh0kFicPdbABdrCglXVae\n"
"SnfSjVwYkVuGd5Ze0WADvjYsVkYBHTvhgRNnRJMg+/vWz3Sf4Ps4rgUbqK8Vc20b\n"
"AU5G6H6tlCvPRGm0ZxrwTWDHTcuKRVs+pJE8C/qWoklE/AAhjluWVoGwUMbPGuiH\n"
"6Gf1bgHF6oj/Sq7rv/VLZ8ECgYBeq7ml05YyLuJutuwa4yzQ/MXfghzv4aVyb0F3\n"
"QCdXR6o2IYgR6jnSewrZKlA9aPqFJrwHNR6sNXlnSmt5Fcf/RWO/qgJQGLUv3+rG\n"
"7kuLTNDR05azSdiZc7J89ID3Bkb+z2YkV+6JUiPq/Ei1+nDBEXb/m+/HqALU/nyj\n"
"P3gXeQKBgBusb8Rbd+KgxSA0hwY6aoRTPRt8LNvXdsB9vRcKKHUFQvxUWiUSS+L9\n"
"/Qu1sJbrUquKOHqksV5wCnWnAKyJNJlhHuBToqQTgKXjuNmVdYSe631saiI7PHyC\n"
"eRJ6DxULPxABytJrYCRrNqmXi5TCiqR2mtfalEMOPxz8rUU8dYyx\n"
"-----END RSA PRIVATE KEY-----\n";

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

WebUI::WebUI() : d(nullptr)
{
    JsonResponders["test"] = new StaticJsonResponder();
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
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
    }
    return ret;
}

int WebUI::ReturnJSON(struct MHD_Connection *connection, const std::string& url, const std::string& params)
{
#define MIMETYPE_JSON "application/json"
#define MIMETYPE_JSONP "text/javascript"
    
    struct MHD_Response *response;
    int ret;
    
    //    Json::FastWriter jsonout;
    Json::StyledWriter jsonout;
    
    Json::Value event;
    
    if (this->JsonResponders.count(url) != 0)
    {
        event = JsonResponders[url]->GetResponse(params);
    }
    else
    {
        event["url"] = url;
        event["params"] = params;
    }
    
    std::string jsonstring = jsonout.write(event);
    const char* jsoncstr = jsonstring.c_str();
    
    /*
     MHD_RESPMEM_MUST_FREE
     MHD_RESPMEM_MUST_COPY
     */
    response = MHD_create_response_from_buffer(strlen(jsoncstr),
                                               (void *)jsoncstr,
                                               MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header (response, "Content-Type", MIMETYPE_JSON);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

static int
iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data,
              uint64_t off, size_t size)
{
    struct connection_info_struct* con_info = (connection_info_struct*) coninfo_cls;
    
    if (0 == strcmp (key, "search"))
    {
        if ((size > 0) && (size <= MAXNAMESIZE))
        {
            char *answerstring;
            answerstring = new char[MAXANSWERSIZE];
            if (!answerstring) return MHD_NO;
            
            snprintf (answerstring, MAXANSWERSIZE, "%s", data);
            con_info->answerstring = answerstring;
        }
        else con_info->answerstring = nullptr;
        
        return MHD_NO;
    }
    
    return MHD_YES;
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
        con_info = new connection_info_struct;
        if (nullptr == con_info) return MHD_NO;
        con_info->answerstring = nullptr;
        
        if (0 == strcmp (method, "POST"))
        {
            
            con_info->postprocessor = MHD_create_post_processor(connection, POSTBUFFERSIZE, iterate_post, (void*) con_info);
            
            if (nullptr == con_info->postprocessor)
            {
                free(con_info);
                return MHD_NO;
            }
            con_info->connectiontype = 1;
        }
        else if (0 != strcmp(method, MHD_HTTP_METHOD_GET))
        {
            free(con_info);
            return MHD_NO;             /* unexpected method */
        }
        *con_cls = (void*)con_info;
        return MHD_YES;
    }
    
    std::string params = "";

    if (0 == strcmp (method, "POST"))
    {
        con_info = (connection_info_struct*)*con_cls;
        
        if (*upload_data_size != 0)
        {
            MHD_post_process (con_info->postprocessor, upload_data, *upload_data_size);
            *upload_data_size = 0;
            
            return MHD_YES;
        }
        else if (NULL != con_info->answerstring)
            params = con_info->answerstring;
//            return send_page (connection, con_info->answerstring);
    }
    
    if (strncmp(url, ROOTJSON, strlen(ROOTJSON)) == 0)
    {
        return ReturnJSON(connection, url, params);
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

int WebUI::start(uint16_t port)
{
    d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION | MHD_USE_DEBUG | MHD_USE_SSL,
                         port, // Port to bind to
                         nullptr, // callback to call to check which clients allowed to connect
                         nullptr, // extra argument to apc
                         &ahc, // handler called for all requests
                         this, // extra argument to dh
                         MHD_OPTION_CONNECTION_TIMEOUT, 256,
                         MHD_OPTION_HTTPS_MEM_KEY, key_pem,
                         MHD_OPTION_HTTPS_MEM_CERT, cert_pem,
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

int
main(int argc, char *const *argv)
{
    int port = 18080;
    
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }
    
    WebUI inst;
    int status = inst.start(port);
    
    if (status)
    {
        fprintf(stderr, "Error: failed to start TLS_daemon\n");
        return 1;
    }
    else
    {
        printf("MHD daemon listening on port %d\n", port);
    }
    
    (void)getc(stdin);
    
    inst.stop();
    
    return 0;
}

