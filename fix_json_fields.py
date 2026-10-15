import json
p = r'D:\project\ies\cad\exploration\Debug\project\钻探编录表.json'
with open(p,'r',encoding='utf-8') as f:
    data = json.load(f)

for hole_no, hole in data.items():
    if '地层列表' not in hole:
        continue
    for layer in hole['地层列表']:
        # 业务代码实际读取的字段名
        new_layer = {}
        new_layer['地层编号'] = layer.get('layer_id','')
        new_layer['进底起(m)'] = layer.get('from',0)
        new_layer['到底深度(m)'] = layer.get('to',0)
        new_layer['地层名'] = layer.get('name','')
        new_layer['颜色'] = layer.get('color','')
        new_layer['状态'] = layer.get('state','')
        new_layer['密实度'] = layer.get('density','')
        new_layer['湿度'] = layer.get('moisture','')
        new_layer['描述内容'] = layer.get('description','')
        if 'sample' in layer:
            new_layer['取样(土、水)样'] = {
                '编号': layer['sample'].get('no',''),
                '取样深度': layer['sample'].get('depth','')
            }
        if 'test' in layer:
            new_layer['原位测试'] = {
                '类型': layer['test'].get('type',''),
                '起(m)': layer['test'].get('from',''),
                '止(m)': layer['test'].get('to',''),
                '杆长(m)': layer['test'].get('rod_length',''),
                '锤击数(击)': layer['test'].get('hits','')
            }
        # 替换原来的layer
        layer.clear()
        layer.update(new_layer)

with open(p,'w',encoding='utf-8') as f:
    json.dump(data, f, ensure_ascii=False, indent=2)
print('Fixed all layer field names to match C++ code expectations')
print('ZK1 layer 0 keys:', list(data['ZK1']['地层列表'][0].keys()))
