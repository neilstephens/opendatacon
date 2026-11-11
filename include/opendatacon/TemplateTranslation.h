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
/*
 * TemplateTranslation.h
 *
 *  Text template (de)serialisers. Placeholders are in the form <NAME>, eg. <INDEX>, <PAYLOAD>, <POINT:name>.
 */

#ifndef ODC_TEMPLATETRANSLATION_H
#define ODC_TEMPLATETRANSLATION_H

#include <opendatacon/Translation.h>

namespace odc
{

class TemplateSerialiser: public ISerialiser
{
public:
	TemplateSerialiser(const std::string& template_str, const TranslationConfig& config);
	std::vector<uint8_t> Serialise(const std::shared_ptr<const EventInfo>& event, const std::string& SenderName, const ExtraPointFields& extra_fields) const override;
	std::string FindNonStaticElement(SourceLookupMethod lookup) const override;

	//Expand a template to a string
	static std::string Fill(const std::string& template_str, const std::shared_ptr<const EventInfo>& event, const std::string& SenderName, const ExtraPointFields& extra_fields, const std::string& dt_fmt, const bool utc, const DataToStringMethod D2S);

private:
	const std::string Template;
	const TranslationConfig Config;
};

class TemplateDeserialiser: public IDeserialiser
{
public:
	TemplateDeserialiser(const std::string& template_str, const TranslationConfig& config);
	~TemplateDeserialiser() override;
	std::shared_ptr<EventInfo> Deserialise(const uint8_t* data, size_t size, const KeyContext& key) override;
	std::vector<std::string> PointFieldNames() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;
};

} //namespace odc

#endif // ODC_TEMPLATETRANSLATION_H
