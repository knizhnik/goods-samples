#pragma once

class CDimensions : public object
{
public:
	METACLASS_DECLARATIONS(CDimensions, object);

	static ref<CDimensions> create(double length, double width, double height)
	{
		return NEW CDimensions(self_class, length, width, height);
	}

	double GetLength() const
	{
		return m_Length;
	}

	double GetWidth() const
	{
		return m_Width;
	}

	double GetHeight() const
	{
		return m_Height;
	}

	void SetLength(double length) 
	{
		m_Length = length;
	}

	void SetWidth(double width) 
	{
		m_Width = width;
	}

	void SetHeight(double height) 
	{
		m_Height = height;
	}


protected:
	CDimensions(class_descriptor& desc, double length, double width, double height);

private:
	double	m_Length;
	double	m_Width;
	double	m_Height;
};
