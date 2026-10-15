# KsExploration.arx AutoCAD端启动加载说明

## 一、前置条件
1.  已安装对应版本的AutoCAD（ObjectARX 2005工程，适配AutoCAD 2005/2006版本）
2.  编译产物路径：`D:\project\ies\cad\exploration\Debug2005\KsExploration.arx`
3.  输出目录下已自动同步全部资源文件（块库、填充图案、菜单、支持文件），无需额外手动复制。

## 二、加载ARX步骤
1.  打开AutoCAD，新建或打开空白dwg图纸
2.  在命令行输入 `APPLOAD` 回车，弹出加载/卸载自定义程序窗口
3.  浏览到 `D:\project\ies\cad\exploration\Debug2005\KsExploration.arx`，选中后点击「加载」
4.  加载成功后命令行无报错，提示已加载模块即可；可将该arx添加到「启动组」，实现AutoCAD启动时自动加载。
    (arxload "D:/project/ies/cad/exploration/Debug/KsExploration.arx")
## 三、启动7100端口HTTP服务
ARX加载完成后，在AutoCAD命令行输入：
```
SLINIT
```
回车后服务自动在本地7100端口启动，命令行将输出服务启动成功提示。
> 注意：服务启动后AutoCAD进程需保持打开，关闭AutoCAD后HTTP服务将自动停止。

## 四、功能验证
服务启动后可使用直连测试脚本验证三个出图接口：
1.  脚本路径：`D:\project\ies\cad_server\test_7100_direct.py`
2.  Python虚拟环境：`D:\project\ies\.workbuddy\qa\.venv\Scripts\python.exe`
3.  常用测试命令示例：
    ```bash
    # 测试平面布置图接口
    .\.workbuddy\qa\.venv\Scripts\python.exe cad_server\test_7100_direct.py --view plan --hole ZK1
    # 测试钻孔柱状图接口
    .\.workbuddy\qa\.venv\Scripts\python.exe cad_server\test_7100_direct.py --view column --hole ZK1
    # 测试钻孔剖面图接口
    .\.workbuddy\qa\.venv\Scripts\python.exe cad_server\test_7100_direct.py --view section --hole ZK1
    ```
4.  三个业务接口路由：
    - `POST http://127.0.0.1:7100/ai/feature/create_plan_view` 平面布置图
    - `POST http://127.0.0.1:7100/create_column_view` 钻孔柱状图
    - `POST http://127.0.0.1:7100/create_section_view` 钻孔剖面图

## 五、落盘与日志位置
ARX端运行过程中文件自动写入AutoCAD当前工作目录下的`project`文件夹：
1.  接收的钻孔编录数据：`project/钻探编录表.json`
2.  双向请求/响应日志：`project/7100.log`，记录所有接口入参、出参、坐标交换、出图执行的全流程日志，用于问题排查。

## 六、坐标交换规则
接口入参坐标约定：
- 入参`x`为测量坐标系北方向坐标、`y`为测量坐标系东方向坐标
- ARX内部自动交换为AutoCAD绘图坐标：CAD坐标系X对应东方向、Y对应北方向，无需调用方手动转换。
- 无效坐标统一使用 `-999999.0` 标记，内部通过`IsInvalid`宏判断，不依赖系统NAN/isnan逻辑，兼容老版本CRT。
