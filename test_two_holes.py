# -*- coding: utf-8 -*-
import json, sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'cad_server'))
from draw_gateway import _normalize_borehole
import requests

with open('cad_client/阳光幸福里-钻探编录表.json','r',encoding='utf-8') as f:
    rows = json.load(f)
norm = _normalize_borehole(rows)
# 只取ZK1和ZK2两个孔
two_holes = {'ZK1': norm['ZK1'], 'ZK2': norm['ZK2']}
# X/Y交换
for h in two_holes.values():
    x, y = h['x'], h['y']
    h['x'], h['y'] = y, x

payload = {'borehole_data': two_holes}
print('Sending request with holes:', list(two_holes.keys()))
resp = requests.post('http://localhost:7100/ai/feature/create_column_view', json=payload, timeout=120)
print('Response status:', resp.status_code)
print('Response body:', resp.text)
