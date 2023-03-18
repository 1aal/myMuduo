#pragma once

#include <chrono>
#include <stdio.h>
#include <string>
#include <inttypes.h>

class Timestamp
{
private:
    int64_t microSecondsSinceEpoch_;

public:
    Timestamp(/* args */) : microSecondsSinceEpoch_(0){};// return a invalid Timestamp
    Timestamp(std::chrono::system_clock::time_point now);
    explicit Timestamp(int64_t microSecondsSinceEpoch) : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}
    ~Timestamp();
    static const int kMicroSecondsPerSecond = 1000 * 1000;

    void swap(Timestamp &that)
    {
        int64_t temp = that.microSecondsSinceEpoch_;
        that.microSecondsSinceEpoch_ = microSecondsSinceEpoch_;
        microSecondsSinceEpoch_ = temp;
    }

    std::string toString() const;
    std::string toFormattedString(bool showMicroseconds = true) const;

    bool valid() const { return microSecondsSinceEpoch_ > 0; }
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
    time_t secondsSinceEpoch() const
    {
        return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    }
    static Timestamp now();
    static Timestamp invalid(){
        return Timestamp();
    }
    static Timestamp fromUnixTime(time_t t){
        return fromUnixTime(t,0);
    }
    
    static Timestamp fromUnixTime(time_t t, int microsecond){
        return Timestamp(static_cast<int64_t>(t) * kMicroSecondsPerSecond * microsecond );
    }

    bool operator<(Timestamp rhs) const{
        return microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
    }
    bool operator==(Timestamp rhs) const{
        return microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
    }
    bool operator<=(Timestamp rhs) const{
        return *this < rhs || *this == rhs;
    }
    bool operator>(Timestamp rhs) const{
        return ! (*this <= rhs) ;
    }
    bool operator>=(Timestamp rhs) const{
        return !(*this < rhs);
    }
    bool operator!=(Timestamp rhs) const{
        return ! (*this== rhs);
    }
};

inline double timeDifference(Timestamp high, Timestamp low){
    int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
    return static_cast<double>(diff)/Timestamp::kMicroSecondsPerSecond;
}

inline Timestamp addTime(Timestamp timestamp, double seconds){
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}


struct DateTime
{
  DateTime() {}
  explicit DateTime(const struct tm&);
  DateTime(int _year, int _month, int _day, int _hour, int _minute, int _second)
      : year(_year), month(_month), day(_day), hour(_hour), minute(_minute), second(_second)
  {
  }

  // "2011-12-31 12:34:56"
  std::string toIsoString() const;

  int year = 0;     // [1900, 2500]
  int month = 0;    // [1, 12]
  int day = 0;      // [1, 31]
  int hour = 0;     // [0, 23]
  int minute = 0;   // [0, 59]
  int second = 0;   // [0, 59]
};
