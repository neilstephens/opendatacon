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
//  MhdWrapper.cpp
//  opendatacon
//
//  Created by Alan Murray on 14/09/2014.
//
//

#include "MhdWrapper.h"
#include <iostream>
#include <opendatacon/util.h>

const int POSTBUFFERSIZE = 512;
const char EMPTY_PAGE[] = "<html><head><title>File not found</title></head><body>File not found</body></html>";

const std::unordered_map<std::string, const std::string> MimeTypeMap {
	{ "json", "application/json" },
	{ "js", "text/javascript" },
	{ "html", "text/html"},
	{ "jpg", "image/jpeg"},
	{ "css", "text/css"},
	{ "txt", "text/plain"},
	{ "svg", "image/svg+xml"},
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

static ssize_t
file_reader(void *cls, uint64_t pos, char *buf, size_t max)
{
	FILE *file = reinterpret_cast<FILE *>(cls);

	(void)fseek(file, pos, SEEK_SET);
	return fread(buf, 1, max, file);
}

static void
file_free_callback(void *cls)
{
	FILE *file = reinterpret_cast<FILE *>(cls);
	fclose(file);
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

int ReturnFile(struct MHD_Connection *connection, const std::string& url)
{
	struct stat buf{};
	FILE *file;
	struct MHD_Response *response;
	int ret;

	if ((0 == stat(url.c_str(), &buf)) && (S_ISREG(buf.st_mode)))
		fopen_s(&file, url.c_str(), "rb");
	else
		file = nullptr;
	if (file == nullptr)
	{
		if (auto log = odc::spdlog_get("WebUI"))
			log->error("WebUI : Failed to open file {}", url);

		response = MHD_create_response_from_buffer(strlen(EMPTY_PAGE),
			(void *)EMPTY_PAGE,
			MHD_RESPMEM_PERSISTENT);
		ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
		MHD_destroy_response(response);
	}
	else
	{
		response = MHD_create_response_from_callback(buf.st_size, 32 * 1024, /* 32k PAGE_NOT_FOUND size */
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

int ReturnJSON(struct MHD_Connection *connection, const std::string& json_str)
{
	struct MHD_Response *response;
	int ret;

	/*
	 MHD_RESPMEM_MUST_FREE
	 MHD_RESPMEM_MUST_COPY
	 */
	response = MHD_create_response_from_buffer(json_str.size(),
		(void *)json_str.c_str(),
		MHD_RESPMEM_MUST_COPY);
	MHD_add_response_header (response, "Content-Type", MimeTypeMap.at("json").c_str());
	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);

	return ret;
}

/*
 * adapted from MHD_http_unescape
 */
std::string post_unescape(const char *val)
{
	std::string result;
	const char *rpos = val;
	//char *wpos = val;
	char *end;
	char buf3[3];

	while ('\0' != *rpos)
	{
		switch (*rpos)
		{
			case '+':
			{
				result += ' ';
				//*wpos = ' ';
				//wpos++;
				rpos++;
				break;
			}
			case '%':
			{
				if ( ('\0' == rpos[1]) ||
				     ('\0' == rpos[2]) )
				{
					//*wpos = '\0';
					//return wpos - val;
					return result;
				}
				buf3[0] = rpos[1];
				buf3[1] = rpos[2];
				buf3[2] = '\0';
				auto num = strtoul (buf3, &end, 16);
				if ('\0' == *end)
				{
					//*wpos = (char)((unsigned char) num);
					//wpos++;
					result += (char)((unsigned char) num);
					rpos += 3;
					break;
				}
				/* intentional fall through! */
			}
			default:
			{
				//*wpos = *rpos;
				//wpos++;
				result += *rpos;
				rpos++;
			}
		}
	}
	//*wpos = '\0'; /* add 0-terminator */
	//return wpos - val; /* = strlen(val) */
	return result;
}

static int
iterate_post (void *coninfo_cls,
	enum MHD_ValueKind kind, //MHD_POSTDATA_KIND
	const char *key,         // POST KEY
	const char *filename,
	const char *content_type,
	const char *transfer_encoding,
	const char *data, // POST VALUE
	uint64_t off,
	size_t size // POST VALUE LENGTH
	)
{
	auto con_info = reinterpret_cast<connection_info_struct*>(coninfo_cls);

	if (kind == MHD_POSTDATA_KIND)
	{
		con_info->PostValues[post_unescape(key)] = post_unescape(data);
		return MHD_YES;
	}
	return MHD_NO;
}

void request_completed(void *cls, struct MHD_Connection *connection,
	void **con_cls,
	enum MHD_RequestTerminationCode toe)
{
	auto *con_info = reinterpret_cast<connection_info_struct*>(*con_cls);

	if (nullptr == con_info) return;
	if (nullptr != con_info->postprocessor) MHD_destroy_post_processor(con_info->postprocessor);
	con_info->postprocessor = nullptr;
	free(con_info);
	*con_cls = nullptr;
}

int CreateNewRequest(void *cls,
	struct MHD_Connection *connection,
	const std::string& url,
	const std::string& method,
	const std::string& version,
	const std::string& upload_data,
	size_t& upload_data_size,
	void **con_cls)
{
	struct connection_info_struct *con_info;

	con_info = new connection_info_struct;
	if (nullptr == con_info) return MHD_NO;
	*con_cls = (void*)con_info;

	if (method == MHD_HTTP_METHOD_GET) return MHD_YES;
	if (method == "POST")
	{
		auto length = atoi(MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Content-Length"));
		if (length == 0) return MHD_YES;
		con_info->postprocessor = MHD_create_post_processor(connection, POSTBUFFERSIZE, iterate_post, (void*) con_info);
		if (nullptr != con_info->postprocessor) return MHD_YES;
	}

	// unexpected method or couldn't create post processor
	delete con_info;
	*con_cls = nullptr;
	return MHD_NO;
}
