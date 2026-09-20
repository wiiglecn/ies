#pragma once

class CInit
{
public:
	//CInit(void);
	static void Prepare();
	static void Unload();
	static void CmdUnloadSLTools();

public:
	//~CInit(void);

private:
	static void unloadMenu();
	static void loadMenu();
};
