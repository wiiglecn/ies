#include "BaseMarker.h"

CBaseMarker::CBaseMarker(void):m_Diameter(0)
{	
}

CBaseMarker::~CBaseMarker(void)
{
}

void CBaseMarker::SetDiameter(float diameter)
{
	this->m_Diameter = diameter;
}