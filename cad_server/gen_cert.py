import datetime
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa

# 生成私钥
key = rsa.generate_private_key(
    public_exponent=65537,
    key_size=2048,
)

# 设置证书主题和颁发者（自签名的情况下两者相同）
subject = issuer = x509.Name([
    x509.NameAttribute(NameOID.COMMON_NAME, u"localhost"),
])

# 构建自签名证书
cert = (
    x509.CertificateBuilder()
    .subject_name(subject)
    .issuer_name(issuer)
    .public_key(key.public_key())
    .serial_number(x509.random_serial_number())
    .not_valid_before(datetime.datetime.utcnow())
    .not_valid_after(datetime.datetime.utcnow() + datetime.timedelta(days=365))
    .add_extension(
        x509.SubjectAlternativeName([x509.DNSName(u"localhost")]),
        critical=False,
    )
    .sign(key, hashes.SHA256())
)

# 将私钥写入 key.pem
with open("key.pem", "wb") as f:
    f.write(
        key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption(),
        )
    )

# 将证书写入 cert.pem
with open("cert.pem", "wb") as f:
    f.write(cert.public_bytes(serialization.Encoding.PEM))

print("成功生成 cert.pem 和 key.pem")