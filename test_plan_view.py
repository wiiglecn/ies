import requests, json

url = "http://127.0.0.1:7100/ai/feature/create_plan_view"
payload = {
    "borehole_data": {
        "ZK1": {
            "钻孔编号": "ZK1",
            "孔口高程(m)": 138.27,
            "终孔深度(m)": 7.7,
            "X坐标(m)": 2747346.764,
            "Y坐标(m)": 36524930.71
        },
        "ZK2": {
            "钻孔编号": "ZK2",
            "孔口高程(m)": 139.12,
            "终孔深度(m)": 8.2,
            "X坐标(m)": 2747375.120,
            "Y坐标(m)": 36524958.33
        }
    },
    "has_base_map": True,
    "base_map_path": "D:/project/ies/cad/exploration/project/05阳光幸福里总平面规划图104(1).dwg",
    "auto_zoom": True
}
resp = requests.post(url, json=payload, timeout=30)
print("Response:", resp.status_code, resp.text)
