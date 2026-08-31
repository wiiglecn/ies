// coding: gb18030
#pragma once
#include <afxmt.h>
#include <fstream>
#include <string>

// 日志级别枚举
enum LogLevel
{
	LOG_LEVEL_DEBUG = 0,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_ERROR
};


class CLogger
{
public:
	

    // 获取单例实例
    static CLogger& GetInstance();

    // 初始化日志
    // strPrefix: 文件名前缀，strDir: 日志目录
    void Init(const CString& strPrefix, const CString& strDir = _T(""));

    // 设置日志级别
    void SetLevel(LogLevel nLevel);

    // 写日志
    
    void Log(LogLevel nLevel, const char* pszFmt, ...); // 新增重载

    // 便捷方法
    
    void Debug(const char* pszFmt, ...); // 新增重载
	void Debug(const std::string& s);
    
    void Info(const char* pszFmt, ...); // 新增重载
    
    void Warning(const char* pszFmt, ...); // 新增重载
    
    void Error(const char* pszFmt, ...); // 新增重载

#if defined(_UNICODE) || defined(UNICODE)
	void Log(LogLevel nLevel, const TCHAR* pszFmt, ...);
	void Debug(const TCHAR* pszFmt, ...);
	void Info(const TCHAR* pszFmt, ...);
	void Warning(const TCHAR* pszFmt, ...);
	void Error(const TCHAR* pszFmt, ...);
#endif

private:
    CLogger(void);
    ~CLogger(void);

    // 禁止拷贝
    CLogger(const CLogger&);
    CLogger& operator=(const CLogger&);

    // 检查并切换日志文件（按天）
    void CheckAndRotateFile();

    // 获取当前日期字符串 YYYY-MM-DD
    CString GetCurrentDateString() const;

    // 获取当前时间字符串 HH:MM:SS
    CString GetCurrentTimeString() const;

    // 日志级别转字符串
    LPCTSTR LevelToString(LogLevel nLevel) const;

    // 确保目录存在
    void EnsureDirectory(const CString& strDir);
    // 直接处理已是 UTF-8 的字符串缓冲区的内部方法
    void LogUtf8(LogLevel nLevel, const char* szBuffer);

private:
    CString     m_strPrefix;     // 文件名前缀
    CString     m_strDir;        // 日志目录
    CString     m_strCurrentDate;// 当前日志文件对应的日期
    CString     m_strFilePath;   // 当前日志文件完整路径
    LogLevel    m_nLevel;        // 当前日志级别
    std::ofstream m_ofs;         // 文件输出流
    CCriticalSection m_cs;       // 线程同步锁 (MFC)
};