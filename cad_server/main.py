import requests
from flask import Flask, request, Response

WORK_URL = "https://ws-93xgld532jdyrkjf.cn-beijing.maas.aliyuncs.com/compatible-mode/v1/chat/completions"
API_KEY = "sk-820adfc78edd47608679411d492c0c20"  # 建议从环境变量或配置文件中读取
CHECK_KEY = "sl-123456"

app = Flask(__name__)

@app.route("/", defaults={"path": ""}, methods=["POST"])
@app.route("/<path:path>", methods=["POST"])
def proxy(path):


    # 2. 校验客户端提交的 Authorization
    client_auth = request.headers.get("Authorization", "")
    if client_auth != f"Bearer {CHECK_KEY}":
        return Response("Unauthorized: Invalid or missing Authorization header.", status=401)

    # 临时返回测试字符串
    # return "This is a temporary test response."

    # Build target URL
    target_url = WORK_URL

    # Forward headers (exclude Host header)
    headers = {k: v for k, v in request.headers if k.lower() != "host"}
    # 额外添加或覆盖 Authorization 和 Content-Type
    headers["Authorization"] = f"Bearer {API_KEY}"
    headers["Content-Type"] = "application/json"

    try:
        # Forward query params and body
        if request.method == "GET":
            resp = requests.get(
                target_url,
                headers=headers,
                params=request.args,
                stream=True,
                timeout=90,  # 设置60秒超时
            )
        else:
            resp = requests.post(
                target_url,
                headers=headers,
                params=request.args,
                data=request.get_data(),
                stream=True,
                timeout=90,  # 设置60秒超时
            )
    except requests.exceptions.Timeout:
        # 同步超时：立即返回 504 给客户端
        return Response("Gateway Timeout: The upstream server took too long to respond.", status=504)
    except requests.exceptions.RequestException as e:
        # 同步出错：立即返回 502 给客户端
        return Response(f"Bad Gateway: {str(e)}", status=502)

    # Return response with status, headers, and body
    excluded_headers = ["content-encoding", "content-length", "transfer-encoding", "connection"]
    response_headers = [(k, v) for k, v in resp.headers.items() if k.lower() not in excluded_headers]

    return Response(resp.content, status=resp.status_code, headers=response_headers)


if __name__ == "__main__":
    # HTTPS using a self-signed certificate
    # Generate certs with:
    #   openssl req -x509 -newkey rsa:4096 -nodes -keyout key.pem -out cert.pem -days 365 -subj "/CN=localhost"
    # app.run(host="0.0.0.0", port=7101, ssl_context=("cert.pem", "key.pem"))
    app.run(host="0.0.0.0", port=7101, ssl_context=("kskjai.com.pem", "kskjai.com.key"))