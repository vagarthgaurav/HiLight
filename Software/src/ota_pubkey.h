#pragma once

// ECDSA P-256 public key that OTA firmware images must be signed with.
// The matching private key lives outside the repo (~/.config/hilight/ota_signing_key.pem);
// see `just sign`. Replacing this key requires a USB flash of every lamp.
static const char OTA_PUBLIC_KEY[] = R"(-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEb7bwu1qbFdG+tnDKjNY2i9Gf1ZJk
WE7HWhwrmhoGF4n4zLuKDOCElnHPF+bvdYpdObD95pWEd2Wrn7vRi9ruSg==
-----END PUBLIC KEY-----
)";
