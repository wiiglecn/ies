import requests, json

url = "http://127.0.0.1:7100/ai/feature/create_plan_view"
# Read all existing borehole data from the saved JSON file
with open("D:/project/ies/cad/exploration/Debug/project/钻探编录表.json", "r", encoding="utf-8") as f:
    borehole_data = json.load(f)

payload = {
    "borehole_data": borehole_data,
    "has_base_map": False,
    "auto_zoom": True
}
resp = requests.post(url, json=payload, timeout=60)
print("Response:", resp.status_code, resp.text)
