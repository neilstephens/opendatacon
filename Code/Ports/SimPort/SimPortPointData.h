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
 * SimPortPointData.h
 *
 *  Created on: 2020-07-15
 *  The year of bush fires and pandemic
 *      Author: Rakesh Kumar <cpp.rakesh@gmail.com>
 */

#ifndef SIMPORTPOINTDATA_H
#define SIMPORTPOINTDATA_H

#include "BinaryControl.h"
#include <opendatacon/IOTypes.h>

typedef asio::basic_waitable_timer<std::chrono::steady_clock> Timer_t;
typedef std::shared_ptr<Timer_t> ptimer_t;
const std::size_t OFF = 0;
const std::size_t ON = 1;

struct Point
{
	Point():
		std_dev(0.0f),
		update_interval(0),
		start_value(0.0f),
		start_quality(odc::QualityFlags::ONLINE),
		forced_state(false) {}

	std::shared_ptr<odc::EventInfo> event;
	double std_dev;
	// This refresh rate is in milliseconds
	std::size_t update_interval;
	double start_value;
	odc::QualityFlags start_quality;
	bool forced_state;
};


class SimPortPointData
{
public:
	SimPortPointData();

	// ---------------------------------------------------------------------- //
	//  These all funtions are for Points[Analog / Binary] manipulations
	// ---------------------------------------------------------------------- //

	/*
	  function         : CreateEvent
	  description      : this function will create an event [Analog / Bianry]
	                     and initialize it with default and provided values
	  param type       : event type like Analog / Binary
	  param index      : index of the data point
	  param name       : name of the sim port reading from configuration
	  param flag       : quality of the data point
	  param s_dev      : standard deviation for randomizing the data point
	  param u_interval : update interval for randomizaing the data point
	  param val        : starting value of the data point
	  return           : void
	 */
	void CreateEvent(odc::EventType type, std::size_t index, const std::string& name,
		odc::QualityFlags flag, double s_dev, std::size_t u_interval, double val);

	/*
	  function         : Event
	  description      : this function will set the event
	  param event      : the event info which holds the event type and index
	  return           : void
	 */
	void Event(std::shared_ptr<odc::EventInfo> event);

	/*
	  function         : Event
	  description      : this function will return the event
	  param event      : the event info which holds the event type and index
	  return           : point to the event
	*/
	std::shared_ptr<odc::EventInfo> Event(odc::EventType type, std::size_t index);

	/*
	  function         : SetCurrrentBinaryControl
	  description      : this function will set the current binary control event
	  param event      : the event info which holds the event information
	  param event      : the index of the binary control
	  return           : void
	 */
	void SetLatestControlEvent(std::shared_ptr<odc::EventInfo> event, std::size_t index);

	/*
	  function         : ForcedState
	  description      : this function will set the forced state
	  param type       : the event type
	  param index      : index of the data point
	  param state      : new value to be forced the data point
	  return           : void
	*/
	void ForcedState(odc::EventType type, std::size_t index, bool state);

	/*
	  function         : ForcedState
	  description      : this function will set the forced state
	  param type       : the event type
	  param index      : index of the data point
	  return           : return the forced state of the data point
	*/
	bool ForcedState(odc::EventType type, std::size_t index);

	/*
	  function         : UpdateInterval
	  description      : this function will set the update interval of the data point
	  param type       : the event type
	  param index      : index of the data point
	  param value      : new upate interval for the data point [SI Unit milliseconds]
	  return           : void
	*/
	void UpdateInterval(odc::EventType type, std::size_t index, std::size_t value);

	/*
	  function         : UpdateInterval
	  description      : this function will get the update interval of the data point
	  param type       : the event type
	  param index      : index of the data point
	  return           : update interval value [SI Unit milliseconds]
	*/
	std::size_t UpdateInterval(odc::EventType type, std::size_t index);
	/*
	  function         : Payload
	  description      : this function will set the payload for the data point
	  param type       : the event type
	  param index      : index of the data point
	  param payload    : new payload to the data point
	  return           : void
	*/
	void Payload(odc::EventType type, std::size_t index, double payload);

	/*
	  function         : Payload
	  description      : this function will get the payload for the data point
	  param type       : the event type
	  param index      : index of the data point
	  return           : payload for the data point
	*/
	double Payload(odc::EventType type, std::size_t index);

	/*
	  function         : Payload
	  description      : this function will get the payload for the data point
	  param type       : the event type
	  param index      : index of the data point
	  return           : payload for the data point
	*/
	double StartValue(odc::EventType type, std::size_t index);

	/*
	  function         : StartQuality
	  description      : this function will get the starting quality for the data point
	  param type       : the event type
	  param index      : index of the data point
	  return           : starting quality for the data point
	*/
	odc::QualityFlags StartQuality(odc::EventType type, std::size_t index);

	/*
	  function         : StdDev
	  description      : this function will get the standard deviation of the data point
	  param type       : index
	  return           : standard deviation of the data point
	*/
	double StdDev(std::size_t index);

	/*
	  function         : Indexes
	  description      : this function will get all the indexes
	  param type       : type
	  return           : all the analog / binary indexes
	*/
	std::vector<std::size_t> Indexes(odc::EventType type);

	/*
	  function         : Values
	  description      : this function will get all the payload
	  param type       : type
	  return           : all the analog / binary payloads
	*/
	std::unordered_map<std::size_t , double> Values(odc::EventType type);

	/*
	  function         : CurrentState
	  description      : this function will return the current state of all the data points
	  param type       : NA
	  return           : current state of all the data points
	*/
	Json::Value CurrentState();

	/*
	  function         : CurrentState
	  description      : this function will return the current state of all the provided indexes
	  param type       : event type
	  param indexes    : vector of the indexes
	  return           : current state of the data points
	*/
	std::string CurrentState(odc::EventType type, std::vector<std::size_t>& indexes);

	void Timer(const std::string& name, ptimer_t ptr);
	ptimer_t Timer(const std::string& name);
	void CancelTimers();

	/*
	  function         : IsIndex
	  description      : this function will find the index into the points container
	  param type       : event type
	  param index      : index
	  return           : is index exist or not
	*/
	bool IsIndex(odc::EventType type, std::size_t index);

	// ---------------------------------------------------------------------- //
	//  These all funtions are for Binary Control manipulations
	// ---------------------------------------------------------------------- //

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

private:
	std::shared_timed_mutex point_mutex;
	std::shared_timed_mutex timer_mutex;
	using Points = std::unordered_map<std::size_t, std::shared_ptr<Point>>;
	std::unordered_map<odc::EventType, Points> m_points;
	BinaryControl m_binary_control;
	std::unordered_map<std::string, ptimer_t> m_timers;
};

#endif // SIMPORTPOINTDATA_H
