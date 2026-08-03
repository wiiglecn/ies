#pragma once

class CBaseMarker
{
public:
	CBaseMarker(void);
public:
	~CBaseMarker(void);
	void SetDiameter(float diameter);
protected:
	float m_Diameter;
};
