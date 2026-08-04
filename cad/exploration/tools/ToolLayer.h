#pragma once
#include "declare.h"
#include "ToolDrawBase.h"
class cJSON;

class ToolLayer : public ToolDrawBase
{
public:
	static ToolLayer& getInstance();
	ToolLayer(const ToolLayer&);            
    ToolLayer& operator=(const ToolLayer&); 
public:
	void create_layer(AiToolCommandData* data);
	void list_layers(AiToolCommandData* data);
	void query_all_layers(AiToolCommandData* data);

	void query_layer(AiToolCommandData* data);
    void get_current_layer(AiToolCommandData* data);
    void set_current_layer(AiToolCommandData* data);
    void get_current_layer_color(AiToolCommandData* data);
	
	void regen();
private:
	ToolLayer(void);
    ~ToolLayer(void);
};
