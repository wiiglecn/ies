#pragma once
#include "declare.h"
#include "ToolDrawBase.h"


class ToolStyle : public ToolDrawBase
{
public:
	static ToolStyle& getInstance();
	ToolStyle(const ToolStyle&);            
    ToolStyle& operator=(const ToolStyle&); 
public:
    // Text style operations
    void add_text_style(AiToolCommandData* data);
    void modify_text_style(AiToolCommandData* data);
    void delete_text_style(AiToolCommandData* data);

    void list_text_styles(AiToolCommandData* data);
    void get_text_styles(AiToolCommandData* data);

    // Dimension style operations
    void add_dim_style(AiToolCommandData* data);
    void modify_dim_style(AiToolCommandData* data);
    void delete_dim_style(AiToolCommandData* data);

    void list_dim_styles(AiToolCommandData* data);
    void get_dim_styles(AiToolCommandData* data);

private:
	ToolStyle(void);
    ~ToolStyle(void);
};
