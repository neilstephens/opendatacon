/*	opendatacon
 *
 *	Copyright (c) 2020:
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
 * BinaryControl.h
 *
 *  Created on: 2021-02-08
 *  A year of hope that pandemic will end soon
 *      Author: Rakesh Kumar <cpp.rakesh@gmail.com>
 */

#ifndef BINARYCONTROL_H
#define BINARYCONTROL_H

#include "Log.h"
#include <opendatacon/IOTypes.h>
#include <opendatacon/asio.h>
#include <json/json.h>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

enum class FeedbackMode { PULSE, LATCH };

enum class FeedbackType : uint8_t
{
	ANALOG = 1,
	BINARY = 2,
	BCD = 3,
	UNDEFINED = 4
};

enum class PositionAction : uint8_t
{
	RAISE = 1,
	LOWER = 2,
	UNDEFINED = 3
};

inline FeedbackType ToFeedbackType(const std::string& str_type)
{
	FeedbackType type = FeedbackType::UNDEFINED;
	if (odc::to_lower(str_type) == "analog")
		type = FeedbackType::ANALOG;
	else if (odc::to_lower(str_type) == "binary")
		type = FeedbackType::BINARY;
	else if (odc::to_lower(str_type) == "bcd")
		type = FeedbackType::BCD;
	return type;
}

inline PositionAction ToPositionAction(const std::string& str_action)
{
	PositionAction action = PositionAction::UNDEFINED;
	if (odc::to_lower(str_action) == "raise")
		action = PositionAction::RAISE;
	else if (odc::to_lower(str_action) == "lower")
		action = PositionAction::LOWER;
	else
		Log.Error("Invalid action for Postion feedback (use 'RAISE' or 'LOWER') : '{}'", str_action);
	return action;
}

inline std::string ToString(const FeedbackType type)
{
	ENUMSTRING(type, FeedbackType, ANALOG            )
	ENUMSTRING(type, FeedbackType, BINARY            )
	ENUMSTRING(type, FeedbackType, BCD               )
	ENUMSTRING(type, FeedbackType, UNDEFINED         )
	return "<no_string_representation>";
}

//DNP3 has 3 control models: complimentary (1-output) latch, complimentary 2-output (pulse), activation (1-output) pulse
//We can generalise, and come up with a simpler superset:
//	-have an arbitrary length list of outputs
//	-arbitrary on/off values for each output
//	-each output either pulsed or latched
struct BinaryFeedback
{
	std::shared_ptr<odc::EventInfo> on_value;
	std::shared_ptr<odc::EventInfo> off_value;
	FeedbackMode mode;
	uint32_t delay;
	std::size_t update_interval;

	BinaryFeedback(const std::shared_ptr<odc::EventInfo>& on,
		const std::shared_ptr<odc::EventInfo>& off,
		FeedbackMode amode,
		uint32_t delay_ms,
		std::size_t u_interval):
		on_value(on),
		off_value(off),
		mode(amode),
		delay(delay_ms),
		update_interval(u_interval)
	{}
};

struct PositionFeedback
{
	PositionFeedback(FeedbackType feedback_type,
		const std::vector<PositionAction>& an,
		const std::vector<std::size_t>& index,
		std::size_t l_limit, std::size_t r_limit,
		double r_tap_step):
		type(feedback_type), action(an), indexes(index), lower_limit(l_limit), raise_limit(r_limit), tap_step(r_tap_step)
	{}

	FeedbackType type;
	std::vector<PositionAction> action;
	std::vector<std::size_t> indexes;
	std::size_t lower_limit;
	std::size_t raise_limit;
	double tap_step;
};

class BinaryControl
{
public:
	void CreateBinaryControl(std::size_t index,
		const std::shared_ptr<odc::EventInfo>& on,
		const std::shared_ptr<odc::EventInfo>& off,
		FeedbackMode mode,
		uint32_t delay,
		std::size_t update_interval);
	std::vector<std::shared_ptr<BinaryFeedback>> BinaryFeedbacks(std::size_t index);

	void CreateBinaryControl(std::size_t index,
		const std::string& port_source,
		FeedbackType type,
		const std::vector<std::size_t>& indexes,
		const std::vector<PositionAction>& action,
		std::size_t lower_limit, std::size_t raise_limit,
		double tap_step);
	std::shared_ptr<PositionFeedback> GetPositionFeedback(std::size_t index);

	void CreateBinaryControl(std::size_t index);

	bool IsIndex(std::size_t index);

	void SetLatestControlEvent(const std::shared_ptr<odc::EventInfo>& event, std::size_t index);
	std::shared_ptr<odc::EventInfo> GetCurrentBinaryEvent(std::size_t index);

private:
	std::shared_timed_mutex feedback_mutex;
	std::shared_timed_mutex position_mutex;
	std::shared_timed_mutex current_mutex;

	std::unordered_map<std::size_t, std::vector<std::shared_ptr<BinaryFeedback>>> m_binary_feedbacks;
	std::unordered_map<std::size_t, std::shared_ptr<PositionFeedback>> m_position_feedbacks;
	std::unordered_map<std::size_t, std::shared_ptr<odc::EventInfo>> m_latest_control_events;
};

#endif // BINARYCONTROL_H
