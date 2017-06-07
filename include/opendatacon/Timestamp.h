//
//  Timestamp.h
//  opendatacon_suite
//
//  Created by Alan Murray on 2/05/2017.
//
//

#ifndef opendatacon_suite_Timestamp_h
#define opendatacon_suite_Timestamp_h

#include <chrono>
#include <opendnp3/Types.h>

namespace odc {
	// my_mktime  converts from  struct tm  UTC to non-leap seconds since
	// 00:00:00 on the first UTC day of year 1970 (fixed).
	// It works from 1970 to 2105 inclusive. It strives to be compatible
	// with C compilers supporting // comments and claiming C89 conformance.
	//
	// input:   Pointer to a  struct tm  with field tm_year, tm_mon, tm_mday,
	//          tm_hour, tm_min, tm_sec set per  mktime  convention; thus
	//          - tm_year is year minus 1900
	//          - tm_mon is [0..11] for January to December, but [-2..14]
	//            works for November of previous year to February of next year
	//          - tm_mday, tm_hour, tm_min, tm_sec similarly can be offset to
	//            the full range [-32767 to 32768], as long as the combination
	//            with tm_year gives a result within years [1970..2105], and
	//            tm_year>0.
	// output:  Number of non-leap seconds since beginning of the first UTC
	//          day of year 1970, as an unsigned at-least-32-bit integer.
	//          The input is not changed (in particular, fields tm_wday,
	//          tm_yday, and tm_isdst are unchanged and ignored).
	
	inline uint32_t my_mktime(const tm *ptm) {
		int m, y = ptm->tm_year;
		if ((m = ptm->tm_mon)<2) { m += 12; --y; }
		return ((( (uint32_t)(y-69)*365u+y/4-y/100*3/4+(m+2)*153/5-446+
				  ptm->tm_mday)*24u+ptm->tm_hour)*60u+ptm->tm_min)*60u+ptm->tm_sec;
	}
	
	struct Clock
	{
		typedef std::chrono::nanoseconds         duration;
		typedef duration::rep                     rep;
		typedef duration::period                  period;
		typedef std::chrono::time_point<Clock>    time_point;
		static const bool is_steady =             false;
		
		static time_point now() noexcept
		{
			using namespace std::chrono;
			return time_point
			(
			 duration_cast<duration>(system_clock::now().time_since_epoch())
			 );
		}
		
		static time_t to_time_t(const time_point& __t)
		{
			return std::chrono::time_point_cast<std::chrono::seconds>(__t).time_since_epoch().count();
		}
		
		static time_point from_time_t(time_t __t)
		{
			return odc::Clock::time_point(std::chrono::seconds(__t));
		}
		
		// my_mktime  converts from  struct tm  UTC to non-leap seconds since
		// 00:00:00 on the first UTC day of year 1970 (fixed).
		// It works from 1970 to 2105 inclusive. It strives to be compatible
		// with C compilers supporting // comments and claiming C89 conformance.
		//
		// input:   Pointer to a  struct tm  with field tm_year, tm_mon, tm_mday,
		//          tm_hour, tm_min, tm_sec set per  mktime  convention; thus
		//          - tm_year is year minus 1900
		//          - tm_mon is [0..11] for January to December, but [-2..14]
		//            works for November of previous year to February of next year
		//          - tm_mday, tm_hour, tm_min, tm_sec similarly can be offset to
		//            the full range [-32767 to 32768], as long as the combination
		//            with tm_year gives a result within years [1970..2105], and
		//            tm_year>0.
		// output:  Number of non-leap seconds since beginning of the first UTC
		//          day of year 1970, as an unsigned at-least-32-bit integer.
		//          The input is not changed (in particular, fields tm_wday,
		//          tm_yday, and tm_isdst are unchanged and ignored).
		static time_point from_tm(const tm& __tm) {
			return odc::Clock::time_point(std::chrono::seconds(my_mktime(&__tm)));
		}
	};
	
	class Timestamp : public odc::Clock::time_point
	{
	public:
		explicit Timestamp(const duration& __d) : odc::Clock::time_point(__d) {}
		Timestamp(const tm& __tm) : odc::Clock::time_point(odc::Clock::from_tm(__tm)) {}
		explicit Timestamp(uint64_t a) : Timestamp(std::chrono::milliseconds(a))
		{}
		
		inline operator opendnp3::DNPTime() const
		{
			return opendnp3::DNPTime(std::chrono::time_point_cast<std::chrono::milliseconds>(*this).time_since_epoch().count());
		}
	};
	
	//typedef	opendnp3::DNPTime Timestamp;
}

#endif
