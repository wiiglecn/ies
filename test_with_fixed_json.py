import json, requests
p = r'D:\project\ies\cad\exploration\Debug\project\钻探编录表.json'
with open(p,'r',encoding='utf-8') as f:
    fixed_data = json.load(f)
# 直接发已经修好字段名的两个孔数据
payload = {'borehole_data': fixed_data}
print('Sending pre-fixed JSON with correct CN field names...')
resp = requests.post('http://localhost:7100/ai/feature/create_column_view', json=payload, timeout=120)
print('Status:', resp.status_code, resp.text)
