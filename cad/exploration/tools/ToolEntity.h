#pragma once
#include "declare.h"
#include "ToolDrawBase.h"


class ToolEntity : public ToolDrawBase
{
public:
	static ToolEntity& getInstance();
	ToolEntity(const ToolEntity&);            
    ToolEntity& operator=(const ToolEntity&); 
public:

	void delete_entities(AiToolCommandData* data);
	void list_entities(AiToolCommandData* data);

	void entity_get(AiToolCommandData* data);
	void entity_set_color(AiToolCommandData* data);
	void entity_copy(AiToolCommandData* data);
	void entity_move(AiToolCommandData* data);
	void entity_rotate(AiToolCommandData* data);
	void entity_scale(AiToolCommandData* data);
	void entity_mirror(AiToolCommandData* data);
	void entity_offset(AiToolCommandData* data);
	void entity_fillet(AiToolCommandData* data);
	void entity_chamfer(AiToolCommandData* data);
	void entity_array(AiToolCommandData* data);

	void get_selected_entity(AiToolCommandData* data);
    void set_selected_entity(AiToolCommandData* data);
	
private:
	ToolEntity(void);
    ~ToolEntity(void);
};
