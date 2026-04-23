#!/bin/sh

set -eu

IDENTITY_NAME="${1:-InputLeap Local Codesign}"
KEYCHAIN="${HOME}/Library/Keychains/login.keychain-db"
OPENSSL_BIN="${OPENSSL_BIN:-/opt/homebrew/bin/openssl}"

if [ ! -x "$OPENSSL_BIN" ]; then
    echo "openssl not found at $OPENSSL_BIN" >&2
    exit 1
fi

if security find-certificate -a -c "$IDENTITY_NAME" "$KEYCHAIN" >/dev/null 2>&1; then
    echo "codesign certificate already exists: $IDENTITY_NAME"
    exit 0
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
PASS="InputLeapLocalCodesign"

cat > "$TMPDIR/openssl.cnf" <<EOF
[ req ]
distinguished_name = dn
x509_extensions = v3_req
prompt = no

[ dn ]
CN = ${IDENTITY_NAME}
O = Local Development
OU = Codex

[ v3_req ]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature
extendedKeyUsage = critical,codeSigning
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer
EOF

"$OPENSSL_BIN" req \
    -new \
    -newkey rsa:2048 \
    -nodes \
    -keyout "$TMPDIR/key.pem" \
    -x509 \
    -days 3650 \
    -out "$TMPDIR/cert.pem" \
    -config "$TMPDIR/openssl.cnf" >/dev/null 2>&1

"$OPENSSL_BIN" pkcs12 \
    -export \
    -out "$TMPDIR/inputleap-local-codesign.p12" \
    -inkey "$TMPDIR/key.pem" \
    -in "$TMPDIR/cert.pem" \
    -passout "pass:${PASS}" >/dev/null 2>&1

security import "$TMPDIR/inputleap-local-codesign.p12" \
    -k "$KEYCHAIN" \
    -f pkcs12 \
    -P "$PASS" \
    -A \
    -T /usr/bin/codesign \
    -T /usr/bin/security >/dev/null

echo "installed codesign certificate: $IDENTITY_NAME"
