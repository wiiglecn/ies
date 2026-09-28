#pragma once
#include "declare.h"
#include "ToolDrawBase.h"

class ToolAnnotation : public ToolDrawBase
{
public:
	static ToolAnnotation& getInstance();
	ToolAnnotation(const ToolAnnotation&);            
    ToolAnnotation& operator=(const ToolAnnotation&); 
public:
	void create_dimension_linear(AiToolCommandData* data);
	void create_dimension_aligned(AiToolCommandData* data);
	void create_dimension_angular(AiToolCommandData* data);
	void create_dimension_radius(AiToolCommandData* data);
	void create_leader(AiToolCommandData* data);
	
private:
	ToolAnnotation(void);
    ~ToolAnnotation(void);
};
