#!/bin/bash
# Run from project root:  bash fix_devtools.sh
FILE="core/tabs/DevToolsPanel.cpp"
[ ! -f "$FILE" ] && echo "ERROR: $FILE not found" && exit 1

# Fix 1: include path
sed -i 's|#include "WASignalDecryptor.h"|#include "../engine/WASignalDecryptor.h"|g' "$FILE"

# Fix 2: loadFromIDBDump → loadFromDump
sed -i 's/m_decryptor\.loadFromIDBDump(/m_decryptor.loadFromDump(/g' "$FILE"

# Fix 3 & 4: .store. not a member
sed -i 's/m_decryptor\.store\.senderKeys\.size()/m_decryptor.senderKeys.size()/g' "$FILE"
sed -i 's/m_decryptor\.store\.sessions\.size()/m_decryptor.sessions.size()/g' "$FILE"

# Fix 5: decryptEnc → decrypt
sed -i 's/m_decryptor\.decryptEnc(/m_decryptor.decrypt(/g' "$FILE"

# Fix 6: decrypt() returns QString not QByteArray — fix the call site with python
python3 - "$FILE" << 'PYEOF'
import sys
path = sys.argv[1]
src = open(path).read()

# These are the exact broken lines — replace them
src = src.replace(
    'QByteArray plaintext = m_decryptor.decrypt(',
    'bool decOk = false;\n        QString decResult = m_decryptor.decrypt(')

src = src.replace(
    'fromJid, groupJid, status);',
    'fromJid, groupJid, decOk);')

src = src.replace(
    'result += "Status: " + status + "\\n";',
    'result += QString("Status: %1\\n").arg(decOk ? "OK" : "failed");')

src = src.replace(
    'if (!plaintext.isEmpty()) {',
    'if (decOk) {')

src = src.replace(
    '            result += "\\n── Plaintext ────────────────────────────────\\n";\n'
    '            result += WASignalDecryptor::plaintextToString(plaintext);\n'
    '            result += "\\n── Raw bytes (" + QString::number(plaintext.size()) + "b) ──\\n";\n'
    '            result += QString::fromLatin1(plaintext.toHex());\n',
    '            result += "\\n── Plaintext ────────────────────────────────\\n";\n'
    '            result += decResult;\n')

# Also remove the now-unused QString status declaration
src = src.replace('        QString status;\n', '')

open(path, 'w').write(src)
print("Python fixes applied")
PYEOF

echo "All done — run: cd build && make -j\$(nproc)"