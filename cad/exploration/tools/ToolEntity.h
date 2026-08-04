#pragma once
#include "declare.h"
#include "ToolDrawBase.h"
class cJSON;

class ToolEntity : public ToolDrawBase
{
public:
	static ToolEntity& getInstance();
	ToolEntity(const ToolEntity&);            
    ToolEntity& operator=(const ToolEntity&); 
public:

	void delete_entities(AiToolCommandData* data);
	void list_entities(AiToolCommandData* data);
	
	
private:
	ToolEntity(void);
    ~ToolEntity(void);
};
