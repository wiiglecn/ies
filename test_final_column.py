# -*- coding: utf-8 -*-
import json, requests, sys, os
sys.path.insert(0, 'cad_server')
from draw_gateway import _normalize_borehole

# 读原始数据
with open('cad_client/阳光幸福里-钻探编录表.json','r',encoding='utf-8') as f:
    rows = json.load(f)
norm = _normalize_borehole(rows)

# 手工构造业务代码完全匹配的中文格式JSON
result = {}
for hole_no in ['ZK1', 'ZK2']:
    en = norm[hole_no]
    cn_layers = []
    for layer in en['layers']:
        cn_layer = {
            '地层编号': layer['layer_id'],
            '进底起(m)': layer['from'],
            '到底深度(m)': layer['to'],
            '地层名称': layer['name'],
            '颜色': layer['color'],
            '状态': layer['state'],
            '密实度': layer['density'],
            '湿度': layer['moisture'],
            '描述内容': layer['description'],
        }
        if 'sample' in layer:
            cn_layer['取样(土、水)样'] = {
                '编号': layer['sample'].get('no',''),
                '取样深度': layer['sample'].get('depth','')
            }
        if 'test' in layer:
            cn_layer['原位测试'] = {
                '类型': layer['test'].get('type',''),
                '起(m)': layer['test'].get('from',''),
                '止(m)': layer['test'].get('to',''),
                '杆长(m)': layer['test'].get('rod_length',''),
                '锤击数(击)': layer['test'].get('hits','')
            }
        cn_layers.append(cn_layer)
    result[hole_no] = {
        '钻孔编号': hole_no,
        'X坐标(m)': en['y'],
        'Y坐标(m)': en['x'],
        '孔口高程(m)': en['elevation'],
        '终孔深度(m)': en['depth'],
        '钻孔类型': en['hole_type'],
        '工程名称': en['project_name'],
        '工程地点': en['site'],
        '开孔日期': en['date_start'],
        '终孔日期': en['date_end'],
        '初见水位(m)': '',
        '稳定水位(m)': '',
        '地层列表': cn_layers
    }

payload = {'borehole_data': result}
print('Sending pre-structured CN-format JSON for holes:', list(result.keys()))
resp = requests.post('http://localhost:7100/ai/feature/create_column_view', json=payload, timeout=120)
print('Response:', resp.status_code, resp.text)
