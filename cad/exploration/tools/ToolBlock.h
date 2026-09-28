#pragma once
#include "declare.h"
#include "ToolDrawBase.h"

class ToolBlock : public ToolDrawBase
{
public:
	static ToolBlock& getInstance();
	ToolBlock(const ToolBlock&);            
    ToolBlock& operator=(const ToolBlock&); 
public:

	void block_list(AiToolCommandData* data);
	void block_insert(AiToolCommandData* data);
	void block_insert_with_attributes(AiToolCommandData* data);
	void block_get_attributes(AiToolCommandData* data);
	void block_update_attributes(AiToolCommandData* data);
	void block_define(AiToolCommandData* data);
	
private:
	ToolBlock(void);
    ~ToolBlock(void);
};
