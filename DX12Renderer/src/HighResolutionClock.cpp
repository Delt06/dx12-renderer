#include "HighResolutionClock.h"

#include "DX12LibPCH.h"

HighResolutionClock::HighResolutionClock()
	: DeltaTime(0)
	  , TotalTime(0)
{
	T0 = std::chrono::high_resolution_clock::now();
}

void HighResolutionClock::Tick()
{
	const auto t1 = std::chrono::high_resolution_clock::now();
	DeltaTime = t1 - T0;
	TotalTime += DeltaTime;
	T0 = t1;
}

void HighResolutionClock::Reset()
{
	T0 = std::chrono::high_resolution_clock::now();
	DeltaTime = std::chrono::high_resolution_clock::duration();
	TotalTime = std::chrono::high_resolution_clock::duration();
}

double HighResolutionClock::GetDeltaNanoseconds() const
{
	return static_cast<double>(DeltaTime.count()) * 1.0;
}

double HighResolutionClock::GetDeltaMicroseconds() const
{
	return static_cast<double>(DeltaTime.count()) * 1e-3;
}

double HighResolutionClock::GetDeltaMilliseconds() const
{
	return static_cast<double>(DeltaTime.count()) * 1e-6;
}

double HighResolutionClock::GetDeltaSeconds() const
{
	return static_cast<double>(DeltaTime.count()) * 1e-9;
}

double HighResolutionClock::GetTotalNanoseconds() const
{
	return static_cast<double>(TotalTime.count()) * 1.0;
}

double HighResolutionClock::GetTotalMicroseconds() const
{
	return static_cast<double>(TotalTime.count()) * 1e-3;
}

double HighResolutionClock::GetTotalMilliseconds() const
{
	return static_cast<double>(TotalTime.count()) * 1e-6;
}

double HighResolutionClock::GetTotalSeconds() const
{
	return static_cast<double>(TotalTime.count()) * 1e-9;
}
