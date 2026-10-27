## 一、cad数据配置

![image-20260918145303144](./assets/image-20260918145303144.png)

![image-20260918145333416](./assets/image-20260918145333416.png)



路径结构如上，项目的数据转成json，保存在project目录下即可，插件会自动取目录下的数据。

## 二、钻探编录表转换方式

1、分成行和列的索引即可，例如：行是1，列是B，那么就代表：第0行，第1列，(Excel是从1行开始，列是从A开始， 对应JSON都是0行，0列开始)

2、Excel底部的“钻孔名称"为键key。

转换后的样式如下：

![image-20260918145940375](./assets/image-20260918145940375.png)

## 三、调用接口

插件会在本地开启：http://localhost:7100 端口

以下请求默认为POST方式

1、平面图

```
无
```

2、剖面图

```
无
```

3、柱状图

```
/ai/feature/create_column_view
请求体：“{}”
```

4、打开 CAD 图纸

```
/ai/tool/open_document
请求体：{"path":"D:/project","fileName":"plan.dwg"}
或：    {"filePath":"D:/project/plan.dwg"}
返回：  {"status":"success","data":{"filePath":"D:\\project\\plan.dwg","fileName":"plan.dwg","alreadyOpen":false,"documentCount":2}}
```

> 说明：仅支持 `.dwg` / `.dxf`；需在 AutoCAD 空闲（无命令执行、无模态对话框）时调用，否则返回错误提示稍后重试；若该图纸已打开则直接激活；打开过程中弹出的对话框（缺少字体、外部参照等）需人工处理。

