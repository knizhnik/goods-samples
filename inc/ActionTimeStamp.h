//
// -- ActionTimeStamp.h
//

#pragma once

class CActionTimeStamp
{
public:
	CActionTimeStamp()
	{
	}
	CActionTimeStamp(const wchar_t* action_by)
		: m_ActionBy(action_by)
		, m_ActionOn(time(nullptr))
	{
	}
	CActionTimeStamp(const wchar_t*  const& action_by, time_t action_on)
		: m_ActionBy(action_by)
		, m_ActionOn(action_on)
	{
	}
	CActionTimeStamp(CActionTimeStamp const& other)
		: m_ActionBy(other.m_ActionBy)
		, m_ActionOn(other.m_ActionOn)	
	{
	}

	CActionTimeStamp& operator=(CActionTimeStamp const& other)
	{
		m_ActionBy = other.m_ActionBy;
		m_ActionOn = other.m_ActionOn;
		return *this;
	}

public:
	std::wstring GetActionBy() const
	{
		return m_ActionBy.getSafeWideChar();
	}
	nat8 GetActionOn() const
	{
		return m_ActionOn;
	}

	void SetActionBy(const wchar_t* entity)
	{
		m_ActionBy = entity;
	}
	void SetActionOn(nat8 date_time) 
	{
		m_ActionOn = date_time;
	}

private:
    field_descriptor& describe_components();

    friend field_descriptor& describe_field(CActionTimeStamp& action_time_stamp);

private:
	wstring_t	m_ActionBy;
	nat8		m_ActionOn;	
};


inline field_descriptor& describe_field(CActionTimeStamp& action_time_stamp) 
{ 
    return action_time_stamp.describe_components();
}
