#pragma once

class cJSON;

class ToolDraw
{
public:
	ToolDraw(void);
public:
	~ToolDraw(void);
	static void create_line(cJSON* params);
};
